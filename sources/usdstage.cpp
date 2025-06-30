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
        UsdStageRefPtr stageptr;
        QMap<QString, QVariant> metadata;
        bool updated = false;
    };
    Data d;
};

StagePrivate::StagePrivate() {}

StagePrivate::~StagePrivate() {}

Stage::Stage()
    : p(new StagePrivate())
{}

Stage::Stage(const QString& filename)
    : p(new StagePrivate())
{
    loadFromFile(filename);
}

Stage::Stage(const Stage& other)
    : p(other.p)
{}

Stage::~Stage() {}

bool
Stage::loadFromFile(const QString& filename)
{
    p->d.stageptr = UsdStage::Open(filename.toStdString());
    if (p->d.stageptr) {
        p->d.metadata.clear();
        p->d.metadata["metersPerUnit"] = UsdGeomGetStageMetersPerUnit(p->d.stageptr);;
        p->d.metadata["upAxis"] = QString::fromStdString(UsdGeomGetStageUpAxis(p->d.stageptr).GetString());
        p->d.metadata["hasAuthoredTimeCodeRange"] = p->d.stageptr->HasAuthoredTimeCodeRange();
        p->d.metadata["startTimeCode"] = p->d.stageptr->GetStartTimeCode();
        p->d.metadata["endTimeCode"] = p->d.stageptr->GetEndTimeCode();
        p->d.metadata["timeCodesPerSecond"] = p->d.stageptr->GetTimeCodesPerSecond();
        p->d.updated = false;
        return true;
    }
    else {
        return false;
    }
}

bool
Stage::exportToFile(const QString& filename)
{
    return p->d.stageptr->Export(filename.toStdString());
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
    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(p->d.stageptr->GetRootLayer(), mask);
    if (!maskedStage) {
        return false;
    }
    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(filename.toStdString());
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

GfBBox3d
Stage::boundingBox(const QList<SdfPath> paths) const
{
    Q_ASSERT("stage is not set" && isValid());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = p->d.stageptr->GetPrimAtPath(path);
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
}  // namespace usd
