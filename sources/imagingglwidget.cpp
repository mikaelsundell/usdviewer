// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "imagingglwidget.h"
#include "application.h"
#include "command.h"
#include "commanddispatcher.h"
#include "os.h"
#include "qtutils.h"
#include "signalguard.h"
#include "stageutils.h"
#include "style.h"
#include "viewcamera.h"
#include <QApplication>
#include <QColor>
#include <QColorSpace>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QLocale>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPointer>
#include <pxr/base/tf/error.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/imaging/glf/diagnostic.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {
class ImagingGLWidgetPrivate : public QObject, public SignalGuard {
public:
    void init();
    void initGL();
    void initCamera();
    void close();
    void frame(const GfBBox3d& bbox);
    void resetView();
    void paintGL();
    void paintEvent(QPaintEvent* event);
    void focusEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void sweepEvent(const QRect& rect, QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);
    void updateStage(UsdStageRefPtr stage);
    void updateBoundingBox(const GfBBox3d& bbox);
    void updateMask(const QList<SdfPath>& paths);
    void updatePrims(const QList<SdfPath>& paths);
    void updateSelection(const QList<SdfPath>& paths);

public:
    QPoint deviceRatio(QPoint value) const;
    double deviceRatio(double value) const;
    double widgetAspectRatio() const;
    GfVec2i widgetSize() const;
    GfVec4d widgetViewport() const;
    void drawBorder(QPainter& painter);
    void drawAxis(QPainter& painter);
    void updateSceneTree();
    void updateGpuPerformance();
    struct Data {
        size_t count;
        qint64 frame;
        QString aov;
        QColor clearColor;
        float defaultAmbient;
        float defaultSpecular;
        float defaultShininess;
        double gpuPerformanceMs;
        bool defaultCameraLightEnabled;
        bool sceneLightsEnabled;
        bool sceneShadersEnabled;
        bool sceneTreeEnabled;
        bool gpuPerformanceEnabled;
        bool cameraAxisEnabled;
        bool drag;
        bool sweep;
        QPoint start;
        QPoint end;
        QPoint mousepos;
        QImage sceneTree;
        QImage gpuPerformance;
        ViewCamera viewCamera;
        GfBBox3d selectionBBox;
        ImagingGLWidget::draw_mode drawMode;
        UsdStageRefPtr stage;
        UsdImagingGLRenderParams params;
        GfBBox3d bbox;
        QList<SdfPath> mask;
        QList<SdfPath> selection;
        QScopedPointer<UsdImagingGLEngine> glEngine;
        QPointer<ImagingGLWidget> glwidget;
    };
    Data d;
};

void
ImagingGLWidgetPrivate::init()
{
    attach(d.glwidget);
    QSurfaceFormat format;
    format.setSamples(4);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    format.setColorSpace(QColorSpace::SRgb);
    d.glwidget->setFormat(format);
    d.count = 0;
    d.frame = 0;
    d.aov = "color";
    d.defaultAmbient = 0.4f;
    d.defaultSpecular = 0.5f;
    d.defaultShininess = 32.0f;
    d.gpuPerformanceMs = 0.0;
    d.defaultCameraLightEnabled = true;
    d.sceneLightsEnabled = true;
    d.sceneShadersEnabled = false;
    d.sceneTreeEnabled = true;
    d.gpuPerformanceEnabled = false;
    d.cameraAxisEnabled = true;
    d.drag = false;
    d.drawMode = ImagingGLWidget::draw_shadedsmooth;
}

void
ImagingGLWidgetPrivate::initGL()
{
    if (!d.glEngine) {
        UsdImagingGLEngine::Parameters params;
        params.allowAsynchronousSceneProcessing = true;
        params.displayUnloadedPrimsWithBounds = true;
        d.glEngine.reset(new UsdImagingGLEngine(params));
        Hgi* hgi = d.glEngine->GetHgi();
        if (hgi) {
            TfToken driver = hgi->GetAPIName();
        }
        else {
            qWarning() << "could not initialize gl engine, no hydra driver found.";
            d.glEngine.reset();
        }
    }
}

void
ImagingGLWidgetPrivate::initCamera()
{
    Q_ASSERT("stage is not loaded" && d.stage);
    d.viewCamera = ViewCamera();
    d.viewCamera.setBoundingBox(d.bbox);
    TfToken upAxis = UsdGeomGetStageUpAxis(d.stage);
    if (upAxis == TfToken("X")) {
        d.viewCamera.setCameraUp(ViewCamera::X);
    }
    else if (upAxis == TfToken("Y")) {
        d.viewCamera.setCameraUp(ViewCamera::Y);
    }
    else if (upAxis == TfToken("Z")) {
        d.viewCamera.setCameraUp(ViewCamera::Z);
    }
    d.viewCamera.frameAll();
}

