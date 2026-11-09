// TRUEITEM modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#include "assistant-panel.h"
#include "audio-capture.h"
#include "preview-track.h"
#include "ui-text.h"
#include "channel-setup.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QTabWidget>
#include <QToolButton>
#include <QStyle>
#include <QScreen>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace lbm {
namespace {
QString configPath(const char *file)
{
	char *dir = obs_module_config_path(nullptr);
	if (dir) { os_mkdirs(dir); bfree(dir); }
	char *p = obs_module_config_path(file);
	QString result = QString::fromUtf8(p ? p : "");
	bfree(p);
	return result;
}
QJsonObject readJson(const char *name)
{
	QFile file(configPath(name));
	if (!file.open(QIODevice::ReadOnly)) return {};
	return QJsonDocument::fromJson(file.readAll()).object();
}
bool writeJson(const char *name, const QJsonObject &value)
{
	QSaveFile file(configPath(name));
	if (!file.open(QIODevice::WriteOnly)) return false;
	if (file.write(QJsonDocument(value).toJson()) < 0) return false;
	return file.commit();
}
QLabel *label(const QString &text, const char *name = nullptr)
{
	auto *p = new QLabel(text);
	p->setWordWrap(true); p->setTextFormat(Qt::PlainText); p->setMinimumWidth(0);
	QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);policy.setHeightForWidth(true);p->setSizePolicy(policy);
	if (name) p->setObjectName(name);
	return p;
}
bool idle()
{
	return !obs_frontend_streaming_active() && !obs_frontend_recording_active() &&
	       !obs_frontend_replay_buffer_active();
}
QString number(double value, const QString &unit)
{
	return std::isfinite(value) ? QString::number(value, 'f', 1) + " " + unit : "— " + unit;
}
QString reason(CalibrationReason r)
{
	switch (r) {
	case CalibrationReason::Ready: return uiText("안전한 범위의 제안을 확인하세요.");
	case CalibrationReason::NoChangeNeeded: return uiText("현재 참고 범위입니다. 음량을 바꿀 필요가 없습니다.");
	case CalibrationReason::MissingCapture: return uiText("입력이 없는 소스가 있습니다. 소스 선택과 재생 상태를 확인하세요.");
	case CalibrationReason::InsufficientSpeech: return uiText("유효한 말소리가 5초보다 짧습니다. 평소 목소리로 다시 확인하세요.");
	case CalibrationReason::SpeechTooQuiet: return uiText("목소리가 너무 작아 자동 증폭을 보류했습니다. 거리와 입력을 확인하세요.");
	case CalibrationReason::NoiseDominated: return uiText("말소리와 방 잡음의 차이가 작습니다. 게인보다 마이크 거리·잡음을 먼저 확인하세요.");
	case CalibrationReason::QuietEvidenceMissing: return uiText("정숙 구간 근거가 부족해 목소리 증폭을 보류했습니다.");
	case CalibrationReason::OutputEvidenceMissing: return uiText("실제 출력 피크 근거가 부족해 목소리 증폭을 보류했습니다.");
	case CalibrationReason::AlreadyBelowTarget: return uiText("이미 배경음 목표보다 작습니다. 자동으로 키우지 않습니다.");
	case CalibrationReason::ChangeLimited: return uiText("작은 보정 한도 안에서 제안합니다. 적용 후 다시 들어 보세요.");
	case CalibrationReason::PeakLimited: return uiText("큰 순간 소리의 여유를 남기도록 보정량을 제한했습니다.");
	case CalibrationReason::PeakRequiresManualAdjustment: return uiText("큰 순간 소리가 보정 한도를 넘습니다. 자동 적용을 보류했습니다.");
	case CalibrationReason::AwaitingRepresentativeAudio: return uiText("평소 게임·음악·목소리로 테스트했는지 확인해 주세요.");
	case CalibrationReason::OutputsActive: return uiText("방송·녹화 중에는 제안값을 적용하지 않습니다.");
	case CalibrationReason::InsufficientDuration: return uiText("측정 시간이 부족합니다. 30초 확인을 다시 시작하세요.");
	default: return uiText("측정값이 충분하지 않습니다. 소리 입력을 확인하고 다시 측정하세요.");
	}
}
bool needsMoreEvidence(CalibrationReason r)
{
    switch(r){
    case CalibrationReason::MissingCapture:case CalibrationReason::InsufficientSpeech:
    case CalibrationReason::SpeechTooQuiet:case CalibrationReason::NoiseDominated:
    case CalibrationReason::QuietEvidenceMissing:case CalibrationReason::OutputEvidenceMissing:
    case CalibrationReason::PeakRequiresManualAdjustment:return true;
    default:return false;
    }
}
QJsonValue finite(double v) { return std::isfinite(v) ? QJsonValue(v) : QJsonValue(); }
std::string uuid(obs_source_t *source)
{
	const char *id = obs_source_get_uuid(source);
	return id ? id : "";
}
QString audioSignature(obs_source_t *source)
{
	QJsonArray filters;
	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *data) {
		auto *items=static_cast<QJsonArray *>(data);
		QJsonObject f{{"id",QString::fromUtf8(obs_source_get_id(filter))},{"enabled",obs_source_enabled(filter)}};
		obs_data_t *settings=obs_source_get_settings(filter);
		for(const char *key:{"db","gain_db","threshold","ratio","attack_time","release_time","output_gain","release"}) {
			// Numeric audio controls only; no VST chunks, paths, URLs or credentials.
			f[QString::fromUtf8(key)]=obs_data_get_double(settings,key);
			f[QString::fromUtf8(key)+"_int"]=double(obs_data_get_int(settings,key));
		}
		f["sidechain_source"]=QString::fromUtf8(obs_data_get_string(settings,"sidechain_source"));
		obs_data_release(settings);items->append(f);
	},&filters);
	return QString::fromUtf8(QJsonDocument(QJsonObject{{"mixers",int(obs_source_get_audio_mixers(source))},
		{"muted",obs_source_muted(source)},{"flags",int(obs_source_get_flags(source))},
		{"sync",double(obs_source_get_sync_offset(source))},{"filters",filters}}).toJson(QJsonDocument::Compact));
}
} // namespace

void AssistantPanel::Accumulator::add(double lufs, double dt, bool quiet)
{
	if (!std::isfinite(lufs) || lufs < -100 || lufs > 3 || dt <= 0) return;
	if (quiet) { quiet_energy += std::pow(10., lufs / 10.) * dt; quiet_seconds += dt; }
	else { energy += std::pow(10., lufs / 10.) * dt; seconds += dt; }
}
CalibrationSource AssistantPanel::Accumulator::summary(bool connected) const
{
	CalibrationSource s;
	s.captured = connected; s.speech_seconds = seconds; s.quiet_seconds = quiet_seconds;
	s.speech_lufs = seconds > 0 ? 10 * std::log10(energy / seconds) : NAN;
	s.quiet_lufs = quiet_seconds > 0 ? 10 * std::log10(quiet_energy / quiet_seconds) : NAN;
	s.peak_dbfs = peak;
	return s;
}

