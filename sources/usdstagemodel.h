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
    enum LoadMode { All, Payload };

public:
    StageModel();
    StageModel(const QString& filename, LoadMode loadmode = LoadMode::All);
    StageModel(const StageModel& other);
    ~StageModel();
    bool loadFromFile(const QString& filename, LoadMode loadMode);
    bool loadPayloads(const QList<SdfPath>& paths, const std::string& variantSet = std::string(),
                      const std::string& variantValue = std::string());
    bool unloadPayloads(const QList<SdfPath>& paths);
    bool saveToFile(const QString& filename);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool reload();
    bool close();
    bool isLoaded() const;
    void setVisible(const QList<SdfPath>& paths, bool visible, bool recursive = false);
    void setMask(const QList<SdfPath>& paths);
    LoadMode loadMode() const;
    GfBBox3d boundingBox();
    GfBBox3d boundingBox(const QList<SdfPath> paths);
    std::map<std::string, std::vector<std::string>> variantSets(const QList<SdfPath>& paths, bool recursive = false);
    QString filename() const;
    UsdStageRefPtr stage() const;
    QReadWriteLock* stageLock() const;

Q_SIGNALS:
    void payloadsRequested(const QList<SdfPath>& paths);
    void payloadsFailed(const SdfPath& path);
    void payloadsLoaded(const SdfPath& path);
    void payloadsUnloaded(const SdfPath& path);
    void boundingBoxChanged(const GfBBox3d& bbox);
    void maskChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths);
    void stageChanged();

private:
    QExplicitlySharedDataPointer<StageModelPrivate> p;
};
}  // namespace usd
