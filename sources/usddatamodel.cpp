// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usddatamodel.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>

namespace usd {
class DataModelPrivate : public QSharedData {
public:
    DataModelPrivate();
    ~DataModelPrivate();
    struct Data {
        UsdStageRefPtr stage;
        DataModel::load_type loadType;
        QThreadPool pool;
    };
    Data d;
};

DataModelPrivate::DataModelPrivate()
{
    d.loadType = DataModel::load_type::load_none;
    d.pool.setMaxThreadCount(4 /*QThread::idealThreadCount()*/);
}

DataModelPrivate::~DataModelPrivate() {}

DataModel::DataModel()
    : p(new DataModelPrivate())
{}

DataModel::DataModel(const QString& filename, load_type loadType)
    : p(new DataModelPrivate())
{
    loadFromFile(filename, loadType);
}

DataModel::DataModel(const DataModel& other)
    : p(other.p)
{}

DataModel::~DataModel() {}

bool
DataModel::loadFromFile(const QString& filename, load_type loadType)
{
    if (loadType == load_type::load_all) {
        p->d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadAll);
    }
    else {
        p->d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadNone);
    }
    p->d.loadType = loadType;
    if (p->d.stage) {
        Q_EMIT stageChanged();
        return true;
    }
    else {
        return false;
    }
}

bool
DataModel::loadFromPaths(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT loadPathsSubmitted(paths);

    auto stage = p->d.stage;
    auto pool = &p->d.pool;
    QFuture<void> future = QtConcurrent::run(pool, [this, stage, paths]() {
        QList<SdfPath> failed;
        QList<SdfPath> loaded;

        for (const SdfPath& path : paths) {
            UsdPrim prim = p->d.stage->GetPrimAtPath(path);
            if (!prim) {
                failed.append(path);
                continue;
            }

            if (prim.IsLoaded()) {
                loaded.append(path);
                Q_EMIT loadPathCompleted(path);
                continue;
            }

            try {
                prim.Load();
                loaded.append(path);
                Q_EMIT loadPathCompleted(path);
            } catch (const std::exception& e) {
                qWarning() << "failed to load prim:" << QString::fromStdString(path.GetString())
                           << "error:" << e.what();
                Q_EMIT loadPathFailed(path);
                failed.append(path);
            }
        }
        QMetaObject::invokeMethod(
            this,
            [this, loaded]() {
                if (!loaded.isEmpty())
                    Q_EMIT primsChanged(loaded);
            },
            Qt::QueuedConnection);
    });
    return true;
}

bool
DataModel::unloadFromPath(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT unloadPathsSubmitted(paths);

    auto stage = p->d.stage;
    auto pool = &p->d.pool;
    QFuture<void> future = QtConcurrent::run(pool, [this, stage, paths]() {
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (!prim)
                continue;

            prim.Unload();
            for (const UsdPrim& child : prim.GetAllDescendants())
                child.Unload();

            Q_EMIT unloadPathCompleted(path);
        }
        QMetaObject::invokeMethod(
            this, [this, paths]() { Q_EMIT primsChanged(paths); }, Qt::QueuedConnection);
    });
    return true;
}

void
DataModel::visibleFromPaths(const QList<SdfPath>& paths, bool visible)
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    for (const SdfPath& path : paths) {
        UsdPrim prim = p->d.stage->GetPrimAtPath(path);
        UsdGeomImageable imageable(prim);
        if (visible) {
            imageable.MakeVisible();
        }
        else {
            imageable.MakeInvisible();
        }
    }
    Q_EMIT primsChanged(paths);
}

bool
DataModel::exportToFile(const QString& filename)
{
    return p->d.stage->Export(filename.toStdString());
}

bool
DataModel::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
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
    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(p->d.stage->GetRootLayer(), mask);
    if (!maskedStage) {
        return false;
    }
    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(filename.toStdString());
}

bool
DataModel::reload()
{
    try {
        if (p->d.stage) {
            p->d.stage->Reload();
            Q_EMIT stageChanged();
        }
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool
DataModel::close()
{
    try {
        p->d.stage = nullptr;
        Q_EMIT stageChanged();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool
DataModel::isLoaded() const
{
    return p->d.stage != nullptr;
}

GfBBox3d
DataModel::boundingBox() const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    return bboxCache.ComputeWorldBound(p->d.stage->GetPseudoRoot());
}

DataModel::load_type
DataModel::loadType() const
{
    return p->d.loadType;
}

GfBBox3d
DataModel::boundingBox(const QList<SdfPath> paths) const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = p->d.stage->GetPrimAtPath(path);
        if (!prim || !prim.IsA<UsdGeomImageable>()) {
            continue;
        }
        GfBBox3d worldbbox = bboxCache.ComputeWorldBound(prim);
        bbox = GfBBox3d::Combine(bbox, worldbbox);
    }
    return bbox;
}

UsdStageRefPtr
DataModel::stage() const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    return p->d.stage;
}
}  // namespace usd