void
ImagingGLWidgetPrivate::close()
{
    d.mask.clear();
    d.glEngine.reset();
    initGL();
    d.glwidget->update();
    d.stage = nullptr;
    d.selection = QList<SdfPath>();
    if (d.sceneTreeEnabled) {
        updateSceneTree();
    }
}

void
ImagingGLWidgetPrivate::frame(const GfBBox3d& bbox)
{
    d.viewCamera.setBoundingBox(bbox);
    d.viewCamera.frameAll();
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::resetView()
{
    initCamera();
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::paintGL()
{
    glClearColor(d.clearColor.redF(), d.clearColor.greenF(), d.clearColor.blueF(), d.clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // paintGL() may be invoked by Qt before the USD stage is fully initialized.
    // Ensure the stage exists and is successfully loaded before rendering.

    if (d.stage) {
        if (d.glEngine) {
            QElapsedTimer timer;
            timer.start();
            if (!d.glEngine->IsColorCorrectionCapable()) {
                glEnable(GL_FRAMEBUFFER_SRGB);
            }
            else {
                glDisable(GL_FRAMEBUFFER_SRGB);
            }
            Q_ASSERT("aov is not set and is required" && d.aov.size());
            TfToken aovtoken(QStringToTfToken(d.aov));
            d.glEngine->SetRendererAov(aovtoken);
            GfVec4d viewport = widgetViewport();
            d.glEngine->SetRenderBufferSize(widgetSize());
            d.glEngine->SetFraming(
                CameraUtilFraming(GfRange2f(GfVec2i(), widgetSize()), GfRect2i(GfVec2i(), widgetSize())));
            d.glEngine->SetWindowPolicy(CameraUtilMatchVertically);
            d.glEngine->SetRenderViewport(viewport);
#ifdef WIN32
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
#endif
            d.viewCamera.setAspectRatio(widgetAspectRatio());
            GfCamera camera = d.viewCamera.camera();
            GfFrustum frustum = camera.GetFrustum();
            GfMatrix4d viewModel = frustum.ComputeViewMatrix();
            GfMatrix4d projectionMatrix = frustum.ComputeProjectionMatrix();
            d.glEngine->SetCameraState(viewModel, projectionMatrix);
            d.params.clearColor = QColorToGfVec4f(d.clearColor);
            // drawmode
            {
                UsdImagingGLDrawMode mode;
                switch (d.drawMode) {
                case ImagingGLWidget::draw_points: mode = UsdImagingGLDrawMode::DRAW_POINTS; break;
                case ImagingGLWidget::draw_wireframe: mode = UsdImagingGLDrawMode::DRAW_WIREFRAME; break;
                case ImagingGLWidget::draw_wireframeonsurface:
                    mode = UsdImagingGLDrawMode::DRAW_WIREFRAME_ON_SURFACE;
                    break;
                case ImagingGLWidget::draw_shadedflat: mode = UsdImagingGLDrawMode::DRAW_SHADED_FLAT; break;
                case ImagingGLWidget::draw_shadedsmooth: mode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH; break;
                case ImagingGLWidget::draw_geomonly: mode = UsdImagingGLDrawMode::DRAW_GEOM_ONLY; break;
                case ImagingGLWidget::draw_geomflat: mode = UsdImagingGLDrawMode::DRAW_GEOM_FLAT; break;
                case ImagingGLWidget::draw_geomsmooth: mode = UsdImagingGLDrawMode::DRAW_GEOM_SMOOTH; break;
                default: mode = UsdImagingGLDrawMode::DRAW_GEOM_SMOOTH;
                }
                d.params.drawMode = mode;
            }
            d.params.cullStyle = UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED;
            d.params.forceRefresh = true;
            d.params.enableLighting = true;
            // defaults
            {
                std::vector<GlfSimpleLight> lights;
                if (d.defaultCameraLightEnabled) {
                    GfCamera camera = d.viewCamera.camera();
                    GfMatrix4d viewInverse = camera.GetTransform();
                    GfVec3d camPos = viewInverse.ExtractTranslation();

                    GlfSimpleLight light;
                    light.SetAmbient(GfVec4f(0, 0, 0, 0));
                    light.SetPosition(GfVec4f(camPos[0], camPos[1], camPos[2], 1.0f));
                    light.SetTransform(viewInverse);
                    lights.push_back(light);
                }
                GfVec4f defaultAmbient(d.defaultAmbient, d.defaultAmbient, d.defaultAmbient, 1.0f);
                GlfSimpleMaterial material;
                material.SetAmbient(defaultAmbient);
                material.SetSpecular(GfVec4f(d.defaultSpecular, d.defaultSpecular, d.defaultSpecular, 1.0f));
                material.SetShininess(d.defaultShininess);
                d.glEngine->SetLightingState(lights, material, defaultAmbient);
            }
            d.params.enableSampleAlphaToCoverage = true;
            d.params.enableSceneLights = d.sceneLightsEnabled;
            d.params.enableSceneMaterials = d.sceneShadersEnabled;  // shaders not materials
            d.params.flipFrontFacing = true;
            d.params.highlight = true;
            d.params.showGuides = false;
            d.params.showProxy = true;
            d.params.showRender = true;
            QElapsedTimer gpuTimer;
            gpuTimer.start();
            TfErrorMark mark;
            {
                QReadLocker locker(CommandDispatcher::stageLock());
                Hgi* hgi = d.glEngine->GetHgi();
                hgi->StartFrame();
                UsdPrim root = d.stage->GetPseudoRoot();
                if (!d.mask.isEmpty()) {
                    SdfPathVector paths;
                    for (const SdfPath& path : d.mask)
                        paths.push_back(path);
                    d.glEngine->PrepareBatch(root, d.params);
                    d.glEngine->RenderBatch(paths, d.params);
                }
                else {
                    d.glEngine->Render(root, d.params);
                }
                hgi->EndFrame();
            }
            if (!mark.IsClean()) {
                qWarning() << "gl engine errors occured during rendering";
            }
            qint64 gpuTimeNSecs = gpuTimer.nsecsElapsed();
            d.gpuPerformanceMs = gpuTimeNSecs / 1e6;
            d.count++;
            Q_EMIT d.glwidget->renderReady(timer.elapsed());
        }
        else {
            qWarning() << "gl engine is not inititialized, render pass will be skipped";
        }
    }
    if (d.gpuPerformanceEnabled) {
        updateGpuPerformance();
    }
}

void
ImagingGLWidgetPrivate::paintEvent(QPaintEvent* event)
{
    QPainter painter(d.glwidget);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (d.sweep) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);
        QRect rect(d.start, d.end);
        rect = rect.normalized();
        painter.setPen(QPen(QColor(0, 150, 255, 200), 1));
        painter.setBrush(QColor(0, 150, 255, 50));
        painter.drawRect(rect);
        painter.restore();
    }
    if (d.sceneTreeEnabled) {
        painter.drawImage(QPoint(0, 0), d.sceneTree);
    }
    if (d.gpuPerformanceEnabled) {
        int marginRight = 24;
        QPoint pos(d.glwidget->width() - d.gpuPerformance.width() / d.gpuPerformance.devicePixelRatio() - marginRight,
                   0);
        painter.drawImage(pos, d.gpuPerformance);
    }
    if (d.cameraAxisEnabled) {
        drawAxis(painter);
    }
    drawBorder(painter);
}

void
ImagingGLWidgetPrivate::focusEvent(QMouseEvent* event)
{
    d.glwidget->makeCurrent();
    if (!d.stage || !d.glEngine)
        return;

#ifdef WIN32
    glDepthMask(GL_TRUE);
#endif

    const qreal deviceRatio = d.glwidget->devicePixelRatioF();
    QPointF mousePosDevice = event->pos() * deviceRatio;
    GfVec4d viewport = widgetViewport();

    GfVec2d pos((mousePosDevice.x() - viewport[0]) / static_cast<double>(viewport[2]),
                (mousePosDevice.y() - viewport[1]) / static_cast<double>(viewport[3]));
    pos[0] = pos[0] * 2.0 - 1.0;
    pos[1] = -1.0 * (pos[1] * 2.0 - 1.0);

    GfVec2d size(1.0 / static_cast<double>(viewport[2]), 1.0 / static_cast<double>(viewport[3]));

    GfCamera camera = d.viewCamera.camera();
    GfFrustum frustum = camera.GetFrustum();
    GfFrustum pickFrustum = frustum.ComputeNarrowedFrustum(pos, size);

    GfVec3d hitPoint, hitNormal;
    SdfPath hitPrimPath, hitInstancerPath;

    bool hit = d.glEngine->TestIntersection(pickFrustum.ComputeViewMatrix(), pickFrustum.ComputeProjectionMatrix(),
                                            d.stage->GetPseudoRoot(), d.params, &hitPoint, &hitNormal, &hitPrimPath,
                                            &hitInstancerPath);

    if (hit) {
        d.viewCamera.setFocusPoint(hitPoint);
        d.glwidget->update();
    }
}

void
ImagingGLWidgetPrivate::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) {
        focusEvent(event);
    }
}

