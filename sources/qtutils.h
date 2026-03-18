// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QOpenGLFunctions>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quaternion.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/base/gf/range2d.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {
namespace qt {

    /** @name Qt / USD Conversions */
    ///@{

    /**
     * @brief Converts a QString to std::string.
     */
    std::string QStringToString(const QString& str);

    /**
     * @brief Converts a std::string to QString.
     */
    QString StringToQString(const std::string& str);

    /**
     * @brief Converts a QString to a USD TfToken.
     */
    TfToken QStringToTfToken(const QString& str);

    /**
     * @brief Converts a USD TfToken to QString.
     */
    QString TfTokenToQString(const TfToken& token);

    /**
     * @brief Converts a TfTokenVector to a QList of QString.
     */
    QList<QString> TfTokenVectorToQList(const TfTokenVector& tokens);

    /**
     * @brief Converts a QList of SdfPath to an SdfPathVector.
     */
    SdfPathVector QListToSdfPathVector(const QList<SdfPath>& paths);

    /**
     * @brief Converts a QColor to a USD GfVec4f.
     *
     * The resulting vector stores normalized RGBA components.
     */
    GfVec4f QColorToGfVec4f(const QColor& color);

    ///@}

}  // namespace qt
}  // namespace usdviewer

PXR_NAMESPACE_OPEN_SCOPE

using namespace usdviewer::qt;

/**
 * @brief Qt hash function for SdfPath.
 *
 * Allows SdfPath to be used as a key in Qt hash containers
 * such as QHash and QSet.
 */
inline size_t
qHash(const SdfPath& path, size_t seed = 0)
{
    return ::qHash(StringToQString(path.GetString()), seed);
}

PXR_NAMESPACE_CLOSE_SCOPE

/** @name Debug Utilities */
///@{

/**
 * @brief Checks for OpenGL errors and reports the source location.
 *
 * Intended to be used through the CHECK_GL_ERROR() macro.
 */
void
CheckOpenGLError(const char* function, const char* file, int line);

/**
 * @brief Convenience macro for OpenGL error checking.
 */
#define CHECK_GL_ERROR() CheckOpenGLError(__FUNCTION__, __FILE__, __LINE__)

/**
 * @brief Prints all prims in a USD stage for debugging.
 */
void
DebugStagePrims(const UsdStageRefPtr& stage);

/**
 * @brief Prints bounding boxes for prims in a USD stage.
 */
void
DebugBoundingBoxes(const UsdStageRefPtr& stage);

///@}

/** @name QDebug Stream Operators */
///@{

/**
 * @brief Debug stream operator for GfBBox3d.
 */
QDebug
operator<<(QDebug debug, const GfBBox3d& bbox);

/**
 * @brief Debug stream operator for GfRange1d.
 */
QDebug
operator<<(QDebug debug, const GfRange1d& range);

/**
 * @brief Debug stream operator for GfRange1f.
 */
QDebug
operator<<(QDebug debug, const GfRange1f& range);

/**
 * @brief Debug stream operator for GfRange2d.
 */
QDebug
operator<<(QDebug debug, const GfRange2d& range);

/**
 * @brief Debug stream operator for GfRotation.
 */
QDebug
operator<<(QDebug debug, const GfRotation& rotation);

/**
 * @brief Debug stream operator for GfVec2d.
 */
QDebug
operator<<(QDebug debug, const GfVec2d& vec);

/**
 * @brief Debug stream operator for GfVec2i.
 */
QDebug
operator<<(QDebug debug, const GfVec2i& vec);

/**
 * @brief Debug stream operator for GfVec3d.
 */
QDebug
operator<<(QDebug debug, const GfVec3d& vec);

/**
 * @brief Debug stream operator for GfVec4d.
 */
QDebug
operator<<(QDebug debug, const GfVec4d& vec);

/**
 * @brief Debug stream operator for GfVec4f.
 */
QDebug
operator<<(QDebug debug, const GfVec4f& vec);

/**
 * @brief Debug stream operator for GfMatrix4d.
 */
QDebug
operator<<(QDebug debug, const GfMatrix4d& matrix);

/**
 * @brief Debug stream operator for GfQuaternion.
 */
QDebug
operator<<(QDebug debug, const GfQuaternion& quat);

/**
 * @brief Debug stream operator for GfRange3d.
 */
QDebug
operator<<(QDebug debug, const GfRange3d& range);

/**
 * @brief Debug stream operator for GfCamera.
 */
QDebug
operator<<(QDebug debug, const GfCamera& camera);

/**
 * @brief Debug stream operator for GfFrustum.
 */
QDebug
operator<<(QDebug debug, const GfFrustum& frustum);

/**
 * @brief Debug stream operator for CameraUtilFraming.
 */
QDebug
operator<<(QDebug debug, const CameraUtilFraming& framing);

/**
 * @brief Debug stream operator for SdfPath.
 */
QDebug
operator<<(QDebug debug, const SdfPath& path);

/**
 * @brief Debug stream operator for TfToken.
 */
QDebug
operator<<(QDebug debug, const TfToken& token);

/**
 * @brief Debug stream operator for TfTokenVector.
 */
QDebug
operator<<(QDebug debug, const TfTokenVector& tokens);

/**
 * @brief Debug stream operator for UsdTimeCode.
 */
QDebug
operator<<(QDebug debug, const UsdTimeCode& timeCode);

/**
 * @brief Debug stream operator for VtValue.
 */
QDebug
operator<<(QDebug debug, const VtValue& value);

/**
 * @brief Debug stream operator for VtDictionary.
 */
QDebug
operator<<(QDebug debug, const VtDictionary& dict);

/**
 * @brief Debug stream operator for UsdImagingGLDrawMode.
 */
QDebug
operator<<(QDebug debug, UsdImagingGLDrawMode drawMode);

/**
 * @brief Debug stream operator for UsdImagingGLCullStyle.
 */
QDebug
operator<<(QDebug debug, UsdImagingGLCullStyle cullStyle);

/**
 * @brief Debug stream operator for UsdImagingGLRenderParams.
 */
QDebug
operator<<(QDebug debug, const UsdImagingGLRenderParams& params);

/**
 * @brief Debug stream operator for render bounding boxes.
 */
QDebug
operator<<(QDebug debug, const UsdImagingGLRenderParams::BBoxVector& bboxes);

/**
 * @brief Debug stream operator for clip planes.
 */
QDebug
operator<<(QDebug debug, const UsdImagingGLRenderParams::ClipPlanesVector& clipPlanes);

///@}
