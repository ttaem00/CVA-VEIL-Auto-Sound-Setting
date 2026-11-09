// TRUEITEM modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
// Compile output-monitor.cpp with output-monitor-stubs first on the include
// path and link the REAL libebur128. Exercises public callback behavior; does
// not replace libebur128 or assert against the implementation's own formula.
#include "output-monitor.h"
#include <obs.h>

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

struct audio_output {};
struct obs_source { float volume{1.0f}; bool muted{false}; };
static audio_output fake_audio;
static obs_source fake_source;
static audio_output_callback_t output_callback = nullptr;
static capture_callback_t capture_callback = nullptr;
static void *callback_param = nullptr;
static size_t selected_track = 99;

audio_t *obs_get_audio() { return &fake_audio; }
uint32_t audio_output_get_sample_rate(const audio_t *) { return 48000; }
size_t audio_output_get_channels(const audio_t *) { return 2; }
bool audio_output_connect(audio_t *, size_t track, const audio_convert_info *conversion,
			  audio_output_callback_t callback, void *param)
{
	if (conversion->speakers != SPEAKERS_STEREO || conversion->samples_per_sec != 48000 || !conversion->allow_clipping)
		return false;
	output_callback = callback;
	callback_param = param;
	selected_track = track;
	return true;
}
void audio_output_disconnect(audio_t *, size_t, audio_output_callback_t, void *) { output_callback = nullptr; }
obs_source_t *obs_get_source_by_name(const char *name) { return std::string(name) == "fixture" ? &fake_source : nullptr; }
uint32_t obs_source_get_output_flags(const obs_source_t *) { return OBS_SOURCE_AUDIO; }
void obs_source_release(obs_source_t *) {}
void obs_source_add_audio_capture_callback(obs_source_t *, capture_callback_t callback, void *param)
{
	capture_callback = callback;
	callback_param = param;
}
void obs_source_remove_audio_capture_callback(obs_source_t *, capture_callback_t, void *) { capture_callback = nullptr; }
float obs_source_get_volume(const obs_source_t *source) { return source->volume; }
bool obs_source_muted(const obs_source_t *source) { return source->muted; }

static void require(bool pass, const char *message)
{
	if (!pass)
		throw std::runtime_error(message);
}

static void feed_tone(double seconds)
{
	constexpr uint32_t frames = 480;
	std::array<float, frames> left{}, right{};
	const size_t blocks = static_cast<size_t>(seconds * 100);
	for (size_t block = 0; block < blocks; ++block) {
		for (uint32_t frame = 0; frame < frames; ++frame) {
			const double position = double(block * frames + frame) / 48000;
			left[frame] = float(0.1 * std::sin(position * 2 * 3.141592653589793 * 1000));
			right[frame] = -left[frame]; // Deliberate anti-phase: mono averaging would erase it.
		}
		audio_data audio{};
		audio.frames = frames;
		audio.data[0] = reinterpret_cast<uint8_t *>(left.data());
		audio.data[1] = reinterpret_cast<uint8_t *>(right.data());
		if (output_callback)
			output_callback(callback_param, selected_track, &audio);
		else if (capture_callback)
			capture_callback(callback_param, &fake_source, &audio, false);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

int main()
{
	try {
		lbm::StereoMonitor monitor;
		require(monitor.connect_output(1), "Output connection failed");
		require(selected_track == 1, "Track 2 must use zero-based mixer 1");
		feed_tone(4.0);
		auto output = monitor.snapshot();
		require(output.ready && output.signal_present, "Anti-phase stereo incorrectly considered silent");
		require(output.dropped_blocks == 0, "Fixture overloaded worker queue");
		require(output.short_lufs > -21.0 && output.short_lufs < -19.0, "Stereo 1kHz reference loudness outside expected tolerance");
		require(output.integrated_lufs > -21.0 && output.integrated_lufs < -19.0, "Integrated loudness outside expected tolerance");
		require(std::abs(output.max_peak_dbfs + 20.0) < .02, "Sample peak should preserve both stereo channels");
		monitor.reset();
		auto reset = monitor.snapshot();
		require(!reset.ready && !std::isfinite(reset.max_peak_dbfs), "Reset exposed stale previous measurement");
		feed_tone(1.0);
		require(!monitor.snapshot().ready, "Fresh measurement needs three seconds of audio");
		monitor.disconnect();
		require(!monitor.snapshot().connected && output_callback == nullptr, "Disconnect left callback attached");

		fake_source.volume = .5f;
		require(monitor.connect_source("fixture"), "Source connection failed");
		feed_tone(4.0);
		auto source = monitor.snapshot();
		require(source.ready, "Source meter did not become ready");
		require(std::abs(source.max_peak_dbfs - output.max_peak_dbfs + 6.0206) < .02, "Source fader half should reduce level by 6.02dB");
		monitor.reset();
		fake_source.muted = true;
		feed_tone(4.0);
		auto muted = monitor.snapshot();
		require(!muted.signal_present && !std::isfinite(muted.max_peak_dbfs), "Muted source leaked signal to measurement");
		monitor.disconnect();
		require(capture_callback == nullptr, "Source callback still attached");
		std::cout << "PASS: stereo anti-phase, track selection, integrated/short-term reference, fader, mute, reset, warmup, disconnect\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