void
ImagingGLWidgetPrivate::mousePressEvent(QMouseEvent* event)
{
    if (d.stage) {
        if (event->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) {
            d.drag = true;
            if (event->button() == Qt::LeftButton) {
                d.viewCamera.setCameraMode(ViewCamera::Tumble);
            }
            else if (event->button() == Qt::MiddleButton) {
                d.viewCamera.setCameraMode(ViewCamera::Truck);
            }
            else if (event->button() == Qt::RightButton) {
                d.viewCamera.setCameraMode(ViewCamera::Zoom);
            }
        }
        else {
            d.sweep = true;
            d.start = event->pos();
            d.end = event->pos();
        }
        d.mousepos = event->pos();
    }
}

void
ImagingGLWidgetPrivate::mouseMoveEvent(QMouseEvent* event)
{
    if (d.stage) {
        QPoint pos = event->pos();
        if (d.drag) {
            QPoint delta = deviceRatio(pos) - deviceRatio(d.mousepos);
            if (d.viewCamera.cameraMode() == ViewCamera::Truck) {
                double height = widgetSize()[1];
                double factor = d.viewCamera.mapToFrustumHeight(height);
                d.viewCamera.truck(-delta.x() * factor, delta.y() * factor);
            }
            else if (d.viewCamera.cameraMode() == ViewCamera::Tumble) {
                d.viewCamera.tumble(0.25 * delta.x(), 0.25 * delta.y());
            }
            else if (d.viewCamera.cameraMode() == ViewCamera::Zoom) {
                double factor = -.002 * (delta.x() + delta.y());
                d.viewCamera.distance(1 + factor);
            }
            d.glwidget->update();
        }
        else if (d.sweep) {
            d.end = event->pos();
            d.glwidget->update();
        }
        d.mousepos = event->pos();
    }
}

