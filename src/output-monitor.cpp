// TRUEITEM modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#include "output-monitor.h"
#include "spsc-queue.h"

#include <obs.h>
#include <media-io/audio-io.h>
#include <ebur128.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

namespace lbm {
namespace {
constexpr uint32_t kFramesPerPacket = 4096;
constexpr size_t kPacketCapacity = 64;
constexpr double kSilentRmsDbfs = -80.0;
using Clock = std::chrono::steady_clock;

double amplitude_db(double amplitude)
{
	return amplitude > 0.0 ? 20.0 * std::log10(amplitude) : -std::numeric_limits<double>::infinity();
}

struct StereoPacket {
	std::array<float, kFramesPerPacket * 2> pcm{};
	uint32_t frames{0};
	uint64_t generation{0};
	Clock::time_point captured_at{};
};
} // namespace

struct StereoMonitor::Impl {
	SPSCQueue<StereoPacket, kPacketCapacity> queue;
	std::atomic<bool> running{false};
	std::atomic<bool> accepting{false};
	std::atomic<bool> connected{false};
	std::atomic<uint64_t> requested_generation{0};
	std::atomic<uint64_t> dropped{0};
	std::thread worker;
	mutable std::mutex snapshot_mutex;
	StereoMeterSnapshot result;
	Clock::time_point last_processed{};
	std::atomic<uint32_t> rate{0}; // Fixed for each connected worker's lifetime.
	audio_t *output{nullptr};
	size_t track{0};
	obs_source_t *source{nullptr};

	~Impl() { disconnect(); }

	void start_worker(uint32_t sample_rate)
	{
		rate.store(sample_rate, std::memory_order_release);
		requested_generation.fetch_add(1, std::memory_order_acq_rel);
		dropped.store(0, std::memory_order_relaxed);
		{
			std::lock_guard<std::mutex> lock(snapshot_mutex);
			result = {};
			result.sample_rate = sample_rate;
			last_processed = {};
		}
		queue.clear(); // No producer or previous worker exists at this point.
		running.store(true, std::memory_order_release);
		worker = std::thread(&Impl::work, this);
		accepting.store(true, std::memory_order_release);
	}

	void disconnect()
	{
		accepting.store(false, std::memory_order_release);
		// OBS disconnect/remove takes the callback mutex, waiting for any callback
		// already in progress. Never hold our snapshot mutex while calling OBS.
		if (output) {
			audio_output_disconnect(output, track, output_callback, this);
			output = nullptr;
		}
		if (source) {
			obs_source_remove_audio_capture_callback(source, source_callback, this);
			obs_source_release(source);
			source = nullptr;
		}
		connected.store(false, std::memory_order_release);
		running.store(false, std::memory_order_release);
		if (worker.joinable())
			worker.join();
		queue.clear();
		requested_generation.fetch_add(1, std::memory_order_acq_rel);
		std::lock_guard<std::mutex> lock(snapshot_mutex);
		result = {};
		last_processed = {};
	}

	bool connect_output(size_t index)
	{
		disconnect();
		if (index >= MAX_AUDIO_MIXES)
			return false;
		audio_t *audio = obs_get_audio();
		if (!audio)
			return false;
		const auto sample_rate = audio_output_get_sample_rate(audio);
		if (!sample_rate)
			return false;
		start_worker(sample_rate);
		audio_convert_info conversion{};
		conversion.samples_per_sec = sample_rate;
		conversion.format = AUDIO_FORMAT_FLOAT_PLANAR;
		conversion.speakers = SPEAKERS_STEREO;
		conversion.allow_clipping = true;
		if (!audio_output_connect(audio, index, &conversion, output_callback, this)) {
			disconnect();
			return false;
		}
		output = audio;
		track = index;
		connected.store(true, std::memory_order_release);
		return true;
	}

	bool connect_source(const std::string &name)
	{
		disconnect();
		if (name.empty())
			return false;
		audio_t *audio = obs_get_audio();
		if (!audio)
			return false;
		const auto sample_rate = audio_output_get_sample_rate(audio);
		obs_source_t *selected = obs_get_source_by_name(name.c_str());
		if (!selected)
			return false;
		if (!sample_rate || !(obs_source_get_output_flags(selected) & OBS_SOURCE_AUDIO)) {
			obs_source_release(selected);
			return false;
		}
		// Captured source audio uses the configured OBS format. For a surround
		// project, pretending its first two channels are a stereo mix is wrong.
		// Refuse it rather than silently report an incomplete measurement.
		if (audio_output_get_channels(audio) > 2) {
			obs_source_release(selected);
			return false;
		}
		start_worker(sample_rate);
		source = selected;
		obs_source_add_audio_capture_callback(source, source_callback, this);
		connected.store(true, std::memory_order_release);
		return true;
	}

	static void output_callback(void *parameter, size_t, audio_data *audio)
	{
		auto *self = static_cast<Impl *>(parameter);
		if (self && audio)
			self->enqueue(audio, 1.0f, false);
	}

	static void source_callback(void *parameter, obs_source_t *captured_source, const audio_data *audio, bool muted)
	{
		auto *self = static_cast<Impl *>(parameter);
		if (self && audio && captured_source)
			self->enqueue(audio, obs_source_get_volume(captured_source),
				      muted || obs_source_muted(captured_source));
	}

