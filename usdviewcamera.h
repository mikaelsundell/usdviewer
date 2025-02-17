// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QObject>
#include <pxr/usd/usdGeom/camera.h>
#include <QExplicitlySharedDataPointer>

PXR_NAMESPACE_USING_DIRECTIVE

class UsdViewCameraPrivate;
class UsdViewCamera : public QObject {
    Q_OBJECT
    public:
        UsdViewCamera();
        UsdViewCamera(double aspectratio, double fov, GfCamera::FOVDirection direction = GfCamera::FOVVertical);
        virtual ~UsdViewCamera();
        GfCamera camera() const;
        
    private:
        QExplicitlySharedDataPointer<UsdViewCameraPrivate> p;
};
