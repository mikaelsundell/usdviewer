// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdstage.h"
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class ControllerPrivate;
class Controller : public QObject {
    Q_OBJECT
public:
    Controller(QObject* parent = nullptr);
    ~Controller();
    
    Stage stage() const;
    bool setStage(const Stage& stage);
    
    void visiblePaths(const QList<SdfPath>& paths, bool visible);
    void removePaths(const QList<SdfPath>& paths);

Q_SIGNALS:
    void dataChanged(const QList<SdfPath>& paths) const;
    void dataRemoved(const QList<SdfPath>& paths) const;

private:
    QScopedPointer<ControllerPrivate> p;
};
}  // namespace usd
