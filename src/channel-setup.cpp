// CVA VEIL modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#include "channel-setup.h"
#include "channel-routing.h"
#include "preview-track.h"
#include "ui-text.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <array>
#include <algorithm>

namespace lbm {
namespace {
QString path(const char *name) { char *p=obs_module_config_path(name);QString s=QString::fromUtf8(p?p:"");bfree(p);return s; }
QJsonObject read(const char *name) { QFile f(path(name));if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object(); }
bool write(const char *name,const QJsonObject &o) { char *dir=obs_module_config_path(nullptr);if(dir){os_mkdirs(dir);bfree(dir);}QSaveFile f(path(name));const auto bytes=QJsonDocument(o).toJson();return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit(); }
bool idle() { return !obs_frontend_streaming_active()&&!obs_frontend_recording_active()&&!obs_frontend_replay_buffer_active(); }
QString owned(char *s) { QString out=QString::fromUtf8(s?s:"");bfree(s);return out; }
QString sourceId(obs_source_t *s) { return QString::fromUtf8(obs_source_get_uuid(s)); }
QString currentScene() { auto *s=obs_frontend_get_current_scene();QString id=s?sourceId(s):QString();if(s)obs_source_release(s);return id; }
QLabel *copyLabel(const QString &s) { auto *l=new QLabel(s);l->setTextFormat(Qt::PlainText);l->setWordWrap(true);l->setMinimumWidth(0);return l; }
struct Output {
    QString mode,quality,type,format,section;
    uint32_t stream{},record{},vod{};
    QString problem;
    QJsonObject signature() const { return {{"mode",mode},{"quality",quality},{"type",type},{"format",format},{"stream",int(stream)},{"vod",int(vod)}}; }
};
Output output()
{
    Output o;auto *c=obs_frontend_get_profile_config();if(!c){o.problem="OBS의 녹화 설정을 읽을 수 없어요.";return o;}
    o.mode=QString::fromUtf8(config_get_string(c,"Output","Mode"));
    auto track=resolve_preview_track();if(track.track>=0&&track.track<6)o.stream=1u<<track.track;
    const bool adv=o.mode=="Advanced";o.section=adv?"AdvOut":"SimpleOutput";
    o.record=uint32_t(config_get_uint(c,o.section.toUtf8().constData(),"RecTracks"))&63u;
    o.type=adv?QString::fromUtf8(config_get_string(c,"AdvOut","RecType")):"Standard";
    o.quality=adv?QString():QString::fromUtf8(config_get_string(c,"SimpleOutput","RecQuality"));
    o.format=QString::fromUtf8(config_get_string(c,o.section.toUtf8().constData(),"RecFormat2"));
    if(o.format.isEmpty())o.format=QString::fromUtf8(config_get_string(c,o.section.toUtf8().constData(),"RecFormat"));
    if(adv&&config_get_bool(c,"AdvOut","VodTrackEnabled")){auto v=config_get_uint(c,"AdvOut","VodTrackIndex");if(v>=1&&v<=6)o.vod=1u<<(v-1);}
    const QSet<QString> formats{"mkv","mp4","hybrid_mp4","fragmented_mp4","mov","hybrid_mov","fragmented_mov"};
    if(!o.stream)o.problem="생방송의 소리 연결을 확인해 주세요.";
    else if(!adv&&o.quality=="Stream")o.problem="OBS의 녹화 화질을 ‘높은 화질’로 바꾸면 소리를 나눌 수 있어요.";
    else if((!adv&&o.quality=="Lossless")||(adv&&!o.type.isEmpty()&&o.type!="Standard")||!formats.contains(o.format))o.problem="이 녹화 방식은 아직 지원하지 않아요. OBS에서 일반 녹화와 MKV 형식을 선택해 주세요.";
    return o;
}
bool setRecording(const Output &o,uint32_t mask)
{
    auto *c=obs_frontend_get_profile_config();if(!c||!idle())return false;
    const std::array<obs_output_t *,2> outputs{obs_frontend_get_recording_output(),obs_frontend_get_replay_buffer_output()};
    auto release=[&]{for(auto *out:outputs)if(out)obs_output_release(out);};
    for(auto *out:outputs)if(out&&obs_output_active(out)){release();return false;}
    config_set_uint(c,o.section.toUtf8().constData(),"RecTracks",mask);
    bool ok=config_save_safe(c,"tmp",nullptr)==0;
    if(ok){
        size_t count=0;for(uint32_t selected=mask&63u;selected;selected>>=1)count+=selected&1u;
        // OBS 33 SetupRecording reconnects selected slots densely but retains
        // old trailing slots when the track count shrinks. Clear only those
        // idle cached slots; OBS reconnects the selected encoders at next start.
        for(auto *out:outputs)if(out)for(size_t i=count;i<6;++i){
            if(obs_output_get_audio_encoder(out,i))obs_output_set_audio_encoder(out,nullptr,i);
            ok=ok&&!obs_output_get_audio_encoder(out,i);
        }
    }
    release();return ok;
}
struct Item {
    QString id,name,type,key,value;
    uint32_t before{};
    bool draft{},muted{},active{};
    QCheckBox *stream{},*record{};
};
QVector<Item> inputs()
{
    QVector<Item> rows;
    auto add=[](void *p,obs_source_t *s){auto &list=*static_cast<QVector<Item> *>(p);if(!(obs_source_get_output_flags(s)&OBS_SOURCE_AUDIO))return true;
        const QString id=sourceId(s);for(const auto &r:list)if(r.id==id)return true;
        Item r;r.id=id;r.name=QString::fromUtf8(obs_source_get_name(s));r.type=QString::fromUtf8(obs_source_get_id(s));r.before=obs_source_get_audio_mixers(s);r.muted=obs_source_muted(s);r.active=obs_source_active(s);list.push_back(r);return true;};
    obs_enum_sources(add,&rows);
    for(uint32_t i=0;i<6;++i){auto *s=obs_get_output_source(i);if(s){add(&rows,s);obs_source_release(s);}}
    std::sort(rows.begin(),rows.end(),[](const Item&a,const Item&b){return a.name.localeAwareCompare(b.name)<0;});return rows;
}
QJsonObject context() { return {{"profile",owned(obs_frontend_get_current_profile())},{"profile_path",owned(obs_frontend_get_current_profile_path())},{"collection",owned(obs_frontend_get_current_scene_collection())},{"scene",currentScene()},{"output",output().signature()}}; }
QString role(int i) { return uiText(i==0?"내 목소리":i==1?"게임":i==2?"음악":i==3?"방송 듣기":"여타 소리"); }
}

struct ChannelSetupDialog::Impl {
    ChannelSetupDialog *q;
    QVector<Item> rows;
    QJsonObject initialContext,assistantBefore;
    Output initial;
    uint32_t recAfter{},main{};
    QStackedWidget *pages{};
    QLabel *heading{},*message{},*summary{},*status{};
    std::array<QComboBox *,4> selectors{};
    QTableWidget *table{};
    QPushButton *back{},*next{},*undo{},*cancel{};
    int step{};
    bool applied{};
    explicit Impl(ChannelSetupDialog *owner):q(owner){}
    int category(const QString &id) const { for(int i=0;i<4;++i)if(!id.isEmpty()&&selectors[i]->currentData().toString()==id)return i;return 4; }
    QString selectedName(int i) const {auto id=selectors[i]->currentData().toString();for(const auto&r:rows)if(r.id==id)return r.name;return {};}
    QString selectedId(int i) const {return selectors[i]->currentData().toString();}
    void showMessage(const QString &s) {status->setText(uiText(s));}
    void refreshButtons() {
        back->setVisible(step>0&&step<3);next->setText(uiText(step==0?"다음":step==1?"바뀔 내용 보기":step==2?"이 설정 적용":"닫기"));
        cancel->setVisible(step!=3);
        bool ready=idle();next->setEnabled(step==3||ready);undo->setEnabled(ready&&read("channel-state.json").value("status")=="applied");
        if(!ready&&step<3)showMessage("방송과 녹화를 끝낸 뒤 설정할 수 있어요.");
    }
    bool unchanged(bool rowsToo=true) {
        if(!idle()){showMessage("방송과 녹화를 끝낸 뒤 설정할 수 있어요.");return false;}
        if(context()!=initialContext||output().record!=initial.record||read("assistant.json")!=assistantBefore){showMessage("설정이 바뀌었어요. 이 창을 닫고 다시 열어 주세요.");return false;}
        if(rowsToo){const auto now=inputs();int count=0;for(const auto&r:rows)if(!r.draft){++count;auto it=std::find_if(now.begin(),now.end(),[&](const Item&n){return n.id==r.id;});if(it==now.end()||it->before!=r.before||it->muted!=r.muted){showMessage("소리 연결이 바뀌었어요. 이 창을 닫고 다시 열어 주세요.");return false;}}if(now.size()!=count){showMessage("소리 목록이 바뀌었어요. 이 창을 닫고 다시 열어 주세요.");return false;}}
        return true;
    }
    bool selectionValid() {
        if(selectedId(0).isEmpty()){showMessage("내 목소리를 연결해 주세요.");return false;}
        QSet<QString> used;for(int i=0;i<4;++i){const auto id=selectedId(i);if(id.isEmpty())continue;if(used.contains(id)){showMessage("같은 소리를 두 번 고를 수 없어요.");return false;}used.insert(id);}
        return true;
    }
    void fillSelectors() {
        std::array<QString,4> old;for(int i=0;i<4;++i)old[i]=selectedId(i);
        for(int i=0;i<4;++i){selectors[i]->clear();selectors[i]->addItem(uiText(i==0?"목소리 선택":"사용 안 함"),QString());for(const auto&r:rows)selectors[i]->addItem(r.name,r.id);selectors[i]->setCurrentIndex(std::max(0,selectors[i]->findData(old[i])));}
    }
    void rebuildTable() {
        table->setRowCount(0);table->setRowCount(rows.size());
        for(int i=0;i<rows.size();++i){auto &r=rows[i];int cat=category(r.id);QString title=role(cat)+"\n"+r.name;if(r.muted)title+=" · "+uiText("음소거됨");
            auto *text=new QTableWidgetItem(title);text->setFlags(Qt::ItemIsEnabled);text->setToolTip(r.name);table->setItem(i,0,text);
            r.stream=new QCheckBox();r.stream->setObjectName(QString("cvaStream%1").arg(i));r.stream->setAccessibleName(uiText("%1 생방송 포함").arg(r.name));
            r.record=new QCheckBox();r.record->setObjectName(QString("cvaRecord%1").arg(i));r.record->setAccessibleName(uiText("%1 녹화 포함").arg(r.name));
            r.stream->setChecked(r.draft?cat!=3:bool(r.before&initial.stream));r.record->setChecked(r.draft?cat!=2&&cat!=3:bool(r.before&initial.record));
            if(cat==2&&assistantBefore.value("source2").toString().isEmpty())r.record->setChecked(false);
            if(cat==3){r.stream->setChecked(false);r.record->setChecked(false);r.stream->setEnabled(false);r.record->setEnabled(false);}
            table->setCellWidget(i,1,r.stream);table->setCellWidget(i,2,r.record);table->setRowHeight(i,58);
        }
    }
    uint32_t afterMask(const Item &r) const {return category(r.id)==3?0u:routedMask({r.before,r.stream->isChecked(),r.record->isChecked()},initial.stream,recAfter,main);}
    void preview() {
        QStringList streaming,recording,excluded;QJsonArray assignments;
        for(const auto&r:rows){const uint32_t m=afterMask(r);const QString name=role(category(r.id))+" · "+r.name;
            if(m&initial.stream)streaming<<name;
            if(m&recAfter)recording<<name;else excluded<<name;
        }
        summary->setText(uiText("생방송에 담겨요")+"\n"+(streaming.isEmpty()?uiText("없음"):streaming.join("\n"))+"\n\n"+uiText("녹화 영상에 담겨요")+"\n"+(recording.isEmpty()?uiText("없음"):recording.join("\n"))+"\n\n"+uiText("녹화에서 빠져요")+"\n"+(excluded.isEmpty()?uiText("없음"):excluded.join("\n"))+"\n\n"+uiText("이 소리를 쓰는 다른 장면에도 적용돼요. 음량과 음질 효과는 유지해요."));
        summary->setToolTip(uiText("녹화에 넣지 않은 소리는 편집용 소리 묶음에서도 빠져요. 이미 한 입력에 섞인 소리는 따로 나눌 수 없어요."));
    }
    void addInput() {
        QDialog dialog(q);dialog.setWindowTitle(uiText("소리 추가"));auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout();
        auto *kind=new QComboBox();kind->addItem(uiText("마이크 또는 오디오 장치"),"wasapi_input_capture");kind->addItem(uiText("앱에서 나는 소리"),"wasapi_process_output_capture");
        auto *list=new QComboBox();list->setProperty("_cvaUserItems",true);auto *destination=new QComboBox();for(int i=0;i<3;++i)destination->addItem(role(i),i);destination->addItem(role(4),4);
        auto *name=new QLineEdit(uiText("새 마이크"));auto *hint=copyLabel(uiText("앱이 보이지 않으면 먼저 앱을 열고 소리를 재생해 주세요."));
        form->addRow(uiText("무슨 소리인가요?"),kind);form->addRow(uiText("어디서 가져올까요?"),list);form->addRow(uiText("소리 종류"),destination);form->addRow(uiText("알아보기 쉬운 이름"),name);layout->addLayout(form);layout->addWidget(hint);
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(uiText("이 소리 사용"));buttons->button(QDialogButtonBox::Cancel)->setText(uiText("취소"));layout->addWidget(buttons);
        auto populate=[&]{list->clear();const QByteArray id=kind->currentData().toString().toUtf8();const char *available=nullptr;bool found=false;for(size_t i=0;obs_enum_input_types(i,&available);++i)if(id==available)found=true;
            if(found){auto *props=obs_get_source_properties(id.constData());if(props){auto *p=obs_properties_get(props,id=="wasapi_input_capture"?"device_id":"window");if(p)for(size_t i=0;i<obs_property_list_item_count(p);++i){if(obs_property_list_item_disabled(p,i))continue;const char *value=obs_property_list_item_string(p,i);if(value&&*value)list->addItem(QString::fromUtf8(obs_property_list_item_name(p,i)),QString::fromUtf8(value));}obs_properties_destroy(props);}}
            buttons->button(QDialogButtonBox::Ok)->setEnabled(list->count()>0);name->setText(uiText(id=="wasapi_input_capture"?"새 마이크":"새 앱 소리"));destination->setCurrentIndex(id=="wasapi_input_capture"?0:2);
            if(list->count()==0)hint->setText(uiText("선택할 소리가 없어요. 장치를 연결하거나 앱을 연 뒤 다시 추가해 주세요."));else hint->setText(uiText("같은 브라우저의 여러 탭은 함께 잡힐 수 있어요. 음악과 방송을 듣는 앱은 다르게 사용해 주세요."));};
        QObject::connect(kind,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int){populate();});populate();QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.resize(520,360);
        if(dialog.exec()!=QDialog::Accepted)return;
        Item r;r.draft=true;r.id="draft:"+QUuid::createUuid().toString(QUuid::WithoutBraces);r.type=kind->currentData().toString();r.key=r.type=="wasapi_input_capture"?"device_id":"window";r.value=list->currentData().toString();r.name=name->text().trimmed();if(r.name.isEmpty())r.name=uiText("새 소리");
        const QString base="CVA · "+r.name;r.name=base;for(int suffix=2;;++suffix){bool used=false;for(const auto&old:rows)if(old.name==r.name)used=true;auto *existing=obs_get_source_by_name(r.name.toUtf8().constData());if(existing){used=true;obs_source_release(existing);}if(!used)break;r.name=base+QString(" %1").arg(suffix);}
        rows.push_back(r);fillSelectors();const int roleIndex=destination->currentData().toInt();if(roleIndex<3)selectors[roleIndex]->setCurrentIndex(selectors[roleIndex]->findData(r.id));showMessage("추가한 소리는 마지막에 적용하면 연결돼요.");
    }
};

