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
class DataModelPrivate;
class DataModel : public QObject {
    Q_OBJECT
public:
    enum load_policy { load_all, load_payload };
    enum payload_mode { payload_loaded, payload_unloaded, payload_failed };
    enum stage_status { stage_loaded, stage_failed, stage_closed };

public:
    DataModel();
    DataModel(const QString& filename, load_policy policy = load_all);
    DataModel(const DataModel& other);
    ~DataModel();
    bool loadFromFile(const QString& filename, load_policy policy = load_all);
    bool loadPayloads(const QList<SdfPath>& paths, const QString& variantSet = QString(),
                      const QString& variantValue = QString());
    bool unloadPayloads(const QList<SdfPath>& paths);
    void cancelPayloads();
    bool saveToFile(const QString& filename);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool reload();
    bool close();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    load_policy loadPolicy() const;
    GfBBox3d boundingBox();
    GfBBox3d boundingBox(const QList<SdfPath>& paths);
    QString filename() const;
    UsdStageRefPtr stage() const;
    QReadWriteLock* stageLock() const;

Q_SIGNALS:
    void boundingBoxChanged(const GfBBox3d& bbox);
    void maskChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths);
    void payloadsRequested(const QList<SdfPath>& paths, payload_mode mode);
    void payloadChanged(const SdfPath& path, payload_mode mode);
    void stageChanged(UsdStageRefPtr stage, load_policy policy, stage_status status);

private:
    QExplicitlySharedDataPointer<DataModelPrivate> p;
};
}  // namespace usd
