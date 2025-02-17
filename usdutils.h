// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QOpenGLFunctions>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/base/gf/range2d.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>
#include <QDebug>

PXR_NAMESPACE_USING_DIRECTIVE

GfVec4f QColor_GfVec4f(const QColor& color);
TfToken QString_TfToken(const QString& str);

QString TfToken_QString(const TfToken& token);
QList<QString> TfTokenVector_QList(const pxr::TfTokenVector& tokens);

void CheckOpenGLError(const char* function, const char* file, int line);
#define CHECK_GL_ERROR() CheckOpenGLError(__FUNCTION__, __FILE__, __LINE__)

void DebugStagePrims(const UsdStageRefPtr& stage);
void DebugBoundingBoxes(const UsdStageRefPtr& stage);
    
QDebug operator<<(QDebug debug, const GfRange1f& range);
QDebug operator<<(QDebug debug, const GfRange2d& range);
QDebug operator<<(QDebug debug, const GfVec2i& vec);
QDebug operator<<(QDebug debug, const GfVec3d& vec);
QDebug operator<<(QDebug debug, const GfVec4d& vec);
QDebug operator<<(QDebug debug, const GfVec4f& vec);
QDebug operator<<(QDebug debug, const GfMatrix4d& matrix);
QDebug operator<<(QDebug debug, const CameraUtilFraming& framing);
QDebug operator<<(QDebug debug, const SdfPath& path);
QDebug operator<<(QDebug debug, const TfToken& token);
QDebug operator<<(QDebug debug, const pxr::TfTokenVector& tokens);
QDebug operator<<(QDebug debug, const UsdTimeCode& timeCode);
QDebug operator<<(QDebug debug, UsdImagingGLDrawMode drawMode);
QDebug operator<<(QDebug debug, UsdImagingGLCullStyle cullStyle);
QDebug operator<<(QDebug debug, const UsdImagingGLRenderParams& params);
QDebug operator<<(QDebug debug, const UsdImagingGLRenderParams::BBoxVector& bboxes);
QDebug operator<<(QDebug debug, const UsdImagingGLRenderParams::ClipPlanesVector& clipPlanes);
