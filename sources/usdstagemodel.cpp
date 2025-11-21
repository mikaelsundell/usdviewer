// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstagemodel.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <stack>

namespace usd {

class StageModelPrivate : public QSharedData {
public:
    StageModelPrivate();
    ~StageModelPrivate();
    bool loadFromFile(const QString& filename, StageModel::LoadMode loadMode);
    bool loadPayloads(const QList<SdfPath>& paths, const std::string& variantSet, const std::string& variantValue);
    bool unloadPayloads(const QList<SdfPath>& paths);
    bool saveToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool close();
    bool reload();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    void setVisible(const QList<SdfPath>& paths, bool visible, bool recursive);
    GfBBox3d boundingBox();
    GfBBox3d boundingBox(const QList<SdfPath>& paths);
    std::map<std::string, std::vector<std::string>> variantSets(const QList<SdfPath>& paths, bool recursive);

public:
    void primsChanged(const QList<SdfPath> paths);
    void stageChanged();
    struct Data {
        UsdStageRefPtr stage;
        StageModel::LoadMode loadMode;
        QString filename;
        GfBBox3d bbox;
        QList<SdfPath> mask;
        QThreadPool pool;
        QReadWriteLock stageLock;
        QScopedPointer<UsdGeomBBoxCache> bboxCache;
        QPointer<StageModel> stageModel;
    };
    Data d;
};

StageModelPrivate::StageModelPrivate()
{
    d.loadMode = StageModel::LoadMode::All;
    d.pool.setMaxThreadCount(QThread::idealThreadCount());
    d.pool.setThreadPriority(QThread::HighPriority);
}

StageModelPrivate::~StageModelPrivate() {}

bool
StageModelPrivate::loadFromFile(const QString& filename, StageModel::LoadMode loadMode)
{
    {
        QWriteLocker locker(&d.stageLock);
        if (loadMode == StageModel::LoadMode::All)
            d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadAll);
        else
            d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadNone);
        d.loadMode = loadMode;
        d.mask = QList<SdfPath>();
    }
    if (d.stage) {
        d.bboxCache.reset();
        d.bbox = boundingBox();
        d.filename = filename;
    }
    else {
        d.bboxCache.reset();
        return false;
    }
    QMetaObject::invokeMethod(
        d.stageModel,
        [this]() {
            setMask(d.mask);
            stageChanged();
        },
        Qt::QueuedConnection);
    return true;
}

bool
StageModelPrivate::loadPayloads(const QList<SdfPath>& paths, const std::string& variantSet,
                                const std::string& variantValue)
{
    if (!isLoaded())
        return false;

    const bool useVariant = (!variantSet.empty() && !variantValue.empty());

    Q_EMIT d.stageModel->payloadsRequested(paths);

    auto stage = d.stage;
    auto pool = &d.pool;
    QFuture<void> future = QtConcurrent::run(pool, [this, stage, paths, variantSet, variantValue, useVariant]() {
        QList<SdfPath> loaded;
        QList<SdfPath> failed;
        {
            QWriteLocker locker(&d.stageLock);
            for (const SdfPath& path : paths) {
                UsdPrim prim = stage->GetPrimAtPath(path);
                if (!prim) {
                    failed.append(path);
                    Q_EMIT d.stageModel->payloadsFailed(path);
                    continue;
                }
                try {
                    if (useVariant) {
                        if (prim.IsLoaded())
                            prim.Unload();

                        UsdVariantSet vs = prim.GetVariantSet(variantSet);
                        if (!vs.IsValid()) {
                            failed.append(path);
                            Q_EMIT d.stageModel->payloadsFailed(path);
                            continue;
                        }
                        auto values = vs.GetVariantNames();
                        if (std::find(values.begin(), values.end(), variantValue) == values.end()) {
                            failed.append(path);
                            Q_EMIT d.stageModel->payloadsFailed(path);
                            continue;
                        }
                        vs.SetVariantSelection(variantValue);
                    }

                    if (prim.IsLoaded()) {
                        loaded.append(path);
                        Q_EMIT d.stageModel->payloadsLoaded(path);
                        continue;
                    }

                    prim.Load();
                    loaded.append(path);
                    Q_EMIT d.stageModel->payloadsLoaded(path);
                } catch (const std::exception& e) {
                    failed.append(path);
                    Q_EMIT d.stageModel->payloadsFailed(path);
                }
            }
        }
        if (!loaded.isEmpty()) {
            QMetaObject::invokeMethod(
                d.stageModel,
                [this, loaded]() {
                    {
                        QWriteLocker locker(&d.stageLock);
                        d.bboxCache.reset();
                        d.bbox = boundingBox();
                    }
                    primsChanged(loaded);
                },
                Qt::QueuedConnection);
        }
    });
    return true;
}

