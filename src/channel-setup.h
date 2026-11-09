// CVA VEIL modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#pragma once
#include <QDialog>
#include <memory>

namespace lbm {
class ChannelSetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChannelSetupDialog(QWidget *parent = nullptr);
    ~ChannelSetupDialog() override;
public slots:
    void nextStep();
    void previousStep();
    void applySetup();
    void undoSetup();
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
} // namespace lbm