	void enqueue(const audio_data *audio, float gain, bool muted)
	{
		if (!accepting.load(std::memory_order_acquire) || !audio->data[0] || !audio->frames)
			return;
		const auto *left = reinterpret_cast<const float *>(audio->data[0]);
		const auto *right = audio->data[1] ? reinterpret_cast<const float *>(audio->data[1]) : left;
		const auto generation = requested_generation.load(std::memory_order_acquire);
		const auto captured_at = Clock::now();
		const float applied_gain = muted || !std::isfinite(gain) ? 0.0f : gain;
		// Fixed-capacity packet storage, including for unusually large callbacks.
		// No heap allocation, libebur128 call, mutex, or file IO on the audio thread.
		for (uint32_t offset = 0; offset < audio->frames; offset += kFramesPerPacket) {
			StereoPacket packet;
			packet.frames = std::min(kFramesPerPacket, audio->frames - offset);
			packet.generation = generation;
			packet.captured_at = captured_at;
			for (uint32_t frame = 0; frame < packet.frames; ++frame) {
				const float l = left[offset + frame] * applied_gain;
				const float r = right[offset + frame] * applied_gain;
				packet.pcm[2 * frame] = std::isfinite(l) ? l : 0.0f;
				packet.pcm[2 * frame + 1] = std::isfinite(r) ? r : 0.0f;
			}
			if (!queue.try_push(packet))
				dropped.fetch_add(1, std::memory_order_relaxed);
		}
	}

	void work()
	{
		const auto sample_rate = rate.load(std::memory_order_acquire);
		ebur128_state *state = nullptr;
		uint64_t generation = std::numeric_limits<uint64_t>::max();
		uint64_t measured_frames = 0;
		double maximum_peak = 0.0;
		StereoPacket packet;
		while (running.load(std::memory_order_acquire)) {
			const uint64_t requested = requested_generation.load(std::memory_order_acquire);
			if (generation != requested) {
				if (state)
					ebur128_destroy(&state);
				state = ebur128_init(2, sample_rate, EBUR128_MODE_S | EBUR128_MODE_I);
				generation = requested;
				measured_frames = 0;
				maximum_peak = 0.0;
				std::lock_guard<std::mutex> lock(snapshot_mutex);
				result = {};
				result.generation = generation;
				result.sample_rate = sample_rate;
				last_processed = {};
			}
			if (!queue.try_pop(packet)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(3));
				continue;
			}
			if (packet.generation != generation || !state)
				continue;
			if (ebur128_add_frames_float(state, packet.pcm.data(), packet.frames) != EBUR128_SUCCESS)
				continue;
			measured_frames += packet.frames;
			double peak = 0.0;
			double square_sum = 0.0;
			for (size_t sample = 0; sample < size_t(packet.frames) * 2; ++sample) {
				const double value = packet.pcm[sample];
				peak = std::max(peak, std::abs(value));
				square_sum += value * value;
			}
			maximum_peak = std::max(maximum_peak, peak);
			double short_lufs = -std::numeric_limits<double>::infinity();
			double integrated_lufs = -std::numeric_limits<double>::infinity();
			if (ebur128_loudness_shortterm(state, &short_lufs) != EBUR128_SUCCESS)
				short_lufs = -std::numeric_limits<double>::infinity();
			if (ebur128_loudness_global(state, &integrated_lufs) != EBUR128_SUCCESS)
				integrated_lufs = -std::numeric_limits<double>::infinity();
			StereoMeterSnapshot next;
			next.sample_rate = sample_rate;
			next.generation = generation;
			next.seconds = double(measured_frames) / sample_rate;
			next.short_lufs = short_lufs;
			next.integrated_lufs = integrated_lufs;
			next.peak_dbfs = amplitude_db(peak);
			next.max_peak_dbfs = amplitude_db(maximum_peak);
			next.rms_dbfs = amplitude_db(std::sqrt(square_sum / (double(packet.frames) * 2)));
			next.signal_present = next.rms_dbfs > kSilentRmsDbfs;
			next.ready = next.seconds >= 3.0 && std::isfinite(short_lufs);
			{
				std::lock_guard<std::mutex> lock(snapshot_mutex);
				result = next;
				last_processed = packet.captured_at;
			}
		}
		if (state)
			ebur128_destroy(&state);
	}

	void reset() { requested_generation.fetch_add(1, std::memory_order_acq_rel); }

	StereoMeterSnapshot snapshot() const
	{
		std::lock_guard<std::mutex> lock(snapshot_mutex);
		StereoMeterSnapshot value = result;
		value.connected = connected.load(std::memory_order_acquire);
		value.dropped_blocks = dropped.load(std::memory_order_relaxed);
		const auto generation = requested_generation.load(std::memory_order_acquire);
		if (value.generation != generation) {
			value = {};
			value.connected = connected.load(std::memory_order_acquire);
			value.sample_rate = rate.load(std::memory_order_acquire);
			value.generation = generation;
			value.dropped_blocks = dropped.load(std::memory_order_relaxed);
			return value;
		}
		if (last_processed != Clock::time_point{})
			value.age_seconds = std::chrono::duration<double>(Clock::now() - last_processed).count();
		if (value.age_seconds > 1.0 || !value.connected) {
			value.ready = false;
			value.signal_present = false;
		}
		return value;
	}
};

StereoMonitor::StereoMonitor() : impl_(std::make_unique<Impl>()) {}
StereoMonitor::~StereoMonitor() = default;
bool StereoMonitor::connect_output(size_t track) { return impl_->connect_output(track); }
bool StereoMonitor::connect_source(const std::string &name) { return impl_->connect_source(name); }
void StereoMonitor::disconnect() { impl_->disconnect(); }
void StereoMonitor::reset() { impl_->reset(); }
StereoMeterSnapshot StereoMonitor::snapshot() const { return impl_->snapshot(); }

} // namespace lbm
