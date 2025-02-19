// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstage.h"
#include <pxr/usd/usdGeom/bboxCache.h>

namespace usd {
class StagePrivate : public QSharedData {
public:
    StagePrivate();
    ~StagePrivate();
    struct Data
    {
        UsdStageRefPtr stageptr;
        bool updated = false;
    };
    Data d;
};

StagePrivate::StagePrivate()
{
}

StagePrivate::~StagePrivate()
{
}

Stage::Stage()
: p(new StagePrivate())
{
}

Stage::Stage(const QString& filename)
: p(new StagePrivate())
{
    loadFromFile(filename);
}

Stage::Stage(const Stage& other)
: p(other.p)
{
}

Stage::~Stage()
{
}

bool
Stage::loadFromFile(const QString& filename)
{
    p->d.stageptr = UsdStage::Open(filename.toStdString());
    if (p->d.stageptr) {
        p->d.updated = false;
        return true;
    }
    else {
        return false;
    }
}

bool
Stage::isValid() const
{
    return p->d.stageptr != nullptr;
}

GfBBox3d
Stage::boundingBox() const
{
    Q_ASSERT("stage is not set" && isValid());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    return bboxCache.ComputeWorldBound(p->d.stageptr->GetPseudoRoot());
}

UsdStageRefPtr
Stage::stagePtr() const
{
    Q_ASSERT("stage is not set" && isValid());
    return p->d.stageptr;
}

Stage&
Stage::operator=(const Stage& other)
{
    if (this != &other) {
        p = other.p;
    }
    return *this;
}
}
