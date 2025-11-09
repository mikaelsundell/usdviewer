// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QExplicitlySharedDataPointer>
#include <QObject>
#include <QReadWriteLock>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class StageModelPrivate;
class StageModel : public QObject {
    Q_OBJECT
public:
    enum load_type { load_none, load_all, load_payload };

public:
    StageModel();
    StageModel(const QString& filename, load_type loadtype = load_type::load_all);
    StageModel(const StageModel& other);
    ~StageModel();
    bool loadFromFile(const QString& filename, load_type loadType);
    bool loadPayloads(const QList<SdfPath>& paths);
    bool unloadPayloads(const QList<SdfPath>& paths);
    void setVisible(const QList<SdfPath>& paths, bool visible, bool hierarchy = false);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool reload();
    bool close();
    bool isLoaded() const;
    load_type loadType() const;
    GfBBox3d boundingBox();
    GfBBox3d boundingBox(const QList<SdfPath> paths);
    UsdStageRefPtr stage() const;
    QReadWriteLock* stageLock() const;

Q_SIGNALS:
    void payloadsRequested(const QList<SdfPath>& paths);
    void payloadsFailed(const SdfPath& path);
    void payloadsLoaded(const SdfPath& path);
    void payloadsUnloaded(const SdfPath& path);
    void boundingBoxChanged(const GfBBox3d& bbox);
    void primsChanged(const QList<SdfPath>& paths);
    void stageChanged();

private:
    QExplicitlySharedDataPointer<StageModelPrivate> p;
};
}  // namespace usd
