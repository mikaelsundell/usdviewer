// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstage.h"
#include <QMap>
#include <QVariant>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>

namespace usd {
class StagePrivate : public QSharedData {
public:
    StagePrivate();
    ~StagePrivate();
    struct Data {
        UsdStageRefPtr stagePtr;
        Stage::load_type loadType;
        bool updated = false;
    };
    Data d;
};

StagePrivate::StagePrivate() { d.loadType = Stage::load_type::load_none; }

StagePrivate::~StagePrivate() {}

Stage::Stage()
    : p(new StagePrivate())
{}

Stage::Stage(const QString& filename, load_type loadType)
    : p(new StagePrivate())
{
    loadFromFile(filename, loadType);
}

Stage::Stage(const Stage& other)
    : p(other.p)
{}

Stage::~Stage() {}

bool
Stage::loadFromFile(const QString& filename, load_type loadType)
{
    if (loadType == load_type::load_all) {
        p->d.stagePtr = UsdStage::Open(filename.toStdString(), UsdStage::LoadAll);
    }
    else {
        p->d.stagePtr = UsdStage::Open(filename.toStdString(), UsdStage::LoadNone);
    }
    p->d.loadType = loadType;
    if (p->d.stagePtr) {
        p->d.updated = false;
        return true;
    }
    else {
        return false;
    }
}

bool
Stage::loadFromPath(const QList<pxr::SdfPath>& paths)
{
    if (!isValid())
        return false;

    UsdStageRefPtr stage = stagePtr();
    if (!stage)
        return false;

    Q_EMIT loadPathsSubmitted(paths);

    for (const SdfPath& path : paths) {
        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim) {
            qWarning() << "invalid prim at" << QString::fromStdString(path.GetString());
            continue;
        }
        prim.Load();
        for (const UsdPrim& child : prim.GetAllDescendants())
            child.Load();
    }

    Q_EMIT loadPathsCompleted(paths);
    return true;
    return true;
}

bool
Stage::unloadFromPath(const QList<pxr::SdfPath>& paths)
{
    if (!isValid())
        return false;

    UsdStageRefPtr stage = stagePtr();
    if (!stage)
        return false;

    Q_EMIT loadPathsSubmitted(paths);
    for (const SdfPath& path : paths) {
        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim) {
            qWarning() << "invalid prim at" << QString::fromStdString(path.GetString());
            continue;
        }
        prim.Unload();
        for (const UsdPrim& child : prim.GetAllDescendants())
            child.Unload();
    }

    Q_EMIT loadPathsCompleted(paths);
    return true;
}

bool
Stage::exportToFile(const QString& filename)
{
    return p->d.stagePtr->Export(filename.toStdString());
}

bool
Stage::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    UsdStagePopulationMask mask;
    for (const SdfPath& path : paths) {
        bool isChildOfAnother = false;
        for (const SdfPath& other : paths) {
            if (path.HasPrefix(other) && path != other) {
                isChildOfAnother = true;
                break;
            }
        }
        if (!isChildOfAnother) {
            mask.Add(path);
        }
    }
    if (mask.GetPaths().empty()) {
        return false;
    }
    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(p->d.stagePtr->GetRootLayer(), mask);
    if (!maskedStage) {
        return false;
    }
    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(filename.toStdString());
}

bool
Stage::reload()
{
    try {
        if (p->d.stagePtr) {
            p->d.stagePtr->Reload();
        }
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool
Stage::close()
{
    try {
        p->d.stagePtr = nullptr;
        p->d.updated = false;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool
Stage::isValid() const
{
    return p->d.stagePtr != nullptr;
}

GfBBox3d
Stage::boundingBox() const
{
    Q_ASSERT("stage is not set" && isValid());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    return bboxCache.ComputeWorldBound(p->d.stagePtr->GetPseudoRoot());
}

Stage::load_type
Stage::loadType() const
{
    return p->d.loadType;
}

GfBBox3d
Stage::boundingBox(const QList<SdfPath> paths) const
{
    Q_ASSERT("stage is not set" && isValid());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = p->d.stagePtr->GetPrimAtPath(path);
        if (!prim || !prim.IsA<UsdGeomImageable>()) {
            continue;
        }
        GfBBox3d worldbbox = bboxCache.ComputeWorldBound(prim);
        bbox = GfBBox3d::Combine(bbox, worldbbox);
    }
    return bbox;
}

UsdStageRefPtr
Stage::stagePtr() const
{
    Q_ASSERT("stage is not set" && isValid());
    return p->d.stagePtr;
}

Stage&
Stage::operator=(const Stage& other)
{
    if (this != &other) {
        p = other.p;
    }
    return *this;
}
}  // namespace usd
