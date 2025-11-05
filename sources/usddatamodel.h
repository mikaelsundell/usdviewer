// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QExplicitlySharedDataPointer>
#include <QObject>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class DataModelPrivate;
class DataModel : public QObject {
    Q_OBJECT
public:
    enum load_type { load_none, load_all, load_structure };

public:
    DataModel();
    DataModel(const QString& filename, load_type loadtype = load_type::load_all);
    DataModel(const DataModel& other);
    ~DataModel();
    bool loadFromFile(const QString& filename, load_type loadType);
    bool loadFromPaths(const QList<SdfPath>& paths);
    bool unloadFromPath(const QList<SdfPath>& paths);
    void visibleFromPaths(const QList<SdfPath>& paths, bool visible);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool reload();
    bool close();
    bool isLoaded() const;
    load_type loadType() const;
    GfBBox3d boundingBox() const;
    GfBBox3d boundingBox(const QList<SdfPath> paths) const;
    UsdStageRefPtr stage() const;

Q_SIGNALS:
    void loadPathsSubmitted(const QList<SdfPath>& paths);
    void loadPathFailed(const SdfPath& path);
    void loadPathCompleted(const SdfPath& path);
    void unloadPathsSubmitted(const QList<SdfPath>& path);
    void unloadPathCompleted(const SdfPath& path);
    void primsChanged(const QList<SdfPath>& paths);
    void stageChanged();

private:
    QExplicitlySharedDataPointer<DataModelPrivate> p;
};
}  // namespace usd