bool
StageModelPrivate::unloadPayloads(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT d.stageModel->payloadsRequested(paths);

    auto stage = d.stage;
    auto pool = &d.pool;
    QFuture<void> future = QtConcurrent::run(pool, [this, stage, paths]() {
        QList<SdfPath> unloaded;
        {
            QWriteLocker locker(&d.stageLock);
            for (const SdfPath& path : paths) {
                UsdPrim prim = stage->GetPrimAtPath(path);
                if (!prim)
                    continue;

                prim.Unload();
                unloaded.append(path);

                Q_EMIT d.stageModel->payloadsUnloaded(path);
            }
        }
        QMetaObject::invokeMethod(
            d.stageModel,
            [this, unloaded]() {
                if (!unloaded.isEmpty()) {
                    {
                        QWriteLocker locker(&d.stageLock);
                        d.bboxCache.reset();
                        d.bbox = boundingBox();
                    }
                    primsChanged(unloaded);
                }
            },
            Qt::QueuedConnection);
    });
    return true;
}

bool
StageModelPrivate::saveToFile(const QString& filename)
{
    if (!isLoaded())
        return false;

    QWriteLocker locker(&d.stageLock);
    try {
        SdfLayerHandle rootLayer = d.stage->GetRootLayer();
        if (!rootLayer) {
            return false;
        }
        if (rootLayer->IsAnonymous()) {
            return d.stage->Export(filename.toStdString());
        }
        if (QFileInfo(d.filename).canonicalFilePath() == QFileInfo(filename).canonicalFilePath()) {
            d.stage->Save();
            return true;
        }
        return d.stage->Export(filename.toStdString());
    } catch (const std::exception& e) {
        return false;
    }
}

bool
StageModelPrivate::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    QReadLocker locker(&d.stageLock);
    UsdStagePopulationMask mask;
    for (const SdfPath& path : paths) {
        bool isChildOfAnother = false;
        for (const SdfPath& other : paths) {
            if (path.HasPrefix(other) && path != other) {
                isChildOfAnother = true;
                break;
            }
        }
        if (!isChildOfAnother)
            mask.Add(path);
    }
    if (mask.GetPaths().empty())
        return false;
    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(d.stage->GetRootLayer(), mask);
    if (!maskedStage)
        return false;

    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(filename.toStdString());
}

bool
StageModelPrivate::close()
{
    {
        QWriteLocker locker(&d.stageLock);
        d.stage = nullptr;
        d.bboxCache.reset();
    }
    QMetaObject::invokeMethod(
        d.stageModel, [this]() { stageChanged(); }, Qt::QueuedConnection);

    return true;
}

bool
StageModelPrivate::reload()
{
    {
        QWriteLocker locker(&d.stageLock);
        if (d.stage) {
            d.stage->Reload();
            d.bboxCache.reset();
            d.bbox = boundingBox();
        }
    }
    QMetaObject::invokeMethod(
        d.stageModel, [this]() { stageChanged(); }, Qt::QueuedConnection);

    return true;
}

bool
StageModelPrivate::isLoaded() const
{
    return d.stage != nullptr;
}

