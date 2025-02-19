// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QObject>
#include <pxr/usd/usdGeom/camera.h>
#include <QExplicitlySharedDataPointer>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class ViewCameraPrivate;
class ViewCamera {
    public:
        enum CameraMode { None, Truck, Tumble, Zoom, Pick };
        enum FovDirection { Vertical, Horizontal };
        ViewCamera();
        ViewCamera(qreal aspectratio, qreal fov, ViewCamera::FovDirection direction = Vertical);
        virtual ~ViewCamera();
        void frameAll() const;
        void tumble(double x, double y);
    
        GfCamera camera() const;
        qreal aspectRatio() const;
        void setAspectRatio(qreal aspectRatio);
        GfBBox3d boundingBox() const;
        void setBoundingBox(const GfBBox3d& boundingBox);
        CameraMode cameraMode();
        void setCameraMode(ViewCamera::CameraMode cameraMode);
        qreal fov() const;
        void setFov(qreal fov);
        FovDirection fovDirection() const;
        void setFovDirection(ViewCamera::FovDirection direction);
        qreal near() const;
        void setNear(qreal near);
        qreal far() const;
        void setFar(qreal near);
        
    private:
        QExplicitlySharedDataPointer<ViewCameraPrivate> p;
    };
}
