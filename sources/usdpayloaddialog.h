// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdselection.h"
#include "usdstagemodel.h"
#include <QDialog>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class PayloadDialogPrivate;
class PayloadDialog : public QDialog {
    Q_OBJECT
public:
    PayloadDialog(QWidget* parent = nullptr);
    virtual ~PayloadDialog();

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    Selection* selection();
    void setSelection(Selection* selection);

public Q_SLOTS:
    void payloadsRequested(const QList<SdfPath>& paths);
    void payloadsFailed(const SdfPath& paths);
    void payloadsLoaded(const SdfPath& paths);
    void payloadsUnloaded(const SdfPath& paths);
    void cancel();

Q_SIGNALS:
    void cancelRequested();

private:
    QScopedPointer<PayloadDialogPrivate> p;
};
}  // namespace usd