ChannelSetupDialog::ChannelSetupDialog(QWidget *parent):QDialog(parent),d(std::make_unique<Impl>(this))
{
    setObjectName("cvaChannelSetup");setWindowTitle(uiText("채널 설정 · 소리 나누기"));setMinimumSize(480,500);resize(620,710);
    setStyleSheet("QDialog#cvaChannelSetup{background:#15191e;color:#f1f4f6;} QLabel{color:#f1f4f6;} QPushButton{min-height:36px;padding:4px 14px;} QPushButton[primary=true]{background:#98ead1;color:#102a23;border:1px solid #98ead1;border-radius:6px;} QPushButton:focus,QComboBox:focus,QCheckBox:focus{border:2px solid #9fc9ff;}");
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(22,20,22,20);layout->setSpacing(16);d->heading=copyLabel(uiText("소리 연결하기"));d->heading->setStyleSheet("font-size:23px;font-weight:600");d->heading->setObjectName("cvaSetupHeading");layout->addWidget(d->heading);
    d->message=copyLabel(uiText("목소리·게임·음악이 각각 어디서 들어오는지 골라 주세요."));layout->addWidget(d->message);d->pages=new QStackedWidget();layout->addWidget(d->pages,1);
    auto *connections=new QWidget();auto *form=new QFormLayout(connections);form->setSpacing(16);
    for(int i=0;i<4;++i){d->selectors[i]=new QComboBox();d->selectors[i]->setObjectName(QString("cvaRole%1").arg(i));d->selectors[i]->setAccessibleName(role(i));d->selectors[i]->setMinimumWidth(0);d->selectors[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);form->addRow(role(i),d->selectors[i]);}
    auto *add=new QPushButton(uiText("소리 추가"));add->setObjectName("cvaAddInput");form->addRow(add);connect(add,&QPushButton::clicked,this,[this]{d->addInput();});
    form->addRow(copyLabel(uiText("여타 소리는 다음 화면에서 함께 고를 수 있어요. 이미 섞인 게임컴·장치 소리는 여기서 따로 나눌 수 없어요.")));d->pages->addWidget(connections);
    d->table=new QTableWidget();d->table->setObjectName("cvaRoutingTable");d->table->setColumnCount(3);d->table->setHorizontalHeaderLabels({uiText("소리"),uiText("생방송"),uiText("녹화 영상")});d->table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);d->table->setColumnWidth(1,90);d->table->setColumnWidth(2,100);d->table->verticalHeader()->hide();d->table->setSelectionMode(QAbstractItemView::NoSelection);d->table->setEditTriggers(QAbstractItemView::NoEditTriggers);d->pages->addWidget(d->table);
    auto *review=new QWidget();auto *reviewLayout=new QVBoxLayout(review);auto *scroll=new QScrollArea();scroll->setWidgetResizable(true);d->summary=copyLabel("");d->summary->setObjectName("cvaRoutingPreview");d->summary->setMargin(10);scroll->setWidget(d->summary);reviewLayout->addWidget(scroll);d->pages->addWidget(review);
    auto *done=copyLabel(uiText("설정을 저장했어요. 짧게 녹화해서 소리를 확인해 보세요."));done->setObjectName("cvaSetupDone");d->pages->addWidget(done);
    d->status=copyLabel("");d->status->setObjectName("cvaSetupStatus");layout->addWidget(d->status);
    auto *buttons=new QHBoxLayout();d->undo=new QPushButton(uiText("이전 배정 복원"));d->undo->setObjectName("cvaRoutingUndo");buttons->addWidget(d->undo);connect(d->undo,&QPushButton::clicked,this,&ChannelSetupDialog::undoSetup);buttons->addStretch();d->back=new QPushButton(uiText("이전"));buttons->addWidget(d->back);d->next=new QPushButton(uiText("다음"));d->next->setObjectName("cvaSetupNext");d->next->setProperty("primary",true);buttons->addWidget(d->next);auto *cancel=new QPushButton(uiText("취소"));d->cancel=cancel;buttons->addWidget(cancel);layout->addLayout(buttons);
    connect(cancel,&QPushButton::clicked,this,[this]{if(d->applied)accept();else reject();});connect(d->next,&QPushButton::clicked,this,&ChannelSetupDialog::nextStep);connect(d->back,&QPushButton::clicked,this,&ChannelSetupDialog::previousStep);
    d->initial=output();d->initialContext=context();d->assistantBefore=read("assistant.json");d->rows=inputs();d->fillSelectors();
    for(int i=0;i<4;++i){const auto name=d->assistantBefore.value(QString("source%1").arg(i)).toString();for(const auto&r:d->rows)if(r.name==name)d->selectors[i]->setCurrentIndex(d->selectors[i]->findData(r.id));}
    if(!d->initial.problem.isEmpty())d->showMessage(d->initial.problem);d->refreshButtons();auto *timer=new QTimer(this);timer->setInterval(500);connect(timer,&QTimer::timeout,this,[this]{d->refreshButtons();});timer->start();
}
ChannelSetupDialog::~ChannelSetupDialog()=default;
void ChannelSetupDialog::nextStep()
{
    if(d->step==3){accept();return;}if(!d->unchanged()||!d->selectionValid())return;
    if(!d->initial.problem.isEmpty()){d->showMessage(d->initial.problem);return;}
    d->status->clear();
    if(d->step==0){d->recAfter=distinctRecordingMask(d->initial.record,d->initial.stream,d->initial.vod);d->main=firstTrack(d->recAfter);if(!d->main){d->showMessage("녹화에 쓸 소리 묶음이 없어요. OBS 출력 설정을 확인해 주세요.");return;}d->rebuildTable();d->step=1;d->heading->setText(uiText("어디에 담을까요?"));d->message->setText(uiText("체크한 소리만 담겨요. 음악은 녹화에서 뺄 수 있어요."));}
    else if(d->step==1){d->preview();d->step=2;d->heading->setText(uiText("바뀔 내용 확인"));d->message->setText(uiText("포함될 소리를 확인한 뒤 적용해 주세요."));}
    else {applySetup();return;}d->pages->setCurrentIndex(d->step);d->refreshButtons();
}
void ChannelSetupDialog::previousStep() { if(d->step<=0||d->step>=3)return;--d->step;d->pages->setCurrentIndex(d->step);d->heading->setText(uiText(d->step==0?"소리 연결하기":"어디에 담을까요?"));d->message->setText(uiText(d->step==0?"목소리·게임·음악이 각각 어디서 들어오는지 골라 주세요.":"체크한 소리만 담겨요. 음악은 녹화에서 뺄 수 있어요."));d->status->clear();d->refreshButtons(); }

