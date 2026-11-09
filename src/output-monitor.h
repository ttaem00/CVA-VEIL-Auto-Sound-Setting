// TRUEITEM modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace lbm {

struct StereoMeterSnapshot {
	bool connected{false};
	bool ready{false}; // At least three seconds of fresh, finite short-term loudness.
	bool signal_present{false};
	uint32_t sample_rate{0};
	uint64_t generation{0};
	uint64_t dropped_blocks{0};
	double seconds{0.0}; // Audio frames measured since reset, including silence.
	double age_seconds{std::numeric_limits<double>::infinity()};
	double short_lufs{-std::numeric_limits<double>::infinity()};
	double integrated_lufs{-std::numeric_limits<double>::infinity()};
	double peak_dbfs{-std::numeric_limits<double>::infinity()}; // Most recent block.
	double max_peak_dbfs{-std::numeric_limits<double>::infinity()}; // Since reset.
	double rms_dbfs{-std::numeric_limits<double>::infinity()}; // Most recent block.
};

// A stereo-preserving, read-only meter. Use one instance for each output track
// or source. Control methods belong to the UI thread; snapshot/reset are safe
// during capture. No recorded PCM is retained outside a bounded worker queue.
// Disconnect before OBS replaces its audio output (e.g. profile changing), and
// release source connections before scene-collection cleanup.
class StereoMonitor {
public:
	StereoMonitor();
	~StereoMonitor();
	StereoMonitor(const StereoMonitor &) = delete;
	StereoMonitor &operator=(const StereoMonitor &) = delete;

	// Zero-based OBS track: 0 is track 1, 1 is track 2. Captures actual mixed
	// stereo PCM before encoding; unclamped values expose clipping risk.
	bool connect_output(size_t zero_based_track);
	// Source meter reflects that source's fader and mute. It does not represent
	// scene activity, track routing, sync offsets, or final multi-source mixing.
	// Mono input is duplicated; stereo input is never averaged to mono.
	bool connect_source(const std::string &source_name);
	void disconnect();
	void reset();
	StereoMeterSnapshot snapshot() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace lbm
