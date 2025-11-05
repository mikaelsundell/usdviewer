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
    enum load_type { load_all, load_structure };
public:
    Stage();
    Stage(const QString& filename, load_type loadtype = load_type::load_all);
    Stage(const Stage& other);
    ~Stage();
    bool loadFromFile(const QString& filename, load_type loadtype);
    bool exportToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool reload();
    bool close();
    bool isValid() const;
    GfBBox3d boundingBox() const;
    GfBBox3d boundingBox(const QList<SdfPath> paths) const;
    UsdStageRefPtr stagePtr() const;

    Stage& operator=(const Stage& other);

private:
    QExplicitlySharedDataPointer<StagePrivate> p;
};
}  // namespace usd