void
StageModelPrivate::setVisible(const QList<SdfPath>& paths, bool visible, bool recursive)
{
    QList<SdfPath> affected;
    {
        QWriteLocker locker(&d.stageLock);
        Q_ASSERT("stage is not loaded" && isLoaded());
        for (const SdfPath& path : paths) {
            UsdPrim prim = d.stage->GetPrimAtPath(path);
            if (!prim)
                continue;

            UsdGeomImageable imageable(prim);
            if (imageable) {
                if (visible)
                    imageable.MakeVisible();
                else
                    imageable.MakeInvisible();
                affected.append(path);
            }
            if (recursive) {
                for (const UsdPrim& child : prim.GetAllDescendants()) {
                    UsdGeomImageable childImageable(child);
                    if (!childImageable)
                        continue;

                    TfToken currentVis;
                    childImageable.GetVisibilityAttr().Get(&currentVis);
                    TfToken desiredVis = visible ? UsdGeomTokens->inherited : UsdGeomTokens->invisible;

                    if (currentVis != desiredVis) {
                        if (visible)
                            childImageable.MakeVisible();
                        else
                            childImageable.MakeInvisible();
                        affected.append(child.GetPath());
                    }
                }
            }
        }
    }
    if (!affected.isEmpty()) {
        QMetaObject::invokeMethod(
            d.stageModel, [this, affected]() { primsChanged(affected); }, Qt::QueuedConnection);
    }
}

void
StageModelPrivate::setMask(const QList<SdfPath>& paths)
{
    d.mask = paths;
    Q_EMIT d.stageModel->maskChanged(paths);
}

GfBBox3d
StageModelPrivate::boundingBox()
{
    QReadLocker locker(&d.stageLock);
    if (d.mask.isEmpty()) {
        Q_ASSERT("stage is not loaded" && isLoaded());
        if (!d.bboxCache) {
            d.bboxCache.reset(
                new UsdGeomBBoxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true));
        }
        return d.bboxCache->ComputeWorldBound(d.stage->GetPseudoRoot());
    }
    else {
        return boundingBox(d.mask);
    }
}

GfBBox3d
StageModelPrivate::boundingBox(const QList<SdfPath>& paths)
{
    QReadLocker locker(&d.stageLock);
    Q_ASSERT("stage is not loaded" && isLoaded());
    UsdGeomBBoxCache cache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true);
    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim || !prim.IsA<UsdGeomImageable>())
            continue;

        bbox = GfBBox3d::Combine(bbox, cache.ComputeWorldBound(prim));
    }
    return bbox;
}

std::map<std::string, std::vector<std::string>>
StageModelPrivate::variantSets(const QList<SdfPath>& paths, bool recursive)
{
    QReadLocker locker(&d.stageLock);

    std::map<std::string, std::vector<std::string>> result;

    std::vector<SdfPath> filtered;
    for (const SdfPath& p : paths) {
        bool isChild = false;
        for (const SdfPath& other : paths) {
            if (p == other)
                continue;
            if (p.HasPrefix(other)) {
                isChild = true;
                break;
            }
        }
        if (!isChild)
            filtered.push_back(p);
    }

    std::vector<UsdPrim> prims;

    for (const SdfPath& path : filtered) {
        UsdPrim root = d.stage->GetPrimAtPath(path);

        if (!root) {
            continue;
        }
        auto countChildren = [](const UsdPrim& prim) {
            size_t n = 0;
            for (const auto& c : prim.GetAllChildren())
                n++;
            return n;
        };
        prims.push_back(root);
        if (recursive) {
            std::stack<UsdPrim> stack;
            stack.push(root);
            while (!stack.empty()) {
                UsdPrim p = stack.top();
                stack.pop();
                size_t numChildren = countChildren(p);
                for (const UsdPrim& child : p.GetAllChildren()) {
                    prims.push_back(child);
                    stack.push(child);
                }
            }
        }
    }
    for (const UsdPrim& prim : prims) {
        if (!prim)
            continue;

        auto sets = prim.GetVariantSets().GetNames();

        for (const std::string& setName : sets) {
            UsdVariantSet vs = prim.GetVariantSet(setName);
            auto values = vs.GetVariantNames();

            auto& bucket = result[setName];
            bucket.insert(bucket.end(), values.begin(), values.end());
        }
    }
    for (auto& it : result) {
        auto& vec = it.second;
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }

    return result;
}

