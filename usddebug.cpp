
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usddebug.h"

void CheckOpenGLError(const char* function, const char* file, int line) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        qWarning() << "OpenGL Error:" << err
                   << " in " << function
                   << " at " << file << ":" << line;
    }
}

QDebug operator<<(QDebug debug, const UsdImagingGLRenderParams::BBoxVector& bboxes) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "BBoxVector { ";

    for (const auto& bbox : bboxes) {
        const GfRange3d& range = bbox.GetRange();
        debug.nospace() << "[Min: (" << range.GetMin()[0] << ", " << range.GetMin()[1] << ", " << range.GetMin()[2]
                        << ") Max: (" << range.GetMax()[0] << ", " << range.GetMax()[1] << ", " << range.GetMax()[2]
                        << ")], ";
    }
    debug.nospace() << " }";
    return debug;
}

QDebug operator<<(QDebug debug, const GfRange2d& range) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "GfRange2d(Min: ["
                    << range.GetMin()[0] << ", " << range.GetMin()[1]
                    << "] Max: ["
                    << range.GetMax()[0] << ", " << range.GetMax()[1]
                    << "])";
    return debug;
}

QDebug operator<<(QDebug debug, const GfVec2i& vec) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "GfVec2i("
                    << vec[0] << ", " << vec[1]
                    << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const GfVec4d& vec) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "GfVec4d("
                    << vec[0] << ", " << vec[1] << ", " << vec[2] << ", " << vec[3]
                    << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const GfVec4f& vec) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "GfVec4f("
                    << vec[0] << ", " << vec[1] << ", " << vec[2] << ", " << vec[3]
                    << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const GfMatrix4d& matrix) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "GfMatrix4d(\n";
    for (int row = 0; row < 4; ++row) {
        debug.nospace() << "  [ ";
        for (int col = 0; col < 4; ++col) {
            debug.nospace() << matrix[row][col] << " ";
        }
        debug.nospace() << "]\n";
    }
    debug.nospace() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const CameraUtilFraming& framing) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "CameraUtilFraming(\n"
                    << "  Display Window: " << framing.displayWindow << "\n"
                    << "  Data Window: ["
                    << framing.dataWindow.GetMinX() << ", "
                    << framing.dataWindow.GetMinY() << ", "
                    << framing.dataWindow.GetMaxX() << ", "
                    << framing.dataWindow.GetMaxY() << "]\n"
                    << "  Pixel Aspect Ratio: " << framing.pixelAspectRatio
                    << "\n)";
    return debug;
}

QDebug operator<<(QDebug debug, const TfToken& token) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "TfToken(\"" << token.GetString().c_str() << "\")";
    return debug;
}

QDebug operator<<(QDebug debug, const UsdTimeCode& timeCode) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "UsdTimeCode(" << timeCode.GetValue() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, UsdImagingGLDrawMode drawMode) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "UsdImagingGLDrawMode(";
    switch (drawMode) {
        case UsdImagingGLDrawMode::DRAW_POINTS: debug.nospace() << "DRAW_POINTS"; break;
        case UsdImagingGLDrawMode::DRAW_WIREFRAME: debug.nospace() << "DRAW_WIREFRAME"; break;
        case UsdImagingGLDrawMode::DRAW_WIREFRAME_ON_SURFACE: debug.nospace() << "DRAW_WIREFRAME_ON_SURFACE"; break;
        case UsdImagingGLDrawMode::DRAW_SHADED_FLAT: debug.nospace() << "DRAW_SHADED_FLAT"; break;
        case UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH: debug.nospace() << "DRAW_SHADED_SMOOTH"; break;
        case UsdImagingGLDrawMode::DRAW_GEOM_ONLY: debug.nospace() << "DRAW_GEOM_ONLY"; break;
        case UsdImagingGLDrawMode::DRAW_GEOM_FLAT: debug.nospace() << "DRAW_GEOM_FLAT"; break;
        case UsdImagingGLDrawMode::DRAW_GEOM_SMOOTH: debug.nospace() << "DRAW_GEOM_SMOOTH"; break;
        default: debug.nospace() << "UNKNOWN"; break;
    }
    debug.nospace() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, UsdImagingGLCullStyle cullStyle) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "UsdImagingGLCullStyle(";

    switch (cullStyle) {
        case UsdImagingGLCullStyle::CULL_STYLE_NO_OPINION: debug.nospace() << "CULL_STYLE_NO_OPINION"; break;
        case UsdImagingGLCullStyle::CULL_STYLE_NOTHING: debug.nospace() << "CULL_STYLE_NOTHING"; break;
        case UsdImagingGLCullStyle::CULL_STYLE_BACK: debug.nospace() << "CULL_STYLE_BACK"; break;
        case UsdImagingGLCullStyle::CULL_STYLE_FRONT: debug.nospace() << "CULL_STYLE_FRONT"; break;
        case UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED: debug.nospace() << "CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED"; break;
        case UsdImagingGLCullStyle::CULL_STYLE_COUNT: debug.nospace() << "CULL_STYLE_COUNT"; break;
        default: debug.nospace() << "UNKNOWN"; break;
    }
    debug.nospace() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const UsdImagingGLRenderParams& params) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "UsdImagingGLRenderParams("
                    << "\n  frame: " << params.frame
                    << "\n  complexity: " << params.complexity
                    << "\n  drawMode: " << params.drawMode
                    << "\n  showGuides: " << params.showGuides
                    << "\n  showProxy: " << params.showProxy
                    << "\n  showRender: " << params.showRender
                    << "\n  forceRefresh: " << params.forceRefresh
                    << "\n  flipFrontFacing: " << params.flipFrontFacing
                    << "\n  cullStyle: " << params.cullStyle
                    << "\n  enableIdRender: " << params.enableIdRender
                    << "\n  enableLighting: " << params.enableLighting
                    << "\n  enableSampleAlphaToCoverage: " << params.enableSampleAlphaToCoverage
                    << "\n  applyRenderState: " << params.applyRenderState
                    << "\n  gammaCorrectColors: " << params.gammaCorrectColors
                    << "\n  highlight: " << params.highlight
                    << "\n  overrideColor: " << params.overrideColor
                    << "\n  wireframeColor: " << params.wireframeColor
                    << "\n  alphaThreshold: " << params.alphaThreshold
                    << "\n  clipPlanes: " << params.clipPlanes
                    << "\n  enableSceneMaterials: " << params.enableSceneMaterials
                    << "\n  enableSceneLights: " << params.enableSceneLights
                    << "\n  enableUsdDrawModes: " << params.enableUsdDrawModes
                    << "\n  clearColor: " << params.clearColor
                    << "\n  colorCorrectionMode: " << params.colorCorrectionMode
                    << "\n  lut3dSizeOCIO: " << params.lut3dSizeOCIO
                    << "\n  ocioDisplay: " << params.ocioDisplay
                    << "\n  ocioView: " << params.ocioView
                    << "\n  ocioColorSpace: " << params.ocioColorSpace
                    << "\n  ocioLook: " << params.ocioLook
                    << "\n  bboxes: " << params.bboxes
                    << "\n  bboxLineColor: " << params.bboxLineColor
                    << "\n  bboxLineDashSize: " << params.bboxLineDashSize
                    << "\n)";
    return debug;
}
