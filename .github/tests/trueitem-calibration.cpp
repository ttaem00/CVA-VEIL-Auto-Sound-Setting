// TRUEITEM modification, 2026-11-09. GPL-2.0; see LICENSE.
#include "../../src/calibration-plan.h"

#include <iostream>
#include <stdexcept>

using namespace lbm;

static void check(bool value, const char *message)
{
	if (!value)
		throw std::runtime_error(message);
}

static CalibrationInput representative()
{
	CalibrationInput in;
	in.elapsed_seconds = 30.0;
	in.context_confirmed = true;
	in.outputs_idle = true;
	in.output_peak_dbfs = -8.0;
	in.voice = {true, -23.0, -9.0, 15.0, -55.0, 5.0};
	in.game = {true, -22.0, -8.0, 15.0, -30.0, 5.0};
	in.music = {true, -24.0, -10.0, 15.0, -24.0, 5.0};
	return in;
}

int main()
{
	try {
		auto in = representative();
		auto plan = make_calibration_plan(in);
		check(plan.measurement_ready && plan.can_apply, "representative speech not actionable");
		check(plan.voice.delta_db == 3.0, "voice reference correction");
		check(plan.game.delta_db == -4.0 && plan.music.delta_db == -6.0, "background references/cap");
		check(plan.voice.predicted_peak_dbfs <= -3.0, "voice headroom promise");

		in.output_peak_dbfs = std::numeric_limits<double>::quiet_NaN();
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 0.0 && !plan.voice.applicable &&
			      plan.voice.reason == CalibrationReason::OutputEvidenceMissing,
		      "voice boosted without output peak evidence");
		check(plan.game.target_lufs == -29.0 && plan.music.target_lufs == -35.0 &&
			      plan.game.delta_db == -6.0 && plan.can_apply,
		      "missing output blocked safe cuts or used unapplied voice gain");
		in.voice.speech_lufs = -16.0;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == -3.0 && plan.can_apply,
		      "missing output peak blocked safe voice reduction");
		in = representative();
		in.output_peak_dbfs = -2.0;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 1.0 && plan.voice.maximum_delta_db == 1.0 &&
			      plan.voice.reason == CalibrationReason::PeakLimited &&
			      plan.game.target_lufs == -28.0 && plan.music.target_lufs == -34.0,
		      "output peak guard or final voice-relative targets ignored");
		check(in.output_peak_dbfs + plan.voice.delta_db <= -1.0,
		      "observed output-plus-gain ceiling exceeded");
		in.output_peak_dbfs = 0.5;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 0.0 && plan.voice.reason == CalibrationReason::PeakLimited &&
			      plan.game.delta_db < 0.0 && plan.can_apply,
		      "already-clipped output permitted boost or blocked reductions");
		in.output_peak_dbfs = -1.5;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 0.0 && plan.voice.reason == CalibrationReason::PeakLimited,
		      "sub-deadband output-limited boost applied");
		in.output_peak_dbfs = std::numeric_limits<double>::infinity();
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 0.0 && plan.voice.reason == CalibrationReason::OutputEvidenceMissing,
		      "invalid output peak permitted boost");
		in = representative();

		in.voice.speech_seconds = 0.0;
		plan = make_calibration_plan(in);
		check(!plan.can_apply && !plan.measurement_ready && plan.reason == CalibrationReason::InsufficientSpeech,
		      "silence produced a plan");
		in = representative();
		in.voice.speech_lufs = -45.0;
		in.voice.quiet_lufs = -46.0;
		plan = make_calibration_plan(in);
		check(!plan.can_apply && plan.reason == CalibrationReason::NoiseDominated, "all-noise boosted");
		in = representative();
		in.voice.speech_lufs = -80.0;
		plan = make_calibration_plan(in);
		check(!plan.can_apply && plan.reason == CalibrationReason::SpeechTooQuiet, "noise floor boosted");

		in = representative();
		in.voice.captured = false;
		plan = make_calibration_plan(in);
		check(!plan.can_apply && plan.reason == CalibrationReason::MissingCapture, "missing voice accepted");
		in = representative();
		in.game.captured = false;
		plan = make_calibration_plan(in);
		check(plan.can_apply && !plan.game.available && plan.game.delta_db == 0.0,
		      "missing background changed");
		in = representative();
		in.game.speech_lufs = -40.0;
		in.music.speech_lufs = -50.0;
		plan = make_calibration_plan(in);
		check(plan.game.delta_db == 0.0 && plan.music.delta_db == 0.0, "background boosted");

		in = representative();
		in.voice.peak_dbfs = -4.0;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 1.0 && plan.voice.reason == CalibrationReason::PeakLimited,
		      "peak ceiling ignored");
		in.voice.peak_dbfs = 1.0;
		plan = make_calibration_plan(in);
		check(!plan.voice.applicable && plan.voice.delta_db == 0.0 &&
			      plan.voice.reason == CalibrationReason::PeakRequiresManualAdjustment && !plan.can_apply &&
			      plan.reason == CalibrationReason::PeakRequiresManualAdjustment,
		      "partial clipping fix automatically applied");
		in = representative();
		in.voice.quiet_seconds = 0.0;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == 0.0 && plan.voice.reason == CalibrationReason::QuietEvidenceMissing,
		      "voice boost without noise evidence");
		in.voice.speech_lufs = -16.0;
		plan = make_calibration_plan(in);
		check(plan.voice.delta_db == -3.0, "safe voice reduction without quiet evidence blocked");

		in = representative();
		CalibrationConfig cfg;
		cfg.voice_target_lufs = -24.0;
		cfg.game_gap_lu = 8.0;
		cfg.music_gap_lu = 15.0;
		plan = make_calibration_plan(in, cfg);
		check(plan.voice.delta_db == -1.0 && plan.game.target_lufs == -32.0 &&
			      plan.music.target_lufs == -39.0,
		      "changed references ignored");
		cfg.voice_max_change_db = 20.0;
		plan = make_calibration_plan(in, cfg);
		check(!plan.can_apply && plan.reason == CalibrationReason::InvalidConfiguration,
		      "unsafe configured voice bound accepted");
		in.elapsed_seconds = 10.0;
		plan = make_calibration_plan(in);
		check(!plan.can_apply && plan.reason == CalibrationReason::InsufficientDuration, "short test accepted");
		in = representative();
		in.voice.speech_seconds = 31.0;
		plan = make_calibration_plan(in);
		check(!plan.can_apply && plan.reason == CalibrationReason::InvalidDuration,
		      "impossible valid duration accepted");
		in = representative();
		in.voice.speech_lufs = std::numeric_limits<double>::infinity();
		plan = make_calibration_plan(in);
		check(!plan.can_apply && plan.reason == CalibrationReason::InvalidMeasurement, "nonfinite audio accepted");
		in = representative();
		in.context_confirmed = false;
		plan = make_calibration_plan(in);
		check(plan.measurement_ready && !plan.can_apply &&
			      plan.reason == CalibrationReason::AwaitingRepresentativeAudio,
		      "unconfirmed test automatically applicable");
		in.context_confirmed = true;
		in.outputs_idle = false;
		plan = make_calibration_plan(in);
		check(plan.measurement_ready && !plan.can_apply && plan.reason == CalibrationReason::OutputsActive,
		      "live application not gated");
		std::cout << "PASS calibration valid_voice silence noise missing_capture peak_limit bounded_changes "
			     "quiet_evidence configurable_targets invalid_inputs representative_context live_guard "
			     "missing_output_peak output_peak_limit clipped_output_safe_cuts final_voice_targets\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "FAIL calibration: " << error.what() << '\n';
		return 1;
	}
}