void
ImagingGLWidgetPrivate::mouseReleaseEvent(QMouseEvent* event)
{
    if (d.stage) {
        if (d.drag) {
            d.drag = false;
            d.viewCamera.setCameraMode(ViewCamera::None);
        }
        else if (d.sweep) {
            d.end = event->pos();
            QRect rect(d.start, d.end);
            sweepEvent(rect, event);
            d.sweep = false;
        }
    }
}

void
ImagingGLWidgetPrivate::sweepEvent(const QRect& rect, QMouseEvent* event)
{
    d.glwidget->makeCurrent();
    if (!d.stage || !d.glEngine)
        return;

#ifdef WIN32
    glDepthMask(GL_TRUE);
#endif
    QRect r = rect.normalized();
    QPoint tl = deviceRatio(r.topLeft());
    QPoint br = deviceRatio(r.bottomRight() - QPoint(1, 1));
    r = QRect(tl, br);

    const int minSize = 10;
    const bool isClick = (r.width() < 3 && r.height() < 3);

    if (isClick) {
        int cx = r.center().x();
        int cy = r.center().y();

        int halfW = minSize / 2;
        int halfH = minSize / 2;

        r = QRect(QPoint(cx - halfW, cy - halfH), QPoint(cx + halfW, cy + halfH));
    }
    GfVec4d viewport = widgetViewport();
    GfVec2d center(((r.left() + r.right()) * 0.5 - viewport[0]) / viewport[2],
                   ((r.top() + r.bottom()) * 0.5 - viewport[1]) / viewport[3]);
    center[0] = center[0] * 2.0 - 1.0;
    center[1] = -1.0 * (center[1] * 2.0 - 1.0);

    GfVec2d size(double(r.width()) / viewport[2], double(r.height()) / viewport[3]);

    UsdImagingGLEngine::PickParams pickParams;
    pickParams.resolveMode = isClick ? TfToken("resolveNearestToCenter") : TfToken("resolveDeep");

    GfCamera cam = d.viewCamera.camera();
    GfFrustum fr = cam.GetFrustum();
    GfFrustum pickFr = fr.ComputeNarrowedFrustum(center, size);

    UsdImagingGLEngine::IntersectionResultVector results;
    const bool hit = d.glEngine->TestIntersection(pickParams, pickFr.ComputeViewMatrix(),
                                                  pickFr.ComputeProjectionMatrix(), d.stage->GetPseudoRoot(), d.params,
                                                  &results);

    QList<SdfPath> selectedPaths;
    if (hit) {
        for (const auto& rItem : results) {
            if (!rItem.hitPrimPath.IsEmpty())
                selectedPaths.append(rItem.hitPrimPath);
        }
    }

    bool update = false;
    if (!selectedPaths.isEmpty()) {
        if (event->modifiers() & Qt::ShiftModifier) {
            for (const SdfPath& path : selectedPaths) {
                qsizetype idx = d.selection.indexOf(path);
                if (idx >= 0)
                    d.selection.removeAt(idx);
                else
                    d.selection.append(path);
                update = true;
            }
        }
        else {
            if (d.selection != selectedPaths) {
                d.selection = selectedPaths;
                update = true;
            }
        }
    }
    else {
        if (!d.selection.isEmpty()) {
            d.selection.clear();
            update = true;
        }
    }

    if (update) {
        CommandDispatcher::run(new Command(selectPaths(d.selection)));
    }

    d.glwidget->update();
}


