// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QDialog>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class ProgressPrivate;
class Progress : public QDialog {
    Q_OBJECT
public:
    Progress(QWidget* parent = nullptr);
    virtual ~Progress();

public Q_SLOTS:
    void loadPathsSubmitted(const QList<SdfPath>& paths);
    void loadPathsCompleted(const QList<SdfPath>& loaded);
    void onCancel();

Q_SIGNALS:
    void cancelRequested();

private:
    QScopedPointer<ProgressPrivate> p;
};
}  // namespace usd
