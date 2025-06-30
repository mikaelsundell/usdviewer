// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdcontroller.h"
#include <QList>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <QDebug>

namespace usd {
class ControllerPrivate {
public:
    ControllerPrivate();
    ~ControllerPrivate();
    struct Data {
        Stage stage;
    };
    Data d;
};

ControllerPrivate::ControllerPrivate() {}

ControllerPrivate::~ControllerPrivate() {}

Controller::Controller(QObject* parent)
    : QObject(parent)
    , p(new ControllerPrivate())
{}

Controller::~Controller() {}

Stage
Controller::stage() const
{
    Q_ASSERT("stage is not set" && p->d.stage.isValid());
    return p->d.stage;
}

bool
Controller::setStage(const Stage& stage)
{
    p->d.stage = stage;
    return true;
}

void
Controller::visiblePaths(const QList<SdfPath>& paths, bool visible)
{
    for (const SdfPath& path : paths) {
        UsdPrim prim = p->d.stage.stagePtr()->GetPrimAtPath(path);
        UsdGeomImageable imageable(prim);
        if (visible) {
            imageable.MakeVisible();
        } else {
            imageable.MakeInvisible();
        }
    }
    dataChanged(paths);
}

void
Controller::removePaths(const QList<SdfPath>& paths)
{
    qDebug() << "remove paths ...";
}

}  // namespace usd
