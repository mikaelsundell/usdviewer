// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QObject>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/usd/usd/stage.h>
#include <QExplicitlySharedDataPointer>

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
        bool isValid() const;
        GfBBox3d boundingBox() const;
        UsdStageRefPtr stagePtr() const;
        
        Stage& operator=(const Stage& other);
    
    private:
        QExplicitlySharedDataPointer<StagePrivate> p;
};
}
