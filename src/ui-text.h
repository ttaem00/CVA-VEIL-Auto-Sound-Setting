// CVA VEIL 1.3.0. SPDX-License-Identifier: GPL-2.0
#pragma once
#include <QString>
class QWidget;
namespace lbm {
// Korean copy is the stable message identity and safe fallback, never an opaque key.
QString uiText(const QString &korean);
QString uiLanguage();
void setUiLanguage(const QString &language);
void translateUi(QWidget *root);
}