AssistantPanel::AssistantPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName("trueitemAssistant");
    setStyleSheet(R"(
      QWidget#trueitemAssistant { background: #15191e; color: #f1f4f6; }
      #trueitemAssistant QLabel { color: #f1f4f6; background: transparent; font-size: 13px; }
      #trueitemAssistant QLabel[tone="muted"] { color: #b0bac5; font-size: 12px; }
      #trueitemAssistant QLabel[tone="brand"] { color: #a9b6c4; font-size: 11px; font-weight: 600; }
      #trueitemAssistant QFrame[card="true"] { background: #20262e; border: 1px solid #343e49; border-radius: 12px; }
      #trueitemAssistant QPushButton { min-height: 34px; padding: 3px 12px; border: 1px solid #485564; border-radius: 7px; color: #e7eef5; background: #29323d; font-size: 13px; }
      #trueitemAssistant QPushButton:hover { background: #344253; border-color: #8ba2b9; }
      #trueitemAssistant QPushButton[primary="true"] { min-height: 40px; font-weight: 600; border-color: #98ead1; background: #98ead1; color: #102a23; }
      #trueitemAssistant QPushButton[primary="true"]:hover { background: #b6f3df; }
      #trueitemAssistant QPushButton:disabled { color: #9aa7b5; background: #2a333e; border-color: #3a4552; }
      #trueitemAssistant QPushButton:focus, #trueitemAssistant QToolButton:focus, #trueitemAssistant QCheckBox:focus { border: 2px solid #9fc9ff; }
      #trueitemAssistant QToolButton { color: #bdc9d6; background: transparent; border: 1px solid transparent; border-radius: 5px; min-height: 30px; padding: 2px 7px; font-size: 12px; }
      #trueitemAssistant QToolButton:hover { background: #2b3541; color: #fff; }
      #trueitemAssistant QCheckBox { color: #c1ccd7; font-size: 12px; spacing: 8px; min-height: 28px; }
      #trueitemAssistant QCheckBox::indicator { width: 17px; height: 17px; }
      #trueitemAssistant QProgressBar { border: none; background: #35404d; border-radius: 3px; min-height: 6px; max-height: 6px; }
      #trueitemAssistant QProgressBar::chunk { border-radius: 3px; background: #98ead1; }
    )");
    auto *outer=new QVBoxLayout(this);outer->setContentsMargins(18,16,18,16);outer->setSpacing(16);
    auto *header=new QHBoxLayout();
    auto *brand=label("CVA · VEIL", "trueitemTitle");brand->setProperty("tone","brand");
    brand->setToolTip("CVA-VEIL-Auto Sound Setting 1.3.0");header->addWidget(brand,1);
    auto *connection=new QToolButton();connection->setObjectName("cvaChannelSetup");connection->setText(uiText("채널 설정"));connection->setAccessibleName(uiText("소리 연결 설정"));connection->setFocusPolicy(Qt::StrongFocus);
    auto *helpButton=new QToolButton();helpButton->setText(uiText("도움말"));helpButton->setAccessibleName(uiText("소리 검사 도움말"));helpButton->setFocusPolicy(Qt::StrongFocus);
    auto *language = new QComboBox(); language->setObjectName("cvaLanguage");
    language->addItem("한국어", "ko-KR"); language->addItem("English", "en-US"); language->addItem("日本語", "ja-JP");
    language->setCurrentIndex(language->findData(uiLanguage())); language->setMaximumWidth(96); language->setAccessibleName(uiText("표시 언어"));
    header->addWidget(language);header->addWidget(connection);header->addWidget(helpButton);outer->addLayout(header);
    connect(language,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,language]{
        if(measuring_)cancelCalibration(); setUiLanguage(language->currentData().toString());
        if(uiLanguage()!=language->currentData().toString()){
            const QSignalBlocker blocker(language);language->setCurrentIndex(language->findData(uiLanguage()));
            status_->setText(uiText("표시 언어를 저장하지 못했어요. 다시 선택해 주세요."));return;
        }
        translateUi(this); updateRoutes(); if(result_ready_)showPlan(); updateActions();
    });

    auto *inputs=new QFrame();inputs->setProperty("card",true);auto *inputLayout=new QVBoxLayout(inputs);inputLayout->setContentsMargins(16,14,16,14);inputLayout->setSpacing(12);
    auto *inputTitle=label(uiText("지금 들어오는 소리"));inputTitle->setProperty("tone","muted");inputLayout->addWidget(inputTitle);
    const std::array<QString,3> roles{uiText("내 목소리"),uiText("게임"),uiText("음악")};
    for(size_t i=0;i<3;++i){
        auto *row=new QVBoxLayout();row->setSpacing(6);auto *names=new QHBoxLayout();
        auto *name=label(roles[i]);auto f=name->font();f.setBold(true);name->setFont(f);names->addWidget(name,1);
        role_states_[i]=label(i==0?uiText("말해 보세요"):uiText("재생 대기"),qPrintable(QString("cvaRoleState%1").arg(i)));
        role_states_[i]->setProperty("tone","muted");role_states_[i]->setAlignment(Qt::AlignRight);names->addWidget(role_states_[i],1);row->addLayout(names);
        level_bars_[i]=new QProgressBar();level_bars_[i]->setRange(0,100);level_bars_[i]->setValue(0);level_bars_[i]->setTextVisible(false);level_bars_[i]->setAccessibleName(roles[i]+uiText(" 입력 크기"));
        level_bars_[i]->setToolTip(uiText("들어오는 소리의 크기를 보여 줍니다. 음량 조절 버튼은 아닙니다."));row->addWidget(level_bars_[i]);inputLayout->addLayout(row);
    }
    outer->addWidget(inputs);

    auto *check=new QFrame();check->setProperty("card",true);auto *controls=new QVBoxLayout(check);controls->setContentsMargins(18,17,18,17);controls->setSpacing(12);
    phase_=label(uiText("30초 소리 검사"),"trueitemPhase");phase_->setProperty("tone","muted");controls->addWidget(phase_);
    countdown_=label(uiText("목소리가 잘 들리는 균형"), "trueitemCountdown");countdown_->setMinimumHeight(54);controls->addWidget(countdown_);
    status_=label(uiText("사용할 소리를 켜고 시작하세요."),"trueitemStatus");status_->setProperty("tone","muted");controls->addWidget(status_);
    progress_=new QProgressBar();progress_->setRange(0,30);progress_->setValue(0);progress_->setTextVisible(false);progress_->setAccessibleName(uiText("소리 검사 진행"));progress_->hide();controls->addWidget(progress_);
    recommendations_=new QWidget();auto *recommendLayout=new QVBoxLayout(recommendations_);recommendLayout->setContentsMargins(0,0,0,0);recommendLayout->setSpacing(8);
    for(size_t i=0;i<3;++i){auto *row=new QHBoxLayout();row->addWidget(label(roles[i]),1);recommendation_labels_[i]=label(uiText("확인 전"));recommendation_labels_[i]->setAlignment(Qt::AlignRight);row->addWidget(recommendation_labels_[i],1);recommendLayout->addLayout(row);}
    recommendations_->hide();controls->addWidget(recommendations_);
    representative_=new QCheckBox(uiText("사용할 소리를 평소 크기로 준비했어요"));representative_->setObjectName("trueitemRepresentative");controls->addWidget(representative_);
    connect(representative_,&QCheckBox::toggled,this,[this]{if(result_ready_)showPlan();else updateActions();});
    start_=new QPushButton(uiText("소리 검사 시작"));start_->setObjectName("trueitemStart");start_->setProperty("primary",true);start_->setToolTip(uiText("25초 동안 말한 뒤, 화면 안내에 따라 마지막 5초는 조용히 있어 주세요. 음성 파일은 저장하지 않습니다."));
    cancel_=new QPushButton(uiText("검사 취소"));cancel_->setObjectName("trueitemCancel");cancel_->setEnabled(false);cancel_->hide();
    apply_=new QPushButton(uiText("추천 설정 적용"));apply_->setObjectName("trueitemApply");apply_->setProperty("primary",true);apply_->setEnabled(false);apply_->hide();
    connect(start_,&QPushButton::clicked,this,&AssistantPanel::startCalibration);connect(cancel_,&QPushButton::clicked,this,&AssistantPanel::cancelCalibration);connect(apply_,&QPushButton::clicked,this,&AssistantPanel::applySuggestions);
    controls->addWidget(apply_);controls->addWidget(start_);controls->addWidget(cancel_);outer->addWidget(check);
    route_summary_=label(uiText("방송·녹화 연결 확인 중"),"cvaRouteSummary");route_summary_->setProperty("tone","muted");outer->addWidget(route_summary_);
    auto *footer=new QHBoxLayout();
    auto *detailsButton=new QToolButton();detailsButton->setText(uiText("세부 정보"));detailsButton->setAccessibleName(uiText("측정 수치와 조절 기준 보기"));detailsButton->setFocusPolicy(Qt::StrongFocus);connect(detailsButton,&QToolButton::clicked,this,&AssistantPanel::showDetails);footer->addWidget(detailsButton);
    auto *save=new QPushButton(uiText("저장"));save->setObjectName("trueitemSave");save->setAccessibleName(uiText("측정 결과와 현재 설정 저장"));save->setToolTip(uiText("측정 수치와 현재 설정을 저장합니다. 소리를 녹음하지 않습니다."));connect(save,&QPushButton::clicked,this,&AssistantPanel::saveReport);footer->addWidget(save);
    footer->addStretch();undo_=new QPushButton(uiText("되돌리기"));undo_->setObjectName("trueitemUndo");undo_->setAccessibleName(uiText("직전 보정 전 음량으로 되돌리기"));undo_->setToolTip(uiText("이 도우미가 마지막으로 보정하기 전의 음량으로 돌아갑니다."));connect(undo_,&QPushButton::clicked,this,&AssistantPanel::undoAdjustment);footer->addWidget(undo_);outer->addLayout(footer);

    details_=new QDialog(this);details_->setObjectName("cvaDetails");details_->setWindowTitle(uiText("세부 정보 · CVA-VEIL-Auto Sound Setting"));details_->setStyleSheet("QDialog#cvaDetails { background:#20262e; color:#eff4f8; } QLabel { color:#dbe4ee; }");
    auto *detailOuter=new QVBoxLayout(details_);detailOuter->setContentsMargins(16,16,16,16);auto *tabs=new QTabWidget();tabs->setObjectName("cvaDetailTabs");detailOuter->addWidget(tabs);
    auto *meterPage=new QWidget();auto *meterLayout=new QVBoxLayout(meterPage);meterLayout->setSpacing(12);
    track_names_=label(uiText("확인 중"),"trueitemTracks");meterLayout->addWidget(track_names_);
    for(size_t i=0;i<5;++i){meters_[i]=label("—",qPrintable(QString("trueitemMeter%1").arg(i)));meterLayout->addWidget(meters_[i]);}
    proposal_=label(uiText("검사 전"),"trueitemProposal");meterLayout->addWidget(proposal_);
    meterLayout->addWidget(label(uiText("LUFS-S는 최근 3초, LUFS-I는 검사 전체의 평균적인 크기입니다. 피크는 가장 큰 순간의 샘플 값이며, True Peak는 측정하지 않습니다.")));meterLayout->addStretch();tabs->addTab(meterPage,uiText("측정 수치"));
    auto *sourcePage=new QWidget();auto *form=new QFormLayout(sourcePage);form->setSpacing(14);
    const std::array<QString,4> sourceNames{uiText("내 목소리"),uiText("게임"),uiText("음악"),uiText("방송을 듣는 앱")};
    for(size_t i=0;i<4;++i){selectors_[i]=new QComboBox();selectors_[i]->setMinimumWidth(0);selectors_[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);selectors_[i]->setObjectName(QString("trueitemSource%1").arg(i));selectors_[i]->setAccessibleName(sourceNames[i]+uiText(" 연결 소스"));form->addRow(sourceNames[i],selectors_[i]);connect(selectors_[i],QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,i]{if(measuring_)cancelCalibration();source_names_[i]=selectors_[i]->currentData().toString().toStdString();result_ready_=false;recommendations_->hide();connectSources();saveSettings();updateRoutes();updateActions();});}
    auto *reload=new QPushButton(uiText("연결 다시 확인"));connect(reload,&QPushButton::clicked,this,&AssistantPanel::refresh);form->addRow(reload);routes_=label(uiText("확인 중"),"trueitemRoutes");form->addRow(routes_);tabs->addTab(sourcePage,uiText("연결 설정"));
    source_toggle_=new QCheckBox(details_);source_toggle_->hide();
    auto *targetPage=new QWidget();auto *targets=new QFormLayout(targetPage);targets->setSpacing(14);
    const std::array<QString,3> targetNames{uiText("목소리 참고 크기"),uiText("게임을 목소리보다 작게"),uiText("음악을 목소리보다 작게")};
    for(size_t i=0;i<3;++i){targets_[i]=new QDoubleSpinBox();targets_[i]->setDecimals(1);targets_[i]->setRange(i==0?-30:0,i==0?-14:24);targets_[i]->setSuffix(i==0?" LUFS":" LU");targets_[i]->setValue(i==0?-20:(i==1?6:12));targets_[i]->setAccessibleName(targetNames[i]);targets->addRow(targetNames[i],targets_[i]);connect(targets_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{saveSettings();if(result_ready_)showPlan();});}
    targets->addRow(label(uiText("개인 참고값입니다. 방송 플랫폼의 공식 규격이 아닙니다. 보정은 목소리 최대 ±3dB, 배경음 최대 6dB 감쇠로 제한합니다. 배경음은 자동 증폭하지 않습니다.")));tabs->addTab(targetPage,uiText("조절 기준"));
    auto *detailClose=new QDialogButtonBox(QDialogButtonBox::Close);detailClose->button(QDialogButtonBox::Close)->setText(uiText("닫기"));connect(detailClose,&QDialogButtonBox::rejected,details_,&QDialog::close);detailOuter->addWidget(detailClose);
    connect(connection,&QToolButton::clicked,this,&AssistantPanel::showChannelSetup);
    help_=new QDialog(this);help_->setWindowTitle(uiText("도움말 · CVA-VEIL-Auto Sound Setting"));auto *helpLayout=new QVBoxLayout(help_);helpLayout->setContentsMargins(22,20,22,20);helpLayout->setSpacing(16);
    helpLayout->addWidget(label("CVA-VEIL-Auto Sound Setting 1.3.0"));
    helpLayout->addWidget(label(uiText("1. 사용할 소리를 평소 크기로 재생하세요.\n2. 검사를 시작하고 25초 동안 평소처럼 말하세요.\n3. ‘말을 멈춰 주세요’가 나오면 5초 동안 조용히 기다리세요.\n4. 추천을 확인하고 적용한 뒤, 녹화나 방송으로 들어 보세요.")));
    helpLayout->addWidget(label(uiText("검사만으로 음량은 바뀌지 않아요. 적용하기 전에 되돌릴 음량을 저장해요. 방송·녹화·OBS의 다시보기 임시 저장이 켜져 있으면 적용하지 않아요.")));
    helpLayout->addWidget(label(uiText("게임과 음악은 사용하지 않아도 검사할 수 있어요. 소리 막대는 입력이 들어오는지 보여 줍니다. ‘들어오는 중’은 음량이 최적이라는 뜻이 아니에요. 방 잡음이나 소리가 없는 구간을 따라 음량을 키우지 않아요. 수치와 연결, 조절 기준은 ‘세부 정보’에서 볼 수 있어요.")));
    helpLayout->addWidget(label(uiText("음악을 녹화에서 뺄 수 있는지는 OBS 연결을 확인해요. 게임 자체의 음악이나 하드웨어에서 이미 섞인 소리는 따로 분리하지 못해요. 검사 수치는 참고용이고 최종 소리는 직접 들어 확인하세요.")));
    auto *helpClose=new QDialogButtonBox(QDialogButtonBox::Close);helpClose->button(QDialogButtonBox::Close)->setText(uiText("닫기"));connect(helpClose,&QDialogButtonBox::rejected,help_,&QDialog::close);helpLayout->addWidget(helpClose);connect(helpButton,&QToolButton::clicked,this,&AssistantPanel::showHelp);
    loadSettings();restoreUndoState();refresh();
    setStage(uiText("30초 소리 검사"),uiText("목소리가 잘 들리는 균형"),uiText("사용할 소리를 켜고 시작하세요."));
    timer_=new QTimer(this);timer_->setInterval(250);connect(timer_,&QTimer::timeout,this,&AssistantPanel::tick);timer_->start();obs_frontend_add_event_callback(frontendEvent,this);updateActions();translateUi(this);
}
void AssistantPanel::showChannelSetup()
{
    if(measuring_)cancelCalibration();
    auto *setup=new ChannelSetupDialog(this);setup->setAttribute(Qt::WA_DeleteOnClose);
    connect(setup,&QDialog::accepted,this,[this]{loadSettings();refresh();});setup->open();
}
void AssistantPanel::showDetails()
{
    details_->resize(QSize(560,490).boundedTo(screen()->availableGeometry().size()-QSize(40,80)));details_->show();details_->raise();
}
void AssistantPanel::showHelp()
{
    help_->resize(QSize(500,480).boundedTo(screen()->availableGeometry().size()-QSize(40,80)));help_->show();help_->raise();
}
void AssistantPanel::setStage(const QString &heading,const QString &message,const QString &hint,bool counting)
{
    phase_->setText(heading);countdown_->setText(message);status_->setText(hint);
    countdown_->setStyleSheet(counting?"font-size:54px;font-weight:600;color:#f5fbf8;":"font-size:23px;font-weight:600;color:#f1f4f6;");
}
void AssistantPanel::updateActions()
{
    const bool canChange=idle();
    const bool readyApply=result_ready_ && plan_.can_apply && sourceIdentityUnchanged();
    start_->setVisible(!measuring_);start_->setEnabled(canChange && representative_->isChecked());
    start_->setText(result_ready_?uiText("다시 검사"):uiText("소리 검사 시작"));
    if(start_->property("primary").toBool()==readyApply){start_->setProperty("primary",!readyApply);start_->style()->unpolish(start_);start_->style()->polish(start_);}
    cancel_->setVisible(measuring_);cancel_->setEnabled(measuring_);
    apply_->setVisible(!measuring_ && readyApply);apply_->setEnabled(canChange && readyApply);
    representative_->setVisible(!measuring_ && !result_ready_);progress_->setVisible(measuring_);
    undo_->setEnabled(canChange && !measuring_ && !undo_state_.empty());
    if(!canChange && !measuring_){
        const QString hint=obs_frontend_replay_buffer_active() && !obs_frontend_streaming_active() && !obs_frontend_recording_active()?uiText("OBS의 다시보기 임시 저장을 멈춘 뒤 검사할 수 있어요."):uiText("방송·녹화를 끝낸 뒤 검사할 수 있어요.");
        status_->setText(hint);
    }
}
AssistantPanel::~AssistantPanel()
{
	obs_frontend_remove_event_callback(frontendEvent,this);
	timer_->stop(); suspend(); saveSettings();
}
void AssistantPanel::loadSettings()
{
	const auto saved=readJson("assistant.json");
	const std::array<QString,4> defaults{};
	for(size_t i=0;i<4;++i)source_names_[i]=saved.value(QString("source%1").arg(i)).toString(defaults[i]).toStdString();
	for(size_t i=0;i<3;++i){QSignalBlocker b(targets_[i]);targets_[i]->setValue(saved.value(QString("target%1").arg(i)).toDouble(targets_[i]->value()));}
}
void AssistantPanel::saveSettings()
{
	QJsonObject value{{"schema","trueitem-assistant.v1"}};
	for(size_t i=0;i<4;++i)value[QString("source%1").arg(i)]=QString::fromStdString(source_names_[i]);
	for(size_t i=0;i<3;++i)value[QString("target%1").arg(i)]=targets_[i]->value();
	writeJson("assistant.json",value);
}
void AssistantPanel::suspend()
{
	if(measuring_)cancelCalibration();
	for(auto &m:monitors_)m.disconnect();
	suspended_=true;
}
void AssistantPanel::frontendEvent(enum obs_frontend_event event,void *data)
{
	auto *self=static_cast<AssistantPanel *>(data);
	if(event==OBS_FRONTEND_EVENT_PROFILE_CHANGING || event==OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP || event==OBS_FRONTEND_EVENT_EXIT || event==OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN)self->suspend();
	else if(event==OBS_FRONTEND_EVENT_PROFILE_CHANGED || event==OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)QTimer::singleShot(0,self,[self]{self->refresh();});
	else if(event==OBS_FRONTEND_EVENT_STREAMING_STARTING || event==OBS_FRONTEND_EVENT_RECORDING_STARTING || event==OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING){if(self->measuring_)self->cancelCalibration();}
}
void AssistantPanel::refresh()
{
	if(measuring_)cancelCalibration();
	if(result_ready_){result_ready_=false;apply_->setEnabled(false);proposal_->setText(uiText("소스와 경로를 갱신했습니다. 다시 측정하면 제안값을 확인할 수 있습니다."));}
	const auto names=AudioCaptureManager::enumerate_audio_sources();
	for(size_t i=0;i<4;++i){
		QSignalBlocker block(selectors_[i]); selectors_[i]->clear(); selectors_[i]->addItem(uiText("선택 없음"),QString());
		for(const auto &n:names)selectors_[i]->addItem(QString::fromStdString(n),QString::fromStdString(n));
		int found=selectors_[i]->findData(QString::fromStdString(source_names_[i]));
		selectors_[i]->setCurrentIndex(found>=0?found:0);
		if(found<0)source_names_[i].clear();
	}
	if(source_names_[0].empty() || source_names_[1].empty() || source_names_[2].empty())source_toggle_->setChecked(true);
	suspended_=false; connectSources(); updateRoutes();
	updateActions();
}
void AssistantPanel::connectSources()
{
	for(size_t i=0;i<3;++i){monitors_[i].disconnect();if(!source_names_[i].empty())monitors_[i].connect_source(source_names_[i]);}
}
void AssistantPanel::updateRoutes()
{
    const auto stream=resolve_preview_track();stream_track_=stream.track;
    config_t *config=obs_frontend_get_profile_config();recording_mask_=0;record_track_=-1;
    bool supported=false;
    if(config){
        const QString mode=QString::fromUtf8(config_get_string(config,"Output","Mode"));
        const bool advanced=mode=="Advanced";
        const QString format=QString::fromUtf8(config_get_string(config,advanced?"AdvOut":"SimpleOutput","RecFormat2"));
        const QString quality=QString::fromUtf8(config_get_string(config,"SimpleOutput","RecQuality"));
        supported=advanced ? QString::fromUtf8(config_get_string(config,"AdvOut","RecType"))!="FFmpeg" : quality!="Stream" && quality!="Lossless";
        supported=supported && format!="flv";
        recording_mask_=supported ? config_get_uint(config,advanced?"AdvOut":"SimpleOutput","RecTracks") & 63 : 0;
        for(int i=0;i<6;++i)if(recording_mask_&(uint64_t(1)<<i)){record_track_=i;break;}
    }
    track_names_->setText(QString(uiText("방송 트랙 %1 · 녹화 메인 트랙 %2 (각 트랙의 실제 스테레오 출력)")).arg(stream_track_>=0?QString::number(stream_track_+1):uiText("미확인")).arg(record_track_>=0?QString::number(record_track_+1):uiText("미확인")));
    QStringList notes,streamRoles,recordRoles;bool invalid=false;
    if(stream_track_<0 || record_track_<0){notes<<uiText("녹화 소리를 나눌 준비가 필요해요. 채널 설정에서 확인해 주세요.");invalid=true;}
    std::array<std::string,3> used{};
    const std::array<QString,3> roles{uiText("목소리"),uiText("게임"),uiText("음악")};
    for(size_t i=0;i<4;++i){
        if(source_names_[i].empty()){
            if(i==0){notes<<uiText("내 목소리를 연결해 주세요.");invalid=true;}
            continue;
        }
        auto *source=obs_get_source_by_name(source_names_[i].c_str());
        if(!source){notes<<uiText("선택한 소리가 없어졌어요. 채널 설정에서 다시 골라 주세요.");invalid=true;continue;}
        const uint32_t mask=obs_source_get_audio_mixers(source);
        if(i<3){
            if(stream_track_>=0 && (mask&(1u<<stream_track_)))streamRoles<<roles[i];
            if(mask&recording_mask_)recordRoles<<roles[i];
            if(obs_source_muted(source))notes<<QString(uiText("%1: 음소거되어 있어요.")).arg(roles[i]);
            for(size_t j=0;j<i;++j)if(!source_names_[j].empty() && source_names_[j]==source_names_[i]){notes<<uiText("역할마다 서로 다른 소리를 골라 주세요.");invalid=true;break;}
        }else if(mask){notes<<uiText("방송을 듣는 앱이 출력에 들어가 있어요. 같은 소리가 반복될 수 있어요.");invalid=true;}
        obs_source_release(source);
    }
    if(notes.empty())notes<<uiText("현재 선택한 소리 배정을 확인했어요.");
    notes<<QString(uiText("생방송: %1")).arg(streamRoles.isEmpty()?uiText("선택 없음"):streamRoles.join(uiText(" · ")));
    notes<<QString(uiText("녹화 영상: %1")).arg(recordRoles.isEmpty()?uiText("선택 없음"):recordRoles.join(uiText(" · ")));
    notes<<uiText("여타 소리를 포함한 전체 목록은 채널 설정에서 확인하세요.");
    notes<<uiText("한 입력에 이미 섞인 소리는 여기서 따로 나눌 수 없어요.");
    routes_->setText(notes.join("\n"));
    route_summary_->setText(invalid?uiText("채널 설정에서 소리 연결을 확인해 주세요."):QString(uiText("주요 소리 · %1")).arg(recordRoles.isEmpty()?uiText("선택 없음"):recordRoles.join(uiText(" · "))));
    route_summary_->setToolTip(routes_->text());
}
void AssistantPanel::startCalibration()
{
	if(suspended_ || measuring_)return;
	if(!idle()){status_->setText(uiText("방송·녹화를 끝낸 뒤 소리 확인을 시작하세요. 현재 방송은 유지합니다."));return;}
	if(!representative_->isChecked()){status_->setText(uiText("사용할 소리를 평소 크기로 준비한 뒤 확인 항목을 체크해 주세요."));return;}
	updateRoutes();
	if(stream_track_<0 || record_track_<0){status_->setText(uiText("출력 트랙을 확인하지 못했습니다."));return;}
	session_ids_={};session_signatures_={};session_volumes_={};
    if(source_names_[0].empty()){status_->setText(uiText("내 목소리를 연결해 주세요."));return;}
    for(size_t i=0;i<3;++i){
        if(source_names_[i].empty())continue;
        auto *s=obs_get_source_by_name(source_names_[i].c_str());
        if(!s){status_->setText(uiText("선택한 소리가 없어졌어요. 채널 설정에서 다시 골라 주세요."));return;}
		if(obs_source_muted(s) || !obs_source_active(s)){obs_source_release(s);status_->setText(uiText("선택한 소스가 음소거되었거나 현재 장면에서 활성화되지 않았습니다."));return;}
		session_ids_[i]=uuid(s);session_volumes_[i]=obs_source_get_volume(s);session_signatures_[i]=audioSignature(s);obs_source_release(s);
	}
	if((!session_ids_[1].empty() && session_ids_[0]==session_ids_[1]) || (!session_ids_[2].empty() && session_ids_[0]==session_ids_[2]) || (!session_ids_[1].empty() && session_ids_[1]==session_ids_[2])){status_->setText(uiText("서로 다른 목소리·게임·음악 소스를 선택하세요."));return;}
	if(!monitors_[3].connect_output(size_t(stream_track_)) || !monitors_[4].connect_output(size_t(record_track_))){
		monitors_[3].disconnect();monitors_[4].disconnect();status_->setText(uiText("실제 출력 연결을 시작하지 못했습니다. OBS 오디오 상태를 확인하세요."));return;
	}
	for(auto &m:monitors_)m.reset();
	accum_={}; measured_={}; measured_at_.clear(); input_={}; plan_={}; result_ready_=false; measuring_=true;
	last_tick_=0; elapsed_.start(); progress_->setValue(0); start_->setEnabled(false);cancel_->setEnabled(true);apply_->setEnabled(false);
	for(auto *s:selectors_)s->setEnabled(false);
	recommendations_->hide();
	setStage(uiText("평소처럼 말해 주세요"),uiText("25초"),uiText("사용할 배경 소리는 계속 재생하세요."),true);
	updateActions();cancel_->setFocus(Qt::OtherFocusReason);
}
void AssistantPanel::cancelCalibration()
{
	if(!measuring_)return;
	measuring_=false;monitors_[3].disconnect();monitors_[4].disconnect();
	start_->setEnabled(true);cancel_->setEnabled(false);apply_->setEnabled(false);
	for(auto *s:selectors_)s->setEnabled(true);
	setStage(uiText("검사 취소"),uiText("다시 시작할 수 있어요"),uiText("음량은 바꾸지 않았어요."));
	updateActions();
}
void AssistantPanel::tick()
{
	if(suspended_)return;
	const auto live_voice=monitors_[0].snapshot();
	const std::array<QString,5> titles{uiText("목소리"), uiText("게임"), uiText("음악"), uiText("방송 실제 출력"), uiText("녹화 메인 실제 출력")};
	for(size_t i=0;i<5;++i){
		const auto s=(i>=3 && !measuring_)?measured_[i]:monitors_[i].snapshot();
		QString text=titles[i]+": ";
		if(i>0 && i<3 && source_names_[i].empty())text+=uiText("사용 안 함");
		else if(i>=3 && !measuring_ && measured_at_.isEmpty())text+=uiText("30초 확인에서 측정합니다");
		else if(!s.connected)text+=uiText("소스/출력 연결 대기");
		else if((!s.signal_present || (i==0 && s.rms_dbfs < -40)) && (i<3 || measuring_))text+=(i==0?uiText("말소리 대기 · "):uiText("신호 대기 · "))+number(s.peak_dbfs,"dBFS");
		else if(i>=3 && !measuring_ && !std::isfinite(s.integrated_lufs))text+=uiText("검사 중 유효한 소리가 없었습니다");
		else if(!s.ready && (i<3 || measuring_))text+=uiText("측정 준비 중 (3초)");
		else {
			text+=(i>=3 && !measuring_?number(s.integrated_lufs,"LUFS-I"):number(s.short_lufs,"LUFS-S"))+uiText(" · 피크 ")+number(s.max_peak_dbfs,"dBFS");
			if(i==0 && s.rms_dbfs>-40){const double difference=targets_[0]->value()-s.short_lufs;
				text+=std::abs(difference)<1?uiText(" · 참고 목표 근처"):QString(uiText(" · 참고 목표보다 %1dB %2")).arg(std::abs(difference),0,'f',1).arg(difference>0?uiText("작음"):uiText("큼"));}
			if((i==1 || i==2) && live_voice.ready && live_voice.rms_dbfs>-40){
				const double gap=live_voice.short_lufs-s.short_lufs;
				text+=QString(uiText(" · 목소리와 %1LU 차이")).arg(gap,0,'f',1);
				if(gap<targets_[i]->value())text+=uiText(" · 대화 중 배경음 크기 확인");
			}
			if(s.max_peak_dbfs>=-1)text+=uiText(" · 큰 순간 소리 여유가 작음");
			if(i>=3 && !measuring_)text+=uiText(" · 최근 검사 전체");
		}
		meters_[i]->setText(text);
		if(i<3){
			QString state;
			if(i>0 && source_names_[i].empty())state=uiText("사용 안 함");
            else if(!s.connected)state=uiText("연결 필요");
			else if(s.age_seconds>=1 || !s.signal_present || (i==0 && s.rms_dbfs < -40))state=i==0?uiText("말해 보세요"):uiText("재생 대기");
			else if(!s.ready)state=uiText("확인 중");
			else if(s.peak_dbfs>=-1)state=uiText("소리가 커요");
			else state=uiText("들어오는 중");
			role_states_[i]->setText(state);
			const bool signal=s.connected && s.age_seconds<1 && s.signal_present && !(i==0 && s.rms_dbfs < -40);
			level_bars_[i]->setValue(signal && std::isfinite(s.peak_dbfs)?int(std::clamp((s.peak_dbfs+60.)/60.*100.,0.,100.)):0);
			level_bars_[i]->setAccessibleDescription(state);
		}
	}
	if(!measuring_){updateActions();return;}
	if(!idle() || !sourceIdentityUnchanged()){cancelCalibration();status_->setText(uiText("출력이나 소스 설정이 바뀌어 측정을 중단했습니다. 현재 값을 유지했습니다."));return;}
	const double seconds=elapsed_.elapsed()/1000.;const double dt=std::clamp(seconds-last_tick_,0.,.5);last_tick_=seconds;
	progress_->setValue(std::min(30,int(seconds)));
	if(seconds<25){
		const int remaining=std::max(1,int(std::ceil(25-seconds)));
		phase_->setText(uiText("평소처럼 말해 주세요"));
		countdown_->setText(QString(uiText("%1초")).arg(remaining));
	}else{
		const int remaining=std::max(0,int(std::ceil(30-seconds)));
		phase_->setText(uiText("이제 말을 멈춰 주세요"));
		countdown_->setText(QString(uiText("%1초")).arg(remaining));
		countdown_->setStyleSheet("font-size:54px;font-weight:600;color:#ffdb93;");
		status_->setText(uiText("배경 소리는 그대로 두세요."));
	}
	const auto voice=monitors_[0].snapshot();
	const bool speaking=voice.ready && voice.age_seconds<1 && voice.rms_dbfs>-40 && seconds<25;
	const bool quiet=voice.connected && voice.age_seconds<1 && voice.seconds>=3 && voice.rms_dbfs<-45 && seconds>=28;
	for(size_t i=0;i<3;++i){
		const auto s=monitors_[i].snapshot();accum_[i].peak=std::max(accum_[i].peak,s.max_peak_dbfs);
		if(s.ready && speaking)accum_[i].add(s.short_lufs,dt,false);
		// Fresh digital silence after the 3s window is valid quiet evidence.
		// -100 is a conservative reporting floor, not a measured exact LUFS.
		if(i==0 && quiet)accum_[0].add(std::isfinite(s.short_lufs)?std::max(-100.,s.short_lufs):-100.,dt,true);
	}
	if(seconds>=30)finishCalibration();
}
bool AssistantPanel::sourceIdentityUnchanged() const
{
	for(size_t i=0;i<3;++i){
        if(source_names_[i].empty()){if(i==0 || !session_ids_[i].empty())return false;continue;}
		auto *s=obs_get_source_by_name(source_names_[i].c_str());if(!s)return false;
		const bool good=uuid(s)==session_ids_[i] && obs_source_active(s) && std::abs(obs_source_get_volume(s)-session_volumes_[i])<.00001 && audioSignature(s)==session_signatures_[i];
		obs_source_release(s);if(!good)return false;
	}return true;
}
void AssistantPanel::finishCalibration()
{
	measured_at_=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	input_.elapsed_seconds=std::min(30.,elapsed_.elapsed()/1000.);
	for(size_t i=0;i<5;++i)measured_[i]=monitors_[i].snapshot();
	input_.output_peak_dbfs=std::max(measured_[3].max_peak_dbfs,measured_[4].max_peak_dbfs);
	input_.voice=accum_[0].summary(measured_[0].connected);
	input_.game=accum_[1].summary(measured_[1].connected);
	input_.music=accum_[2].summary(measured_[2].connected);
	monitors_[3].disconnect();monitors_[4].disconnect();
	measuring_=false;result_ready_=true;progress_->setValue(30);cancel_->setEnabled(false);start_->setEnabled(true);
	for(auto *s:selectors_)s->setEnabled(true);
	showPlan(); saveReport();
	if(apply_->isVisible())apply_->setFocus(Qt::OtherFocusReason);else start_->setFocus(Qt::OtherFocusReason);
}
void AssistantPanel::showPlan()
{
	if(!result_ready_)return;
	input_.outputs_idle=idle();input_.context_confirmed=representative_->isChecked();
	CalibrationConfig cfg;cfg.voice_target_lufs=targets_[0]->value();cfg.game_gap_lu=targets_[1]->value();cfg.music_gap_lu=targets_[2]->value();
	plan_=make_calibration_plan(input_,cfg);
	const std::array<GainSuggestion,3> suggestions{plan_.voice,plan_.game,plan_.music};
	const std::array<QString,3> titles{uiText("목소리"), uiText("게임"), uiText("음악")};QStringList lines;
	CalibrationReason displayReason=plan_.reason;
	for(size_t i=0;i<3;++i){const auto &s=suggestions[i];
        if(source_names_[i].empty() && i>0){recommendation_labels_[i]->setText(uiText("사용 안 함"));recommendation_labels_[i]->setToolTip(QString());lines<<titles[i]+uiText(": 사용 안 함");continue;}
		lines<<titles[i]+(s.available?QString(": %1%2dB · ").arg(s.delta_db>0?"+":"").arg(s.delta_db,0,'f',1):uiText(": 유지 · "))+reason(s.reason);
		recommendation_labels_[i]->setText(!s.available || needsMoreEvidence(s.reason)?uiText("추가 확인"):std::abs(s.delta_db)<.05?uiText("그대로"):s.delta_db>0?uiText("조금 키워요"):uiText("조금 낮춰요"));
		if(displayReason==CalibrationReason::NoChangeNeeded && needsMoreEvidence(s.reason))displayReason=s.reason;
		recommendation_labels_[i]->setToolTip(QString("%1dB · %2").arg(s.delta_db,0,'f',1).arg(reason(s.reason)));
	}
	proposal_->setText(lines.join("\n"));apply_->setEnabled(plan_.can_apply && sourceIdentityUnchanged());
	recommendations_->show();
	QString hint;
	switch(displayReason){
	case CalibrationReason::Ready: case CalibrationReason::ChangeLimited: case CalibrationReason::PeakLimited:hint=uiText("적용 전 음량을 저장해요. 적용한 뒤 소리를 비교하세요.");break;
	case CalibrationReason::NoChangeNeeded:hint=uiText("이번 검사에서 바꿀 설정은 없어요.");break;
	case CalibrationReason::InsufficientSpeech:hint=uiText("말하기 안내가 보이는 동안 평소 목소리로 계속 말해 주세요.");break;
	case CalibrationReason::NoiseDominated:hint=uiText("방 소리가 크게 잡혀요. 마이크를 조금 가까이 두고 다시 검사하세요.");break;
	case CalibrationReason::MissingCapture:hint=uiText("게임과 음악을 재생하고, 연결 설정도 확인해 주세요.");break;
	case CalibrationReason::QuietEvidenceMissing:hint=uiText("마지막 안내가 나오면 잠깐 조용히 있어 주세요.");break;
	default:hint=uiText("소리를 조금 더 확인해야 해요. 세부 정보에서 이유를 볼 수 있어요.");break;
	}
	setStage(uiText("검사 완료"),plan_.can_apply?uiText("바꿔 볼 설정을 찾았어요"):displayReason==CalibrationReason::NoChangeNeeded?uiText("지금 음량을 유지해요"):uiText("한 번 더 확인해 주세요"),hint);
	updateActions();
}
void AssistantPanel::applySuggestions()
{
	if(!result_ready_ || measuring_ || applying_)return;
	showPlan();
	if(!idle() || !plan_.can_apply || !sourceIdentityUnchanged()){status_->setText(uiText("측정 후 소스/음량이 바뀌었거나 적용 조건이 부족합니다. 다시 확인해 주세요."));return;}
	const std::array<GainSuggestion,3> suggestions{plan_.voice,plan_.game,plan_.music};
	std::array<obs_source_t *,3> sources{};std::array<double,3> nextVolumes{};QJsonArray entries;
	for(size_t i=0;i<3;++i){
        if(source_names_[i].empty())continue;
		sources[i]=obs_get_source_by_name(source_names_[i].c_str());
		if(!sources[i] || uuid(sources[i])!=session_ids_[i]){for(auto *s:sources)if(s)obs_source_release(s);return;}
		const double before=obs_source_get_volume(sources[i]);const double after=before*std::pow(10.,suggestions[i].delta_db/20.);
		if(!std::isfinite(after) || after<0 || after>4){for(auto *s:sources)if(s)obs_source_release(s);status_->setText(uiText("음량 범위를 확인할 수 없어 적용하지 않았습니다."));return;}
		nextVolumes[i]=after;
        entries.append(QJsonObject{{"role",int(i)},{"name",QString::fromStdString(source_names_[i])},{"uuid",QString::fromStdString(session_ids_[i])},{"before",before},{"after",after}});
	}
	// Persist undo before changing any source; a failed disk write leaves audio untouched.
	if(!writeJson("last-adjustment.json",QJsonObject{{"schema","trueitem-adjustment.v1"},{"created_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"entries",entries}})){
		for(auto *s:sources)if(s)obs_source_release(s);status_->setText(uiText("원복 정보를 저장하지 못해 적용하지 않았습니다."));return;
	}
	applying_=true;
	for(size_t i=0;i<3;++i)if(sources[i]){obs_source_set_volume(sources[i],float(nextVolumes[i]));obs_source_release(sources[i]);}
	applying_=false;undo_state_=entries;result_ready_=false;apply_->setEnabled(false);undo_->setEnabled(true);
	obs_frontend_save();saveReport();recommendations_->hide();setStage(uiText("적용 완료"),uiText("설정을 저장했어요"),uiText("마음에 들지 않으면 ‘되돌리기’를 누르세요."));updateActions();
}
void AssistantPanel::restoreUndoState()
{
    undo_state_=readJson("last-adjustment.json")["entries"].toArray();
    bool valid=!undo_state_.isEmpty() && undo_state_.size()<=3;
    QStringList identities;
    for(const auto &item:undo_state_){const auto e=item.toObject();const QString id=e["uuid"].toString();
        if(!e["before"].isDouble() || !e["after"].isDouble() || !std::isfinite(e["before"].toDouble()) ||
           !std::isfinite(e["after"].toDouble()) || e["before"].toDouble()<0 || e["before"].toDouble()>4 ||
           e["after"].toDouble()<0 || e["after"].toDouble()>4 || e["name"].toString().isEmpty() ||
           id.isEmpty() || identities.contains(id)){valid=false;break;}
        identities<<id;
    }
    if(!valid)undo_state_={};
    undo_->setEnabled(!undo_state_.empty());
}
void AssistantPanel::undoAdjustment()
{
	if(!idle() || measuring_ || undo_state_.empty())return;
	std::vector<obs_source_t *> sources;
	for(const auto &v:undo_state_){const auto e=v.toObject();auto *s=obs_get_source_by_name(e["name"].toString().toUtf8().constData());
		if(!s || uuid(s)!=e["uuid"].toString().toStdString() || std::abs(obs_source_get_volume(s)-e["after"].toDouble())>.0001){
			if(s)obs_source_release(s);for(auto *held:sources)obs_source_release(held);
			status_->setText(uiText("보정 후 다른 음량 변경이 있어 자동 원복을 중단했습니다. 현재 설정을 덮어쓰지 않았습니다."));return;
		}sources.push_back(s);
	}
	for(size_t i=0;i<sources.size();++i){obs_source_set_volume(sources[i],float(undo_state_[int(i)].toObject()["before"].toDouble()));obs_source_release(sources[i]);}
	undo_state_={};writeJson("last-adjustment.json",QJsonObject{{"entries",undo_state_}});undo_->setEnabled(false);result_ready_=false;apply_->setEnabled(false);
	obs_frontend_save();saveReport();recommendations_->hide();setStage(uiText("되돌리기 완료"),uiText("이전 음량으로 돌아왔어요"),uiText("바뀐 소리를 다시 확인할 수 있어요."));updateActions();
}
void AssistantPanel::saveReport()
{
	QJsonArray levels,sources,calibration;
	for(size_t i=0;i<5;++i){const auto s=i>=3?measured_[i]:monitors_[i].snapshot();
		levels.append(QJsonObject{{"index",int(i)},{"short_lufs",finite(s.short_lufs)},{"integrated_lufs",finite(s.integrated_lufs)},{"max_sample_peak_dbfs",finite(s.max_peak_dbfs)},{"seconds",s.seconds},{"dropped_blocks",double(s.dropped_blocks)}});
	}
	for(size_t i=0;i<4;++i){auto *s=obs_get_source_by_name(source_names_[i].c_str());if(!s)continue;
		sources.append(QJsonObject{{"role",int(i)},{"name",QString::fromStdString(source_names_[i])},{"uuid",QString::fromStdString(uuid(s))},{"volume",obs_source_get_volume(s)},{"mixers",int(obs_source_get_audio_mixers(s))},{"muted",obs_source_muted(s)}});obs_source_release(s);}
	const std::array<CalibrationSource,3> inputs{input_.voice,input_.game,input_.music};
	const std::array<GainSuggestion,3> proposals{plan_.voice,plan_.game,plan_.music};
	for(size_t i=0;i<3;++i){const auto &s=inputs[i];const auto &p=proposals[i];
		calibration.append(QJsonObject{{"role",int(i)},{"speech_lufs",finite(s.speech_lufs)},{"speech_seconds",s.speech_seconds},
			{"quiet_lufs",finite(s.quiet_lufs)},{"quiet_seconds",s.quiet_seconds},{"sample_peak_dbfs",finite(s.peak_dbfs)},
			{"enabled",!source_names_[i].empty()},{"suggested_delta_db",p.delta_db},{"reason",source_names_[i].empty()?uiText("사용 안 함"):reason(p.reason)}});
	}
	QJsonObject report{{"schema","trueitem-measurement.v1"},{"created_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
		{"measurement_completed_at",measured_at_},
		{"audio_saved",false},{"true_peak",false},{"stream_track",stream_track_+1},{"record_main_track",record_track_+1},
		{"record_mask",double(recording_mask_)},{"measurements",levels},{"sources",sources},{"calibration",calibration},{"routing",routes_->text()},
		{"recommendations",proposal_->text()},{"valid_speech_seconds",input_.voice.speech_seconds},{"quiet_seconds",input_.voice.quiet_seconds}};
	if(!writeJson("last-measurement.json",report))status_->setText(uiText("측정 결과를 저장하지 못했습니다."));
	else if(!measuring_ && !result_ready_)status_->setText(uiText("현재 설정을 저장했어요."));
}
} // namespace lbm
