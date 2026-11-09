// TRUEITEM modification, 2026-11-09: bounded calibration recommendations.
// Distributed under GPL-2.0; see the repository LICENSE.
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace lbm {

// Caller supplies measurements of the same representative interval. speech_lufs
// is measured only while the microphone has validated speech; it is not the LUFS
// of an entire interval containing silence. No OBS API or audio mutation occurs
// in this policy layer. LUFS references below are starting points, not platform
// delivery specifications or a guarantee of intelligibility.
struct CalibrationSource {
	bool captured{false};
	double speech_lufs{std::numeric_limits<double>::quiet_NaN()};
	double peak_dbfs{std::numeric_limits<double>::quiet_NaN()};
	double speech_seconds{0.0};
	double quiet_lufs{std::numeric_limits<double>::quiet_NaN()};
	double quiet_seconds{0.0};
};

struct CalibrationInput {
	double elapsed_seconds{0.0};
	// Set only after the user has tested ordinary speech/game/music levels.
	bool context_confirmed{false};
	// Recommendations may be displayed while live; application requires idle.
	bool outputs_idle{false};
	// Maximum sample peak observed across the broadcast/recording output
	// tracks during this same test. Missing evidence must not permit a boost.
	double output_peak_dbfs{std::numeric_limits<double>::quiet_NaN()};
	CalibrationSource voice;
	CalibrationSource game;
	CalibrationSource music;
};

struct CalibrationConfig {
	double voice_target_lufs{-20.0};
	double game_gap_lu{6.0};
	double music_gap_lu{12.0};
	double minimum_total_seconds{20.0};
	double minimum_speech_seconds{5.0};
	double minimum_quiet_seconds{2.0};
	double minimum_voice_to_noise_lu{10.0};
	double minimum_voice_lufs{-45.0};
	double voice_max_change_db{3.0};
	double background_max_cut_db{6.0};
	double peak_ceiling_dbfs{-3.0};
	double deadband_db{1.0};
};

enum class CalibrationReason {
	Ready,
	NoChangeNeeded,
	InvalidConfiguration,
	InvalidDuration,
	InsufficientDuration,
	MissingCapture,
	InvalidMeasurement,
	InsufficientSpeech,
	SpeechTooQuiet,
	NoiseDominated,
	QuietEvidenceMissing,
	OutputEvidenceMissing,
	AlreadyBelowTarget,
	ChangeLimited,
	PeakLimited,
	PeakRequiresManualAdjustment,
	AwaitingRepresentativeAudio,
	OutputsActive,
};

struct GainSuggestion {
	bool available{false};
	bool applicable{false};
	double delta_db{0.0};
	double minimum_delta_db{0.0};
	double maximum_delta_db{0.0};
	double target_lufs{std::numeric_limits<double>::quiet_NaN()};
	double predicted_lufs{std::numeric_limits<double>::quiet_NaN()};
	double predicted_peak_dbfs{std::numeric_limits<double>::quiet_NaN()};
	CalibrationReason reason{CalibrationReason::MissingCapture};
};

struct CalibrationPlan {
	bool measurement_ready{false};
	bool can_apply{false};
	CalibrationReason reason{CalibrationReason::InsufficientDuration};
	GainSuggestion voice;
	GainSuggestion game;
	GainSuggestion music;
};

namespace calibration_detail {
inline bool between(double value, double low, double high)
{
	return std::isfinite(value) && value >= low && value <= high;
}

inline bool valid_config(const CalibrationConfig &c)
{
	return between(c.voice_target_lufs, -30.0, -14.0) && between(c.game_gap_lu, 0.0, 24.0) &&
	       between(c.music_gap_lu, 0.0, 30.0) && between(c.minimum_total_seconds, 20.0, 120.0) &&
	       between(c.minimum_speech_seconds, 5.0, c.minimum_total_seconds) &&
	       between(c.minimum_quiet_seconds, 2.0, c.minimum_total_seconds) &&
	       c.minimum_speech_seconds + c.minimum_quiet_seconds <= c.minimum_total_seconds &&
	       between(c.minimum_voice_to_noise_lu, 6.0, 30.0) &&
	       between(c.minimum_voice_lufs, -60.0, -30.0) && between(c.voice_max_change_db, 0.0, 3.0) &&
	       between(c.background_max_cut_db, 0.0, 12.0) && between(c.peak_ceiling_dbfs, -12.0, -1.0) &&
	       between(c.deadband_db, 0.0, 2.0);
}

inline CalibrationReason source_reason(const CalibrationSource &s, double elapsed, const CalibrationConfig &c)
{
	if (!s.captured)
		return CalibrationReason::MissingCapture;
	if (!between(s.speech_seconds, 0.0, elapsed + 0.1) ||
	    !between(s.quiet_seconds, 0.0, elapsed + 0.1) ||
	    s.speech_seconds + s.quiet_seconds > elapsed + 0.1)
		return CalibrationReason::InvalidDuration;
	if (s.speech_seconds < c.minimum_speech_seconds)
		return CalibrationReason::InsufficientSpeech;
	if (!between(s.speech_lufs, -100.0, 3.0) || !between(s.peak_dbfs, -120.0, 12.0))
		return CalibrationReason::InvalidMeasurement;
	return CalibrationReason::Ready;
}

inline GainSuggestion suggestion(const CalibrationSource &s, double elapsed, double target,
				double minimum, double maximum, const CalibrationConfig &c,
				bool voice_boost_allowed)
{
	GainSuggestion out;
	out.minimum_delta_db = minimum;
	out.maximum_delta_db = maximum;
	out.target_lufs = target;
	out.reason = source_reason(s, elapsed, c);
	if (out.reason != CalibrationReason::Ready)
		return out;
	out.available = true;
	out.predicted_lufs = s.speech_lufs;
	out.predicted_peak_dbfs = s.peak_dbfs;
	const double requested = target - s.speech_lufs;
	double delta = std::clamp(requested, minimum, maximum);
	if (delta > 0.0 && !voice_boost_allowed) {
		delta = 0.0;
		out.reason = CalibrationReason::QuietEvidenceMissing;
	} else if (requested > maximum || requested < minimum) {
		out.reason = maximum == 0.0 && requested > 0.0 ? CalibrationReason::AlreadyBelowTarget
								 : CalibrationReason::ChangeLimited;
	}
	const double peak_limit = c.peak_ceiling_dbfs - s.peak_dbfs;
	if (peak_limit < minimum) {
		// A permitted small change cannot establish the promised headroom. Do
		// not auto-apply a partial fix and describe it as a completed solution.
		out.reason = CalibrationReason::PeakRequiresManualAdjustment;
		return out;
	}
	if (delta > peak_limit) {
		delta = peak_limit;
		out.reason = CalibrationReason::PeakLimited;
	}
	// The deadband cannot hide a necessary peak reduction.
	if (std::abs(delta) < c.deadband_db && peak_limit >= 0.0) {
		delta = 0.0;
		if (out.reason == CalibrationReason::Ready)
			out.reason = CalibrationReason::NoChangeNeeded;
	}
	out.delta_db = delta;
	out.predicted_lufs += delta;
	out.predicted_peak_dbfs += delta;
	out.applicable = std::abs(delta) > 1e-9;
	return out;
}

inline void limit_voice_to_observed_output(GainSuggestion &voice, const CalibrationSource &measured,
					  double output_peak, const CalibrationConfig &config)
{
	if (!voice.applicable || voice.delta_db <= 0.0)
		return;
	if (!between(output_peak, -120.0, 12.0)) {
		voice.maximum_delta_db = 0.0;
		voice.delta_db = 0.0;
		voice.applicable = false;
		voice.predicted_lufs = measured.speech_lufs;
		voice.predicted_peak_dbfs = measured.peak_dbfs;
		voice.reason = CalibrationReason::OutputEvidenceMissing;
		return;
	}
	// Additional working guard: observed whole-output sample peak plus the
	// proposed positive gain must remain <= -1 dBFS. This is not True Peak or
	// a mathematical guarantee of future/post-change output: phase, overlap,
	// dynamics and the program material may change. Do not infer that a
	// negative proposal alone fixes a clipped mix; that needs remeasurement.
	const double output_headroom = std::max(0.0, -1.0 - output_peak);
	voice.maximum_delta_db = std::min(voice.maximum_delta_db, output_headroom);
	if (voice.delta_db <= output_headroom)
		return;
	voice.delta_db = output_headroom < config.deadband_db ? 0.0 : output_headroom;
	voice.applicable = voice.delta_db > 1e-9;
	voice.predicted_lufs = measured.speech_lufs + voice.delta_db;
	voice.predicted_peak_dbfs = measured.peak_dbfs + voice.delta_db;
	voice.reason = CalibrationReason::PeakLimited;
}
} // namespace calibration_detail

inline CalibrationPlan make_calibration_plan(const CalibrationInput &input,
					     const CalibrationConfig &config = {})
{
	using namespace calibration_detail;
	CalibrationPlan out;
	if (!valid_config(config)) {
		out.reason = CalibrationReason::InvalidConfiguration;
		return out;
	}
	if (!between(input.elapsed_seconds, 0.0, 3600.0)) {
		out.reason = CalibrationReason::InvalidDuration;
		return out;
	}
	if (input.elapsed_seconds < config.minimum_total_seconds)
		return out;
	out.reason = source_reason(input.voice, input.elapsed_seconds, config);
	if (out.reason != CalibrationReason::Ready)
		return out;
	if (input.voice.speech_lufs < config.minimum_voice_lufs) {
		out.reason = CalibrationReason::SpeechTooQuiet;
		return out;
	}
	const bool quiet_valid = input.voice.quiet_seconds >= config.minimum_quiet_seconds &&
				 between(input.voice.quiet_lufs, -120.0, 3.0);
	if (quiet_valid && input.voice.speech_lufs - input.voice.quiet_lufs < config.minimum_voice_to_noise_lu) {
		out.reason = CalibrationReason::NoiseDominated;
		return out;
	}
	out.measurement_ready = true;
	out.voice = suggestion(input.voice, input.elapsed_seconds, config.voice_target_lufs,
			       -config.voice_max_change_db, config.voice_max_change_db, config, quiet_valid);
	limit_voice_to_observed_output(out.voice, input.voice, input.output_peak_dbfs, config);
	// Compare backgrounds with the voice level after the applicable proposal.
	const double voice_lufs = input.voice.speech_lufs + (out.voice.applicable ? out.voice.delta_db : 0.0);
	out.game = suggestion(input.game, input.elapsed_seconds, voice_lufs - config.game_gap_lu,
			      -config.background_max_cut_db, 0.0, config, false);
	out.music = suggestion(input.music, input.elapsed_seconds, voice_lufs - config.music_gap_lu,
			       -config.background_max_cut_db, 0.0, config, false);
	if (!input.context_confirmed)
		out.reason = CalibrationReason::AwaitingRepresentativeAudio;
	else if (!input.outputs_idle)
		out.reason = CalibrationReason::OutputsActive;
	else if (out.voice.reason == CalibrationReason::PeakRequiresManualAdjustment ||
		 out.game.reason == CalibrationReason::PeakRequiresManualAdjustment ||
		 out.music.reason == CalibrationReason::PeakRequiresManualAdjustment)
		// Leave a clearly reported unresolved clipping/headroom problem rather
		// than auto-applying only the easy part of a calibration proposal.
		out.reason = CalibrationReason::PeakRequiresManualAdjustment;
	else {
		out.can_apply = out.voice.applicable || out.game.applicable || out.music.applicable;
		out.reason = out.can_apply ? CalibrationReason::Ready : CalibrationReason::NoChangeNeeded;
	}
	return out;
}

} // namespace lbm