void
StageModelPrivate::primsChanged(const QList<SdfPath> paths)
{
    Q_EMIT d.stageModel->primsChanged(paths);
    Q_EMIT d.stageModel->boundingBoxChanged(d.bbox);
    if (!d.mask.isEmpty()) {
        QList<SdfPath> newMask;
        bool changed = false;
        {
            QReadLocker locker(&d.stageLock);
            for (const SdfPath& path : d.mask) {
                UsdPrim prim = d.stage->GetPrimAtPath(path);
                if (prim && prim.IsValid() && prim.IsActive()) {
                    newMask.append(path);
                }
                else {
                    changed = true;
                }
            }
        }
        if (changed) {
            d.mask = newMask;
            Q_EMIT d.stageModel->maskChanged(newMask);
        }
    }
}

void
StageModelPrivate::stageChanged()
{
    Q_EMIT d.stageModel->stageChanged();
    Q_EMIT d.stageModel->boundingBoxChanged(d.bbox);
}

StageModel::StageModel()
    : p(new StageModelPrivate())
{
    p->d.stageModel = this;
}

StageModel::StageModel(const QString& filename, LoadMode loadMode)
    : p(new StageModelPrivate())
{
    p->d.stageModel = this;
    loadFromFile(filename, loadMode);
}

StageModel::StageModel(const StageModel& other)
    : p(other.p)
{}

StageModel::~StageModel() {}

bool
StageModel::loadFromFile(const QString& filename, LoadMode loadMode)
{
    return p->loadFromFile(filename, loadMode);
}

bool
StageModel::loadPayloads(const QList<SdfPath>& paths, const std::string& variantSet, const std::string& variantValue)
{
    return p->loadPayloads(paths, variantSet, variantValue);
}

bool
StageModel::unloadPayloads(const QList<SdfPath>& paths)
{
    return p->unloadPayloads(paths);
}

bool
StageModel::saveToFile(const QString& filename)
{
    return p->saveToFile(filename);
}

bool
StageModel::exportToFile(const QString& filename)
{
    QReadLocker locker(&p->d.stageLock);
    return p->d.stage->Export(filename.toStdString());
}

bool
StageModel::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    return p->exportPathsToFile(paths, filename);
}

bool
StageModel::reload()
{
    return p->reload();
}

bool
StageModel::close()
{
    return p->close();
}

bool
StageModel::isLoaded() const
{
    return p->isLoaded();
}

void
StageModel::setMask(const QList<SdfPath>& paths)
{
    p->setMask(paths);
}

void
StageModel::setVisible(const QList<SdfPath>& paths, bool visible, bool recursive)
{
    p->setVisible(paths, visible, recursive);
}

GfBBox3d
StageModel::boundingBox()
{
    return p->boundingBox();
}

StageModel::LoadMode
StageModel::loadMode() const
{
    return p->d.loadMode;
}

GfBBox3d
StageModel::boundingBox(const QList<SdfPath>& paths)
{
    return p->boundingBox(paths);
}

std::map<std::string, std::vector<std::string>>
StageModel::variantSets(const QList<SdfPath>& paths, bool recursive)
{
    return p->variantSets(paths, recursive);
}

QString
StageModel::filename() const
{
    return p->d.filename;
}

UsdStageRefPtr
StageModel::stage() const
{
    QReadLocker locker(&p->d.stageLock);
    Q_ASSERT("stage is not loaded" && isLoaded());
    return p->d.stage;
}

QReadWriteLock*
StageModel::stageLock() const
{
    return &p->d.stageLock;
}

}  // namespace usd
