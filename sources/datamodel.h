// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QExplicitlySharedDataPointer>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QVariant>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class DataModelPrivate;
class DataModel : public QObject {
    Q_OBJECT
public:
    enum load_policy { load_all, load_payload };
    enum progress_mode { progress_idle, progress_running };
    enum stage_status { stage_loaded, stage_failed, stage_closed };

public:
    struct Notify {
        QString message;
        QList<SdfPath> paths;
        QVariantMap details;
        Notify() = default;
        Notify(const QString& n, const QList<SdfPath>& p, const QVariantMap& d = {})
            : message(n)
            , paths(p)
            , details(d)
        {}
    };

public:
    DataModel();
    DataModel(const QString& filename, load_policy policy = load_all);
    DataModel(const DataModel& other);
    ~DataModel();
    void beginProgressBlock(const QString& name, size_t count = 0);
    void updateProgressNotify(const Notify& notify, size_t completed);
    void cancelProgressBlock();
    void endProgressBlock();
    bool isProgressBlockCancelled() const;
    bool loadFromFile(const QString& filename, load_policy policy = load_all);
    bool saveToFile(const QString& filename);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool reload();
    bool close();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    void setStatus(const QString& status);
    load_policy loadPolicy() const;
    GfBBox3d boundingBox();
    QString filename() const;
    UsdStageRefPtr stage() const;
    QReadWriteLock* stageLock() const;

Q_SIGNALS:
    void progressBlockChanged(const QString& name, progress_mode mode);
    void progressNotifyChanged(const Notify& notify, size_t completed, size_t expected);
    void boundingBoxChanged(const GfBBox3d& bbox);
    void maskChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, load_policy policy, stage_status status);
    void statusChanged(const QString& status);

private:
    QExplicitlySharedDataPointer<DataModelPrivate> p;
};
}  // namespace usd
