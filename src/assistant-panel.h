// TRUEITEM modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#pragma once
#include "calibration-plan.h"
#include "output-monitor.h"
#include <obs-frontend-api.h>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QWidget>
#include <array>
#include <string>

class QCheckBox;
class QDialog;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

namespace lbm {
class AssistantPanel : public QWidget {
	Q_OBJECT
public:
	explicit AssistantPanel(QWidget *parent = nullptr);
	~AssistantPanel() override;
	void suspend();
public slots:
	void refresh();
	void startCalibration();
	void cancelCalibration();
	void applySuggestions();
	void undoAdjustment();
	void saveReport();
	void showDetails();
	void showChannelSetup();
	void showHelp();
private:
	struct Accumulator {
		double energy{0}, seconds{0}, quiet_energy{0}, quiet_seconds{0};
		double peak{-120};
		void add(double lufs, double dt, bool quiet);
		CalibrationSource summary(bool connected) const;
	};
	void tick();
	void finishCalibration();
	void connectSources();
	void loadSettings();
	void saveSettings();
	void updateRoutes();
	void showPlan();
	void updateActions();
	void setStage(const QString &heading, const QString &message, const QString &hint, bool counting = false);
	void restoreUndoState();
	bool sourceIdentityUnchanged() const;
	static void frontendEvent(enum obs_frontend_event event, void *data);
	std::array<QComboBox *, 4> selectors_{};
	std::array<QLabel *, 5> meters_{};
	std::array<StereoMonitor, 5> monitors_;
	std::array<StereoMeterSnapshot, 5> measured_{};
	std::array<Accumulator, 3> accum_{};
	std::array<std::string, 3> session_ids_{};
	std::array<double, 3> session_volumes_{};
	std::array<QString, 3> session_signatures_{};
	std::array<std::string, 4> source_names_{};
	QLabel *status_{nullptr}, *routes_{nullptr}, *proposal_{nullptr};
	QLabel *phase_{nullptr}, *countdown_{nullptr};
	QLabel *route_summary_{nullptr};
	std::array<QLabel *,3> role_states_{}, recommendation_labels_{};
	std::array<QProgressBar *,3> level_bars_{};
	QWidget *recommendations_{nullptr};
	QDialog *details_{nullptr}, *help_{nullptr};
	QLabel *track_names_{nullptr};
	QProgressBar *progress_{nullptr};
	QPushButton *start_{nullptr}, *cancel_{nullptr}, *apply_{nullptr}, *undo_{nullptr};
	QCheckBox *representative_{nullptr};
	QCheckBox *source_toggle_{nullptr};
	std::array<QDoubleSpinBox *, 3> targets_{};
	QTimer *timer_{nullptr};
	QElapsedTimer elapsed_;
	double last_tick_{0};
	bool measuring_{false}, suspended_{false}, result_ready_{false}, applying_{false};
	int stream_track_{-1}, record_track_{-1};
	uint64_t recording_mask_{0};
	CalibrationInput input_;
	CalibrationPlan plan_;
	QJsonArray undo_state_;
	QString measured_at_;
};
} // namespace lbm
