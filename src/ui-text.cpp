// CVA VEIL 1.3.0. SPDX-License-Identifier: GPL-2.0
#include "ui-text.h"
#include <obs-module.h>
#include <util/platform.h>
#include <QAbstractButton>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QWidget>

namespace lbm {
namespace {
QJsonObject catalog;
QString current;
bool loaded = false;
QString languagePath()
{
    char *path = obs_module_config_path("ui-language.json");
    const QString result = QString::fromUtf8(path ? path : ""); bfree(path); return result;
}
void load()
{
    if (loaded) return;
    loaded = true; current = "ko-KR";
    char *path = obs_module_file("i18n.json");
    QFile messages(QString::fromUtf8(path ? path : "")); bfree(path);
    if (messages.open(QIODevice::ReadOnly)) catalog = QJsonDocument::fromJson(messages.readAll()).object();
    QFile saved(languagePath());
    if (saved.open(QIODevice::ReadOnly)) {
        const QString language = QJsonDocument::fromJson(saved.readAll()).object()["language"].toString();
        if (language == "en-US" || language == "ja-JP") current = language;
    }
}
QString original(const QString &displayed)
{
    if (catalog.contains(displayed)) return displayed;
    for (auto it=catalog.constBegin(); it!=catalog.constEnd(); ++it) {
        const auto values=it.value().toObject();
        if (values["en-US"].toString()==displayed || values["ja-JP"].toString()==displayed) return it.key();
    }
    return displayed;
}
}
QString uiText(const QString &korean)
{
    load();
    if (current == "ko-KR") return korean;
    const QString translated=catalog.value(korean).toObject().value(current).toString();
    return translated.isEmpty() ? korean : translated;
}
QString uiLanguage() { load(); return current; }
void setUiLanguage(const QString &language)
{
    load();
    if (language!="ko-KR" && language!="en-US" && language!="ja-JP") return;
    char *dir=obs_module_config_path(nullptr); if(dir){os_mkdirs(dir);bfree(dir);}
    QSaveFile saved(languagePath());
    if(!saved.open(QIODevice::WriteOnly)) return;
    const QByteArray bytes=QJsonDocument(QJsonObject{{"language",language}}).toJson();
    if(saved.write(bytes)!=bytes.size() || !saved.commit())return;
    current=language;
}
void translateUi(QWidget *root)
{
    load(); if(!root)return;
    auto objects=root->findChildren<QObject *>(); objects.prepend(root);
    for(auto *object:objects){
        for(const char *property:{"text","windowTitle","toolTip","accessibleName","accessibleDescription"}){
            // Editable names and device/app names belong to the user, not the catalog.
            if(QByteArray(property)=="text" && !qobject_cast<QLabel *>(object) && !qobject_cast<QAbstractButton *>(object))continue;
            const QVariant value=object->property(property);
            if(value.isValid() && value.metaType().id()==QMetaType::QString){
                const QString old=value.toString();
                const QByteArray sourceName=QByteArray("_cva_i18n_")+property+"_source";
                const QByteArray lastName=QByteArray("_cva_i18n_")+property+"_last";
                // Keep message identity across languages whose short captions
                // happen to be equal; re-identify only when application state changed.
                const QString remembered=object->property(sourceName.constData()).toString();
                const QString key=!remembered.isEmpty() && object->property(lastName.constData()).toString()==old ? remembered : original(old);
                if(catalog.contains(key)){
                    const QString translated=uiText(key);
                    object->setProperty(sourceName.constData(),key);object->setProperty(lastName.constData(),translated);
                    object->setProperty(property,translated);
                }
            }
        }
        if(auto *tabs=qobject_cast<QTabWidget *>(object))for(int i=0;i<tabs->count();++i){
            const QString key=original(tabs->tabText(i)); if(catalog.contains(key))tabs->setTabText(i,uiText(key));
        }
        if(auto *combo=qobject_cast<QComboBox *>(object)){
            if(combo->objectName()=="cvaLanguage" || combo->property("_cvaUserItems").toBool() ||
               combo->objectName().startsWith("trueitemSource") || combo->objectName().startsWith("cvaRole"))continue;
            const QSignalBlocker blocker(combo);
            for(int i=0;i<combo->count();++i){
                const QString key=original(combo->itemText(i)); if(catalog.contains(key))combo->setItemText(i,uiText(key));
            }
        }
    }
}
}