void ChannelSetupDialog::applySetup()
{
    if(d->step!=2||!d->unchanged()||!d->selectionValid())return;
    QJsonArray entries;QVector<obs_source_t *> refs;QVector<obs_source_t *> created;
    auto release=[&]{for(auto*s:refs)obs_source_release(s);refs.clear();};
    const QJsonObject previousState=read("channel-state.json");
    auto rollback=[&]{for(int i=0;i<refs.size();++i)obs_source_set_audio_mixers(refs[i],d->rows[i].before);bool restored=setRecording(d->initial,d->initial.record);for(auto*s:created)obs_source_remove(s);restored=write("assistant.json",d->assistantBefore)&&restored;restored=write("channel-state.json",previousState)&&restored;restored=restored&&output().record==d->initial.record;for(int i=0;i<refs.size();++i)restored=restored&&obs_source_get_audio_mixers(refs[i])==d->rows[i].before;obs_frontend_save();release();d->showMessage(restored?"저장하지 못해 이전 설정으로 돌렸어요.":"복원을 확인하지 못했어요. 방송을 시작하기 전에 소리 연결을 다시 확인해 주세요.");};
    // Establish a writable journal before creating a source or touching routes.
    QJsonObject state{{"schema","cva-channel-setup.v1"},{"status","prepared"},{"context",d->initialContext},{"record_before",int(d->initial.record)},{"record_after",int(d->recAfter)},{"assistant_before",d->assistantBefore}};
    if(!write("channel-state.json",state)){d->showMessage("설정을 저장할 수 없어 변경하지 않았어요.");return;}
    auto *sceneSource=obs_frontend_get_current_scene();auto *scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;
    for(auto&r:d->rows){obs_source_t *s=nullptr;if(r.draft){auto*settings=obs_data_create();obs_data_set_string(settings,r.key.toUtf8().constData(),r.value.toUtf8().constData());if(r.type=="wasapi_process_output_capture")obs_data_set_int(settings,"priority",2);s=obs_source_create(r.type.toUtf8().constData(),r.name.toUtf8().constData(),settings,nullptr);obs_data_release(settings);if(s){obs_source_set_audio_mixers(s,0);created.push_back(s);if(!scene||!obs_scene_add(scene,s)){obs_source_remove(s);obs_source_release(s);s=nullptr;created.removeLast();}}}else s=obs_get_source_by_uuid(r.id.toUtf8().constData());
        if(!s){if(sceneSource)obs_source_release(sceneSource);rollback();return;}refs.push_back(s);
        entries.append(QJsonObject{{"uuid",sourceId(s)},{"name",r.name},{"before",int(r.before)},{"after",int(d->afterMask(r))},{"created",r.draft},{"muted",r.muted}});
    }
    if(sceneSource)obs_source_release(sceneSource);
    QJsonObject assistant=d->assistantBefore;assistant["schema"]="trueitem-assistant.v1";for(int i=0;i<4;++i)assistant[QString("source%1").arg(i)]=d->selectedName(i);
    state["entries"]=entries;state["assistant_after"]=assistant;state["status"]="applied";
    if(!write("channel-state.json",state)||!write("assistant.json",assistant)){rollback();return;}
    if(!setRecording(d->initial,d->recAfter)){rollback();return;}
    for(int i=0;i<refs.size();++i)obs_source_set_audio_mixers(refs[i],uint32_t(entries[i].toObject()["after"].toInt()));
    bool ok=output().record==d->recAfter;for(int i=0;i<refs.size();++i)ok=ok&&obs_source_get_audio_mixers(refs[i])==uint32_t(entries[i].toObject()["after"].toInt());
    if(!ok){rollback();return;}obs_frontend_save();release();d->applied=true;d->step=3;d->pages->setCurrentIndex(3);d->heading->setText(uiText("설정을 마쳤어요"));d->message->setText(uiText("다음 녹화부터 선택한 소리가 담겨요."));d->status->clear();d->refreshButtons();emit accepted();
}
void ChannelSetupDialog::undoSetup()
{
    auto state=read("channel-state.json");if(state.value("status")!="applied")return;
    auto savedContext=state.value("context").toObject();if(!idle()||context()!=savedContext||output().record!=uint32_t(state["record_after"].toInt())||read("assistant.json")!=state["assistant_after"].toObject()){d->showMessage("설정이 바뀌어 되돌릴 수 없어요. 이 화면에서 다시 골라 주세요.");return;}
    const auto entries=state["entries"].toArray();QVector<obs_source_t *> refs;bool ok=true;
    for(const auto&e:entries){auto item=e.toObject();auto*s=obs_get_source_by_uuid(item["uuid"].toString().toUtf8().constData());if(!s){ok=false;break;}refs.push_back(s);if(obs_source_get_audio_mixers(s)!=uint32_t(item["after"].toInt())||obs_source_muted(s)!=item["muted"].toBool()){ok=false;break;}}
    if(!ok){for(auto*s:refs)obs_source_release(s);d->showMessage("소리 연결이 바뀌어 이전 설정을 덮어쓰지 않았어요.");return;}
    const auto currentAssistant=read("assistant.json");auto undone=state;undone["status"]="undone";
    if(!write("channel-state.json",undone)||!write("assistant.json",state["assistant_before"].toObject())){write("channel-state.json",state);write("assistant.json",currentAssistant);for(auto*s:refs)obs_source_release(s);d->showMessage("설정을 저장할 수 없어 변경하지 않았어요.");return;}
    const Output current=output();if(!setRecording(current,uint32_t(state["record_before"].toInt()))){setRecording(current,uint32_t(state["record_after"].toInt()));write("channel-state.json",state);write("assistant.json",currentAssistant);for(auto*s:refs)obs_source_release(s);d->showMessage("저장하지 못해 이전 설정으로 돌렸어요.");return;}
    for(int i=0;i<refs.size();++i){obs_source_set_audio_mixers(refs[i],uint32_t(entries[i].toObject()["before"].toInt()));obs_source_release(refs[i]);}
    // Newly created inputs remain available, with their original zero routing.
    // They are never removed after the user could have reused them elsewhere.
    obs_frontend_save();d->applied=true;d->step=3;d->pages->setCurrentIndex(3);d->heading->setText(uiText("이전 배정을 복원했어요"));d->message->setText(uiText("추가한 입력은 보관하고, 방송·녹화 배정은 이전으로 돌렸어요."));d->status->clear();d->refreshButtons();emit accepted();
}
} // namespace lbm
