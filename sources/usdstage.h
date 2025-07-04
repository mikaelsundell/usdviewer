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
class StagePrivate;
class Stage {
public:
    Stage();
    Stage(const QString& filename);
    Stage(const Stage& other);
    ~Stage();
    bool loadFromFile(const QString& filename);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool isValid() const;
    GfBBox3d boundingBox() const;
    GfBBox3d boundingBox(const QList<SdfPath> paths) const;
    UsdStageRefPtr stagePtr() const;
    QMap<QString, QVariant> metadata() const;

    Stage& operator=(const Stage& other);

private:
    QExplicitlySharedDataPointer<StagePrivate> p;
};
}  // namespace usd
