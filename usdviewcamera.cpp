// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewcamera.h"
#include "usdutils.h"
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/range1d.h>
#include <QDebug>

namespace usd {
class ViewCameraPrivate : public QSharedData {
public:
    void frameAll();
    void tumble(double x, double y);
    GfCamera camera();
    GfMatrix4d rotateAngle(const GfVec3d& value, double angle);
    struct Data
    {
        double fov = 60.0;
        double aspectratio = 1.0;
        double near = 1;
        double far = 2000000;
        double fit = 1.1;
        double distance;
        GfBBox3d boundingBox;
        GfVec3d center;
        ViewCamera::CameraMode cameraMode;
        ViewCamera::FovDirection direction = ViewCamera::Vertical;
        GfCamera camera;
    
        bool valid = false;
        
        // internals
        
        GfMatrix4d zupmatrix;
        GfMatrix4d invzupmatrix;
        
        GfMatrix4d yzupmatrix;
        GfMatrix4d invyzupmatrix;
        
        size_t maxsafezresolution = 1e6;
        size_t goodzresolution = 5e4;
        
        bool zup = true;
        bool overridenear = false;
        bool overridefar = false;
        
        double rottheta = 0;
        double rotphi = 0;
        double rotpsi = 0;
        
        double closesvisibledist = 0.0;
        double lastframeddist = 0.0;
        double lastframedclosestdist = 0.0;
        double selsize = 10;
        
        
        
        // internals
        
        

    };
    Data d;
};

void
ViewCameraPrivate::frameAll()
{
    d.center = d.boundingBox.ComputeCentroid();
    GfRange3d range = d.boundingBox.ComputeAlignedRange();
    GfVec3d size = range.GetSize();
    double maxsize = std::max(size[0], std::max(size[1], size[2]));
    double fovangle = d.fov * 0.5;
    if (fovangle == 0.0) {
        fovangle = 0.5;
    }
    double length = maxsize * d.fit * 0.5;
    d.distance = length / std::atan(fovangle * M_PI / 180.0);
    if (d.distance < d.near + maxsize * 0.5) {
        d.distance = d.near + length;
    }
    d.valid = false;
}

void
ViewCameraPrivate::tumble(double x, double y)
{
    d.rottheta += x;
    d.rotphi += y;
    d.valid = false;
}

GfCamera
ViewCameraPrivate::camera()
{
    if (!d.valid) {
        GfMatrix4d matrix = GfMatrix4d().SetTranslate(GfVec3d().ZAxis() * d.distance);
        matrix *= rotateAngle(GfVec3d().ZAxis(), -d.rotpsi);
        matrix *= rotateAngle(GfVec3d().XAxis(), -d.rotphi);
        matrix *= rotateAngle(GfVec3d().YAxis(), -d.rottheta);
        matrix *= GfMatrix4d().SetTranslate(d.center);
        d.camera.SetTransform(matrix);
        d.camera.SetPerspectiveFromAspectRatioAndFieldOfView(d.aspectratio, d.fov, GfCamera::FOVVertical);
        d.camera.SetClippingRange(GfRange1f(d.near, d.far));
        d.valid = true;
    }
    return d.camera;
}

GfMatrix4d
ViewCameraPrivate::rotateAngle(const GfVec3d& value, double angle)
{
    return GfMatrix4d(1.0).SetRotate(GfRotation(value, angle));
}

ViewCamera::ViewCamera()
: p(new ViewCameraPrivate())
{
}

ViewCamera::ViewCamera(qreal aspectratio, qreal fov, ViewCamera::FovDirection direction)
: p(new ViewCameraPrivate())
{
    p->d.aspectratio = aspectratio;
    p->d.fov = fov;
    p->d.direction = direction;
}

ViewCamera::~ViewCamera()
{
}

void
ViewCamera::frameAll() const
{
    p->frameAll();
}

void
ViewCamera::tumble(double x, double y)
{
    p->tumble(x, y);
}

qreal
ViewCamera::aspectRatio() const
{
    return p->d.aspectratio;
}

void
ViewCamera::setAspectRatio(qreal aspectRatio)
{
    if (!qFuzzyCompare(p->d.aspectratio, aspectRatio)) {
        p->d.aspectratio = aspectRatio;
        p->d.valid = false;
    }
}

GfBBox3d
ViewCamera::boundingBox() const
{
    return p->d.boundingBox;
}

void
ViewCamera::setBoundingBox(const GfBBox3d& boundingBox)
{
    if (p->d.boundingBox != boundingBox) {
        p->d.boundingBox = boundingBox;
        p->d.valid = false;
    }
}

ViewCamera::CameraMode
ViewCamera::cameraMode()
{
    return p->d.cameraMode;
}

void
ViewCamera::setCameraMode(ViewCamera::CameraMode cameraMode)
{
    if (p->d.cameraMode != cameraMode) {
        p->d.cameraMode = cameraMode;
        p->d.valid = false;
    }
}

double
ViewCamera::fov() const
{
    return p->d.fov;
}

void
ViewCamera::setFov(double fov)
{
    if (p->d.fov != fov) {
        p->d.fov = fov;
        p->d.valid = false;
    }
}

ViewCamera::FovDirection
ViewCamera::fovDirection() const
{
    return p->d.direction;
}

void
ViewCamera::setFovDirection(ViewCamera::FovDirection direction)
{
    if (p->d.direction != direction) {
        p->d.direction = direction;
        p->d.valid = false;
    }
}

qreal
ViewCamera::near() const
{
    return p->d.near;
}

void
ViewCamera::setNear(qreal near)
{
    if (p->d.near != near) {
        p->d.near = near;
        p->d.valid = false;
    }
}

qreal
ViewCamera::far() const
{
    return p->d.far;
}

void
ViewCamera::setFar(qreal far)
{
    if (p->d.far != far) {
        p->d.far = far;
        p->d.valid = false;
    }
}

GfCamera
ViewCamera::camera() const
{
    return p->camera();
}
}

