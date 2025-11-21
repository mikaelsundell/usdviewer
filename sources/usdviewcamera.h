// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <pxr/usd/usdGeom/camera.h>
#include <QExplicitlySharedDataPointer>
#include <QObject>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class ViewCameraPrivate;
class ViewCamera {
public:
    enum CameraUp { X, Y, Z };
    enum CameraMode { None, Truck, Tumble, Zoom, Pick };
    enum FovDirection { Vertical, Horizontal };
    ViewCamera();
    ViewCamera(double aspectratio, double fov, ViewCamera::FovDirection direction = Vertical);
    ViewCamera(const ViewCamera& other);
    virtual ~ViewCamera();
    void frameAll() const;
    void resetView();
    void tumble(double x, double y);
    void truck(double right, double up);
    void distance(double factor);
    double mapToFrustumHeight(int height);
    GfCamera camera() const;

    double aspectRatio() const;
    void setAspectRatio(double aspectRatio);

    GfVec3d focusPoint() const;
    void setFocusPoint(const GfVec3d& point);

    GfBBox3d boundingBox() const;
    void setBoundingBox(const GfBBox3d& boundingBox);

    CameraUp cameraUp();
    void setCameraUp(ViewCamera::CameraUp cameraUp);

    CameraMode cameraMode();
    void setCameraMode(ViewCamera::CameraMode cameraMode);

    double fov() const;
    void setFov(double fov);

    FovDirection fovDirection() const;
    void setFovDirection(ViewCamera::FovDirection direction);

    double nearClipping() const;
    void setNearClipping(double near);

    double farClipping() const;
    void setFarClipping(double near);

    ViewCamera& operator=(const ViewCamera& other);

private:
    QExplicitlySharedDataPointer<ViewCameraPrivate> p;
};
}  // namespace usd
