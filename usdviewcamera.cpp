// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewcamera.h"
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/rotation.h>
#include <QDebug>

class UsdViewCameraPrivate : public QSharedData {
    public:
        void init();
        void push_cameratransform();
        void pull_cameratransform();
        GfMatrix4d rotate_matrix(const GfVec3d& vec, double angle);
    
        struct Data
        {
            GfCamera camera;
            GfCamera::FOVDirection direction = GfCamera::FOVVertical;
            
            
            
            GfMatrix4d zupmatrix;
            GfMatrix4d invzupmatrix;
            
            GfMatrix4d yzupmatrix;
            GfMatrix4d invyzupmatrix;

            size_t defaultnear = 1;
            size_t defaulfar = 2000000;
            size_t maxsafezresolution = 1e6;
            size_t goodzresolution = 5e4;
            
            bool zup = true;
            bool overridenear = false;
            bool overridefar = false;
            qreal fov = 60.0;
            qreal aspectratio = 1.0;
            qreal rottheta = 0;
            qreal rotphi = 0;
            qreal rotpsi = 0;
            

            
            GfVec3d center = GfVec3d(0.0, 0.0, 0.0);
            qreal distance = 100;
            double focusdistance = distance;
            qreal closesvisibledist = 0.0;
            qreal lastframeddist = 0.0;
            qreal lastframedclosestdist = 0.0;
            qreal selsize = 10;
            
            bool dirty = false;
        };
        Data d;
};

void
UsdViewCameraPrivate::init()
{
    d.camera.SetPerspectiveFromAspectRatioAndFieldOfView(d.aspectratio, d.fov, GfCamera::FOVVertical);
    if (d.zup) {
        d.zupmatrix = GfMatrix4d().SetRotate(
            GfRotation(GfVec3d().XAxis(), -90)
        );
        d.invzupmatrix = d.zupmatrix.GetInverse();
    }
    else {
        d.yzupmatrix = GfMatrix4d(1.0);
        d.invyzupmatrix = GfMatrix4d(1.0);
    }
}

void
UsdViewCameraPrivate::push_cameratransform()
{
    // updates the camera's transform matrix, that is, the matrix that
    // brings the camera to the origin, with the camera view pointing down:
    //   +Y if this is a Zup camera, or
    //   -Z if this is a Yup camera
    if (!d.dirty) {
        return;
    }
    GfMatrix4d matrix = GfMatrix4d().SetTranslate(GfVec3d().ZAxis() * d.distance);
    matrix *= rotate_matrix(GfVec3d().ZAxis(), -d.rotpsi);
    matrix *= rotate_matrix(GfVec3d().XAxis(), -d.rotphi);
    matrix *= rotate_matrix(GfVec3d().YAxis(), -d.rottheta);
    matrix *= d.invzupmatrix;
    matrix *= GfMatrix4d().SetTranslate(d.center);
    d.camera.SetTransform(matrix);
    d.dirty = false;
}

void
UsdViewCameraPrivate::pull_cameratransform()
{
    // updates parameters (center, rotTheta, etc.) from the camera transform
    GfMatrix4d cameratransform = d.camera.GetTransform();
    float distance = d.camera.GetFocusDistance();
    GfFrustum frustum = d.camera.GetFrustum();
    GfVec3d position = frustum.GetPosition();
    GfVec3d viewdirection = frustum.ComputeViewDirection();
    d.distance = distance;
    d.selsize = distance / 10.0;
    d.center = position + distance * viewdirection;
    
    GfMatrix4d transform = cameratransform * d.yzupmatrix;
    transform.Orthonormalize();
    GfRotation rotation = transform.ExtractRotation();
    
    GfVec3d yaxis = GfVec3d::YAxis();
    GfVec3d xaxis = GfVec3d::XAxis();
    GfVec3d zaxis = GfVec3d::ZAxis();
    
    GfVec3d vec = rotation.Decompose(yaxis, xaxis, zaxis);
    d.rottheta = -vec[0];
    d.rotphi = -vec[1];
    d.rotpsi = -vec[2];
    d.dirty = true;
    return;
}

GfMatrix4d
UsdViewCameraPrivate::rotate_matrix(const GfVec3d& vec, double angle)
{
    return GfMatrix4d(1.0).SetRotate(GfRotation(vec, angle));
}

UsdViewCamera::UsdViewCamera()
: p(new UsdViewCameraPrivate())
{
    p->init();
}

UsdViewCamera::UsdViewCamera(double aspectratio, double fov, GfCamera::FOVDirection direction)
: p(new UsdViewCameraPrivate())
{
    p->d.aspectratio = aspectratio;
    p->d.fov = fov;
    p->d.direction = direction;
    p->init();
}

UsdViewCamera::~UsdViewCamera()
{
}

GfCamera
UsdViewCamera::camera() const
{
    return p->d.camera;
}