void
ImagingGLWidgetPrivate::wheelEvent(QWheelEvent* event)
{
    double delta = static_cast<double>(event->angleDelta().y()) / 1000.0;
    double clamped = std::max(-0.5, std::min(0.5, delta));
    double factor = 1.0 - clamped;
    d.viewCamera.distance(factor);
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::updateStage(UsdStageRefPtr stage)
{
    SignalGuard::Scope guard(this);
    d.stage = stage;
    initCamera();
    d.glEngine.reset();
    initGL();
    if (d.sceneTreeEnabled) {
        updateSceneTree();
    }
}

void
ImagingGLWidgetPrivate::updateBoundingBox(const GfBBox3d& bbox)
{
    SignalGuard::Scope guard(this);
    d.bbox = bbox;
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::updateMask(const QList<SdfPath>& paths)
{
    SignalGuard::Scope guard(this);
    d.mask = paths;
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::updatePrims(const QList<SdfPath>& paths)
{
    SignalGuard::Scope guard(this);
    if (d.sceneTreeEnabled) {
        updateSceneTree();
    }
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::updateSelection(const QList<SdfPath>& paths)
{
    SignalGuard::Scope guard(this);
    Q_ASSERT("gl engine is not set" && d.glEngine);
    d.glEngine->SetSelected(QListToSdfPathVector(paths));
    updateSceneTree();
    d.glwidget->update();
    d.selection = paths;
}

QPoint
ImagingGLWidgetPrivate::deviceRatio(QPoint value) const
{
    return (QPoint(deviceRatio(value.x()), deviceRatio(value.y())));
}

double
ImagingGLWidgetPrivate::deviceRatio(double value) const
{
    return value * d.glwidget->devicePixelRatio();
}

double
ImagingGLWidgetPrivate::widgetAspectRatio() const
{
    GfVec2i size = widgetSize();
    double width = static_cast<double>(size[0]);
    double height = static_cast<double>(size[1]);
    return width / std::max(1.0, height);
}

GfVec2i
ImagingGLWidgetPrivate::widgetSize() const
{
    int w = deviceRatio(d.glwidget->width());
    int h = deviceRatio(d.glwidget->height());
    return GfVec2i(w, h);
}

GfVec4d
ImagingGLWidgetPrivate::widgetViewport() const
{
    GfVec2i size = widgetSize();
    return GfVec4d(0, 0, size[0], size[1]);
}

void
ImagingGLWidgetPrivate::drawBorder(QPainter& painter)
{
    const int w = 2;
    painter.setPen(QPen(style()->color(Style::ColorBorderAlt), w));
    painter.setBrush(Qt::NoBrush);
    QRect r = d.glwidget->rect().adjusted(w / 2, w / 2, -w / 2, -w / 2);
    painter.drawRect(r);
}

void
ImagingGLWidgetPrivate::drawAxis(QPainter& painter)
{
    GfCamera camera = d.viewCamera.camera();
    GfFrustum frustum = camera.GetFrustum();
    GfMatrix4d viewMatrix = frustum.ComputeViewMatrix();

    const GfVec3d xCam = viewMatrix.TransformDir(GfVec3d(1.0, 0.0, 0.0));
    const GfVec3d yCam = viewMatrix.TransformDir(GfVec3d(0.0, 1.0, 0.0));
    const GfVec3d zCam = viewMatrix.TransformDir(GfVec3d(0.0, 0.0, 1.0));

    const int margin = 18;
    const int radius = 30;
    const int bubbleRadius = 10;

    const QPoint center(margin + radius, d.glwidget->height() - margin - radius);

    auto toPoint = [&](const GfVec3d& dir) -> QPointF {
        return QPointF(center.x() + dir[0] * radius, center.y() - dir[1] * radius);
    };

    struct AxisLine {
        QString label;
        QColor color;
        GfVec3d dir;
    };

    QVector<AxisLine> axes { { "X", QColor(200, 50, 50), xCam },
                             { "Y", QColor(50, 170, 70), yCam },
                             { "Z", QColor(70, 110, 220), zCam } };
    std::sort(axes.begin(), axes.end(), [](const AxisLine& a, const AxisLine& b) { return a.dir[2] < b.dir[2]; });

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 20));
    painter.drawEllipse(center, radius - 10, radius - 10);

    QFont font = app()->font();
    font.setPointSize(style()->fontSize(Style::UISmall));
    font.setBold(true);
    painter.setFont(font);

    QFontMetrics fm(font);
    for (const AxisLine& axis : axes) {
        QPointF end = toPoint(axis.dir);
        painter.setPen(QPen(axis.color, 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(center, end);
        painter.setBrush(axis.color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(end, bubbleRadius, bubbleRadius);
        QRectF textRect(end.x() - bubbleRadius, end.y() - bubbleRadius, bubbleRadius * 2, bubbleRadius * 2);

        painter.setPen(QColor(0, 0, 0, 180));
        painter.drawText(textRect.translated(1, 1), Qt::AlignCenter, axis.label);
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, axis.label);
    }
    painter.restore();
}

void
ImagingGLWidgetPrivate::updateSceneTree()
{
    struct SceneStats {
        size_t prims = 0;
        size_t meshes = 0;
        size_t xforms = 0;
        size_t payloads = 0;
        size_t instances = 0;
        size_t vertices = 0;
        size_t normals = 0;
        size_t faces = 0;
    };

    auto accumulate = [&](const UsdPrim& prim, SceneStats& s) {
        if (!prim.IsActive() || !prim.IsLoaded())
            return;

        s.prims++;

        if (prim.IsA<UsdGeomXform>())
            s.xforms++;

        if (prim.IsA<UsdGeomMesh>()) {
            s.meshes++;
            UsdGeomMesh mesh(prim);

            VtArray<GfVec3f> points;
            mesh.GetPointsAttr().Get(&points);
            s.vertices += points.size();

            VtArray<int> faceCounts;
            mesh.GetFaceVertexCountsAttr().Get(&faceCounts);
            s.faces += faceCounts.size();

            VtArray<GfVec3f> meshNormals;
            UsdGeomPrimvarsAPI pvAPI(prim);
            UsdGeomPrimvar normalsPv = pvAPI.GetPrimvar(TfToken("normals"));

            bool hasNormals = false;
            if (normalsPv && normalsPv.HasValue()) {
                normalsPv.Get(&meshNormals);
                hasNormals = true;
            }
            if (!hasNormals) {
                mesh.GetNormalsAttr().Get(&meshNormals);
            }

            s.normals += meshNormals.size();
        }

        if (prim.HasPayload())
            s.payloads++;

        if (prim.IsInstanceable())
            s.instances++;
    };

    auto filterRootPaths = [](const QList<SdfPath>& paths) {
        QList<SdfPath> result;
        for (const SdfPath& p : paths) {
            bool isChild = false;
            for (const SdfPath& other : paths) {
                if (p == other)
                    continue;
                if (p.HasPrefix(other)) {
                    isChild = true;
                    break;
                }
            }
            if (!isChild)
                result.append(p);
        }
        return result;
    };

    SceneStats total;
    SceneStats selected;
    if (d.stage) {
        QReadLocker locker(CommandDispatcher::stageLock());
        for (const UsdPrim& prim : d.stage->Traverse()) {
            accumulate(prim, total);
        }
        if (!d.selection.isEmpty()) {
            const QList<SdfPath> roots = filterRootPaths(d.selection);

            for (const SdfPath& path : roots) {
                UsdPrim root = d.stage->GetPrimAtPath(path);
                if (!root)
                    continue;

                for (const UsdPrim& prim : UsdPrimRange(root)) {
                    accumulate(prim, selected);
                }
            }
        }
    }

    QLocale locale = QLocale::system();
    auto fmt = [&](size_t v) { return locale.toString((qlonglong)v); };

    const bool hasSelection = !d.selection.isEmpty();
    auto fmtPair = [&](size_t totalValue, size_t selectedValue) {
        if (hasSelection && selectedValue > 0)
            return QString("%1 (%2)").arg(fmt(totalValue), fmt(selectedValue));
        return fmt(totalValue);
    };

    struct Row {
        QString label;
        QString value;
    };

    QVector<Row> rows { { "Prims", fmtPair(total.prims, selected.prims) },
                        { "Meshes", fmtPair(total.meshes, selected.meshes) },
                        { "Xforms", fmtPair(total.xforms, selected.xforms) },
                        { "Payloads", fmtPair(total.payloads, selected.payloads) },
                        { "Instances", fmtPair(total.instances, selected.instances) },
                        { "Vertices", fmtPair(total.vertices, selected.vertices) },
                        { "Normals", fmtPair(total.normals, selected.normals) },
                        { "Faces", fmtPair(total.faces, selected.faces) } };

    double dpr = d.glwidget->devicePixelRatioF();

    QFont font = app()->font();
    font.setPointSize(style()->fontSize(Style::UISmall));
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);

    QFontMetrics fm(font);
    int rowHeight = fm.lineSpacing() + 2;
    int marginLeft = 18;
    int marginTop = 16;
    int columnSpacing = 20;

    int labelWidth = 0;
    int valueWidth = 0;

    for (const auto& r : rows) {
        labelWidth = std::max(labelWidth, fm.horizontalAdvance(r.label));
        valueWidth = std::max(valueWidth, fm.horizontalAdvance(r.value));
    }

    qsizetype width = labelWidth + columnSpacing + valueWidth + marginLeft;
    qsizetype height = rows.size() * rowHeight + marginTop;

    d.sceneTree = QImage(width * dpr, height * dpr, QImage::Format_ARGB32_Premultiplied);
    d.sceneTree.setDevicePixelRatio(dpr);
    d.sceneTree.fill(Qt::transparent);

    QPainter p(&d.sceneTree);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(font);

    int y = marginTop + fm.ascent();
    for (const auto& r : rows) {
        int labelX = marginLeft;
        int valueX = marginLeft + labelWidth + columnSpacing;
        p.setPen(QColor(0, 0, 0, 180));
        p.drawText(labelX + 1, y + 1, r.label);
        p.drawText(valueX + 1, y + 1, r.value);

        p.setPen(Qt::white);
        p.drawText(labelX, y, r.label);
        p.drawText(valueX, y, r.value);

        y += rowHeight;
    }
}

void
ImagingGLWidgetPrivate::updateGpuPerformance()
{
    const VtDictionary stats = d.glEngine->GetRenderStats();
    QLocale locale = QLocale::system();
    auto fmtMB = [&](unsigned long bytes) {
        return QString::number(double(bytes) / (1024.0 * 1024.0), 'f', 2) + " MB";
    };

    struct Row {
        QString label;
        QString value;
    };

    double fps = 0.0;
    if (d.gpuPerformanceMs > 0.0)
        fps = 1000.0 / d.gpuPerformanceMs;

    QVector<Row> rows;
    rows.append({ "GPU time", QString::number(d.gpuPerformanceMs, 'f', 2) + " ms" });
    rows.append({ "GPU fps", QString::number(fps, 'f', 1) });
    if (stats.count("gpuMemoryUsed"))
        rows.append({ "GPU mem", fmtMB(VtDictionaryGet<unsigned long>(stats, "gpuMemoryUsed")) });
    if (stats.count("primvar"))
        rows.append({ " primvar", fmtMB(VtDictionaryGet<unsigned long>(stats, "primvar")) });
    if (stats.count("topology"))
        rows.append({ " topology", fmtMB(VtDictionaryGet<unsigned long>(stats, "topology")) });
    if (stats.count("drawingShader"))
        rows.append({ " shader", fmtMB(VtDictionaryGet<unsigned long>(stats, "drawingShader")) });
    if (stats.count("textureMemory"))
        rows.append({ " texture", fmtMB(VtDictionaryGet<unsigned long>(stats, "textureMemory")) });

    double dpr = d.glwidget->devicePixelRatioF();
    QFont font = app()->font();
    font.setPointSize(style()->fontSize(Style::UISmall));
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);

    QFontMetrics fm(font);
    int rowHeight = fm.lineSpacing() + 2;
    int marginLeft = 18;
    int marginTop = 16;
    int columnSpacing = 20;
    int labelWidth = 0;
    int valueWidth = 0;

    for (const auto& r : rows) {
        labelWidth = std::max(labelWidth, fm.horizontalAdvance(r.label));
        valueWidth = std::max(valueWidth, fm.horizontalAdvance(r.value));
    }

    qsizetype width = labelWidth + columnSpacing + valueWidth + marginLeft;
    qsizetype height = rows.size() * rowHeight + marginTop;

    d.gpuPerformance = QImage(width * dpr, height * dpr, QImage::Format_ARGB32_Premultiplied);
    d.gpuPerformance.setDevicePixelRatio(dpr);
    d.gpuPerformance.fill(Qt::transparent);

    QPainter p(&d.gpuPerformance);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(font);

    int y = marginTop + fm.ascent();

    for (const auto& r : rows) {
        int labelX = marginLeft;
        int valueX = marginLeft + labelWidth + columnSpacing;

        p.setPen(QColor(0, 0, 0, 180));
        p.drawText(labelX + 1, y + 1, r.label);
        p.drawText(valueX + 1, y + 1, r.value);

        p.setPen(Qt::white);
        p.drawText(labelX, y, r.label);
        p.drawText(valueX, y, r.value);

        y += rowHeight;
    }
}

ImagingGLWidget::ImagingGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , p(new ImagingGLWidgetPrivate())
{
    p->d.glwidget = this;
    p->init();
}

ImagingGLWidget::~ImagingGLWidget() = default;

ViewCamera
ImagingGLWidget::viewCamera() const
{
    return p->d.viewCamera;
}

QImage
ImagingGLWidget::captureImage()
{
    return QOpenGLWidget::grabFramebuffer();
}

void
ImagingGLWidget::close()
{
    p->close();
}

void
ImagingGLWidget::frame(const GfBBox3d& bbox)
{
    p->frame(bbox);
}

void
ImagingGLWidget::resetView()
{
    p->resetView();
}

ImagingGLWidget::draw_mode
ImagingGLWidget::drawMode() const
{
    return p->d.drawMode;
}

void
ImagingGLWidget::setDrawMode(draw_mode drawMode)
{
    if (drawMode != p->d.drawMode) {
        p->d.drawMode = drawMode;
        update();
    }
}

QColor
ImagingGLWidget::clearColor() const
{
    return p->d.clearColor;
}

void
ImagingGLWidget::setClearColor(const QColor& color)
{
    if (color != p->d.clearColor) {
        p->d.clearColor = color;
        update();
    }
}

bool
ImagingGLWidget::defaultCameraLightEnabled() const
{
    return p->d.defaultCameraLightEnabled;
}

void
ImagingGLWidget::enableDefaultCameraLight(bool enabled)
{
    if (enabled != p->d.defaultCameraLightEnabled) {
        p->d.defaultCameraLightEnabled = enabled;
        update();
    }
}

bool
ImagingGLWidget::sceneLightsEnabled() const
{
    return p->d.sceneLightsEnabled;
}

void
ImagingGLWidget::enableSceneLights(bool enabled)
{
    if (enabled != p->d.defaultCameraLightEnabled) {
        p->d.defaultCameraLightEnabled = enabled;
        update();
    }
}

bool
ImagingGLWidget::sceneShadersEnabled() const
{
    return p->d.sceneShadersEnabled;
}

void
ImagingGLWidget::enableSceneShaders(bool enabled)
{
    if (enabled != p->d.sceneShadersEnabled) {
        p->d.sceneShadersEnabled = enabled;
        update();
    }
}

bool
ImagingGLWidget::sceneTreeEnabled() const
{
    return p->d.sceneTreeEnabled;
}

void
ImagingGLWidget::enableSceneTree(bool enabled)
{
    if (enabled != p->d.sceneTreeEnabled) {
        p->d.sceneTreeEnabled = enabled;
        p->updateSceneTree();
        update();
    }
}

bool
ImagingGLWidget::gpuPerformanceEnabled() const
{
    return p->d.gpuPerformanceEnabled;
}

void
ImagingGLWidget::enableGpuPerformance(bool enabled)
{
    if (enabled != p->d.gpuPerformanceEnabled) {
        p->d.gpuPerformanceEnabled = enabled;
        update();
    }
}

bool
ImagingGLWidget::cameraAxisEnabled() const
{
    return p->d.cameraAxisEnabled;
}

void
ImagingGLWidget::enableCameraAxis(bool enabled)
{
    if (enabled != p->d.cameraAxisEnabled) {
        p->d.cameraAxisEnabled = enabled;
        update();
    }
}

QList<QString>
ImagingGLWidget::rendererAovs() const
{
    Q_ASSERT("gl engine is not inititialized" && p->d.glEngine);
    return TfTokenVectorToQList(p->d.glEngine->GetRendererAovs());
}

void
ImagingGLWidget::setRendererAov(const QString& aov)
{
    if (p->d.aov != aov) {
        p->d.aov = aov;
        update();
    }
}

void
ImagingGLWidget::updateStage(UsdStageRefPtr stage)
{
    p->updateStage(stage);
}

void
ImagingGLWidget::updateBoundingBox(const GfBBox3d& bbox)
{
    p->updateBoundingBox(bbox);
}

void
ImagingGLWidget::updateMask(const QList<SdfPath>& paths)
{
    p->updateMask(paths);
}

void
ImagingGLWidget::updatePrims(const QList<SdfPath>& paths)
{
    p->updatePrims(paths);
}

void
ImagingGLWidget::updateSelection(const QList<SdfPath>& paths)
{
    p->updateSelection(paths);
}

void
ImagingGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    p->initGL();
}

void
ImagingGLWidget::paintGL()
{
    p->paintGL();
}

void
ImagingGLWidget::paintEvent(QPaintEvent* event)
{
    QOpenGLWidget::paintEvent(event);
    p->paintEvent(event);
}

void
ImagingGLWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    p->mouseDoubleClickEvent(event);
}

void
ImagingGLWidget::mousePressEvent(QMouseEvent* event)
{
    p->mousePressEvent(event);
}

void
ImagingGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    p->mouseMoveEvent(event);
}

void
ImagingGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    p->mouseReleaseEvent(event);
}

void
ImagingGLWidget::wheelEvent(QWheelEvent* event)
{
    p->wheelEvent(event);
}
}  // namespace usdviewer
