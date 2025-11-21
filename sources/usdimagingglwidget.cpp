// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdimagingglwidget.h"
#include "platform.h"
#include "usdutils.h"
#include "usdviewcamera.h"
#include <QColor>
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
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class ImagingGLWidgetPrivate : public QObject {
public:
    void init();
    void initGL();
    void initCamera();
    void initController();
    void initStageModel();
    void initSelection();
    void paintGL();
    void paintEvent(QPaintEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);

public Q_SLOTS:
    void loaded(const QString& filename);
    void selectionChanged(const QList<SdfPath>& paths);
    void maskChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths);
    void stageChanged();

public:
    void focusEvent(QMouseEvent* event);
    void sweepEvent(const QRect& rect, QMouseEvent* event);
    double complexityRefinement(ImagingGLWidget::Complexity complexity);
    QPoint deviceRatio(QPoint value) const;
    double deviceRatio(double value) const;
    double widgetAspectRatio() const;
    GfVec2i widgetSize() const;
    GfVec4d widgetViewport() const;
    void cleanUp();
    struct Data {
        size_t count;
        qint64 frame;
        QString aov;
        QColor clearColor;
        float defaultAmbient;
        float defaultSpecular;
        float defaultShininess;
        bool defaultCameraLightEnabled;
        bool sceneLightsEnabled;
        bool sceneMaterialsEnabled;
        bool drag;
        bool sweep;
        QPoint start;
        QPoint end;
        QPoint mousepos;
        ViewCamera viewCamera;
        GfBBox3d selectionBBox;
        ImagingGLWidget::Complexity complexity;
        ImagingGLWidget::DrawMode drawMode;
        UsdImagingGLRenderParams params;
        QList<SdfPath> mask;
        QScopedPointer<UsdImagingGLEngine> glEngine;
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<ImagingGLWidget> glwidget;
    };
    Data d;
};

void
ImagingGLWidgetPrivate::init()
{
    QSurfaceFormat format;
    format.setSamples(4);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    d.glwidget->setFormat(format);
    d.count = 0;
    d.frame = 0;
    d.aov = "color";
    d.defaultAmbient = 0.4f;
    d.defaultSpecular = 0.5f;
    d.defaultShininess = 32.0f;
    d.defaultCameraLightEnabled = true;
    d.sceneLightsEnabled = true;
    d.sceneMaterialsEnabled = true;
    d.drag = false;
    d.complexity = ImagingGLWidget::Low;
    d.drawMode = ImagingGLWidget::ShadedSmooth;
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
            qDebug() << "gl engine initialized, using hydra driver: " << driver.GetText();
        }
        else {
            qWarning() << "could not initialize gl engine, no hydra driver found.";
            d.glEngine.reset();
        }
        d.glwidget->rendererReady();
    }
}

void
ImagingGLWidgetPrivate::initCamera()
{
    Q_ASSERT("stage is not loaded" && d.stageModel->isLoaded());
    d.viewCamera = ViewCamera();
    d.viewCamera.setBoundingBox(d.stageModel->boundingBox());
    TfToken upAxis = UsdGeomGetStageUpAxis(d.stageModel->stage());
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
ImagingGLWidgetPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &ImagingGLWidgetPrivate::stageChanged);
    connect(d.stageModel.data(), &StageModel::maskChanged, this, &ImagingGLWidgetPrivate::maskChanged);
    connect(d.stageModel.data(), &StageModel::primsChanged, this, &ImagingGLWidgetPrivate::primsChanged);
}

void
ImagingGLWidgetPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this,
            &ImagingGLWidgetPrivate::selectionChanged);
}

void
ImagingGLWidgetPrivate::paintGL()
{
    glClearColor(d.clearColor.redF(), d.clearColor.greenF(), d.clearColor.blueF(), d.clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // paintGL() may be invoked by Qt before the USD stage is fully initialized.
    // Ensure the stage exists and is successfully loaded before rendering.

    if (d.stageModel && d.stageModel->isLoaded()) {
        if (d.glEngine) {
            if (!d.glEngine->IsColorCorrectionCapable()) {
                glEnable(GL_FRAMEBUFFER_SRGB);
            }
            else {
                glDisable(GL_FRAMEBUFFER_SRGB);
            }
            Q_ASSERT("aov is not set and is required" && d.aov.size());
            TfToken aovtoken(QString_TfToken(d.aov));
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
            d.params.clearColor = QColor_GfVec4f(d.clearColor);
            d.params.complexity = complexityRefinement(d.complexity);
            // drawmode
            {
                UsdImagingGLDrawMode mode;
                switch (d.drawMode) {
                case ImagingGLWidget::Points: mode = UsdImagingGLDrawMode::DRAW_POINTS; break;
                case ImagingGLWidget::Wireframe: mode = UsdImagingGLDrawMode::DRAW_WIREFRAME; break;
                case ImagingGLWidget::WireframeOnSurface: mode = UsdImagingGLDrawMode::DRAW_WIREFRAME_ON_SURFACE; break;
                case ImagingGLWidget::ShadedFlat: mode = UsdImagingGLDrawMode::DRAW_SHADED_FLAT; break;
                case ImagingGLWidget::ShadedSmooth: mode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH; break;
                case ImagingGLWidget::GeomOnly: mode = UsdImagingGLDrawMode::DRAW_GEOM_ONLY; break;
                case ImagingGLWidget::GeomFlat: mode = UsdImagingGLDrawMode::DRAW_GEOM_FLAT; break;
                case ImagingGLWidget::GeomSmooth: mode = UsdImagingGLDrawMode::DRAW_GEOM_SMOOTH; break;
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
            d.params.enableSceneMaterials = d.sceneMaterialsEnabled;
            d.params.flipFrontFacing = true;
            d.params.gammaCorrectColors = true;
            d.params.colorCorrectionMode = TfToken("sRGB");
            d.params.highlight = true;
            d.params.showGuides = false;
            d.params.showProxy = true;
            d.params.showRender = true;
            TfErrorMark mark;
            Hgi* hgi = d.glEngine->GetHgi();
            hgi->StartFrame();
            QReadLocker locker(d.stageModel->stageLock());
            UsdPrim root = d.stageModel->stage()->GetPseudoRoot();
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
            if (!mark.IsClean()) {
                qWarning() << "gl engine errors occured during rendering";
            }
            d.count++;
        }
        else {
            qWarning() << "gl engine is not inititialized, render pass will be skipped";
        }
    }
}

void
ImagingGLWidgetPrivate::paintEvent(QPaintEvent* event)
{
    if (d.sweep) {
        QPainter painter(d.glwidget);
        painter.setRenderHint(QPainter::Antialiasing, false);
        QRect rect(d.start, d.end);
        rect = rect.normalized();
        painter.setPen(QPen(QColor(0, 150, 255, 200), 1));
        painter.setBrush(QColor(0, 150, 255, 50));
        painter.drawRect(rect);
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
    if (d.stageModel->isLoaded()) {
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
    if (d.stageModel->isLoaded()) {
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
    if (d.stageModel->isLoaded()) {
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
ImagingGLWidgetPrivate::wheelEvent(QWheelEvent* event)
{
    double delta = static_cast<double>(event->angleDelta().y()) / 1000.0;
    double clamped = std::max(-0.5, std::min(0.5, delta));
    double factor = 1.0 - clamped;
    d.viewCamera.distance(factor);
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::focusEvent(QMouseEvent* event)
{
    d.glwidget->makeCurrent();
    if (!d.stageModel || !d.stageModel->isLoaded() || !d.glEngine)
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
                                            d.stageModel->stage()->GetPseudoRoot(), d.params, &hitPoint, &hitNormal,
                                            &hitPrimPath, &hitInstancerPath);

    if (hit) {
        d.viewCamera.setFocusPoint(hitPoint);
        d.glwidget->update();
    }
}

void
ImagingGLWidgetPrivate::sweepEvent(const QRect& rect, QMouseEvent* event)
{
    d.glwidget->makeCurrent();
    if (!d.stageModel || !d.stageModel->isLoaded() || !d.glEngine)
        return;

#ifdef WIN32
    glDepthMask(GL_TRUE);
#endif
    QPoint tl = deviceRatio(rect.topLeft());
    QPoint br = deviceRatio(rect.bottomRight());

    if (br.x() < tl.x())
        std::swap(tl.rx(), br.rx());
    if (br.y() < tl.y())
        std::swap(tl.ry(), br.ry());
    const QRect r(tl, br);

    GfVec4d viewport = widgetViewport();

    GfVec2d center(((r.left() + r.right()) * 0.5 - viewport[0]) / static_cast<double>(viewport[2]),
                   ((r.top() + r.bottom()) * 0.5 - viewport[1]) / static_cast<double>(viewport[3]));
    center[0] = center[0] * 2.0 - 1.0;
    center[1] = -1.0 * (center[1] * 2.0 - 1.0);

    GfVec2d size(static_cast<double>(r.width()) / viewport[2], static_cast<double>(r.height()) / viewport[3]);

    const bool click = (r.width() < 3 && r.height() < 3);

    UsdImagingGLEngine::PickParams pickParams;
    pickParams.resolveMode = click ? TfToken("resolveNearestToCenter") : TfToken("resolveDeep");

    GfCamera cam = d.viewCamera.camera();
    GfFrustum fr = cam.GetFrustum();
    GfFrustum pickFr = fr.ComputeNarrowedFrustum(center, size);

    UsdImagingGLEngine::IntersectionResultVector results;
    const bool hit = d.glEngine->TestIntersection(pickParams, pickFr.ComputeViewMatrix(),
                                                  pickFr.ComputeProjectionMatrix(),
                                                  d.stageModel->stage()->GetPseudoRoot(), d.params, &results);

    QList<SdfPath> selectedPaths;
    if (hit) {
        for (const auto& rItem : results)
            if (!rItem.hitPrimPath.IsEmpty())
                selectedPaths.append(rItem.hitPrimPath);
    }
    if (!selectedPaths.isEmpty()) {
        if (event->modifiers() & Qt::ShiftModifier) {
            d.selectionModel->togglePaths(selectedPaths);
        }
        else {
            d.selectionModel->replacePaths(selectedPaths);
        }
    }
    else {
        d.selectionModel->clear();
    }
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::selectionChanged(const QList<SdfPath>& paths)
{
    Q_ASSERT("gl engine is not set" && d.glEngine);
    d.glEngine->ClearSelected();
    for (SdfPath path : paths) {
        d.glEngine->AddSelected(path, UsdImagingDelegate::ALL_INSTANCES);
    }
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::maskChanged(const QList<SdfPath>& paths)
{
    d.mask = paths;
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::primsChanged(const QList<SdfPath>& paths)
{
    d.glwidget->update();
}

void
ImagingGLWidgetPrivate::stageChanged()
{
    d.mask.clear();
    d.glEngine.reset();
    if (d.stageModel->isLoaded()) {
        initCamera();
        initGL();
    }
    else {
        initGL();
        d.glwidget->update();
    }
}

double
ImagingGLWidgetPrivate::complexityRefinement(ImagingGLWidget::Complexity complexity)
{
    switch (complexity) {
    case ImagingGLWidget::Low: return 1.0;
    case ImagingGLWidget::Medium: return 1.1;
    case ImagingGLWidget::High: return 1.2;
    case ImagingGLWidget::VeryHigh: return 1.3;
    default: Q_ASSERT("complexity value not defined"); return 1.0;
    }
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

// todo: not yet in use
// #include "usdimagingglwidget.moc"

ImagingGLWidget::ImagingGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , p(new ImagingGLWidgetPrivate())
{
    p->d.glwidget = this;
    p->init();
}

ImagingGLWidget::~ImagingGLWidget() {}

ViewCamera
ImagingGLWidget::viewCamera() const
{
    return p->d.viewCamera;
}

QImage
ImagingGLWidget::image()
{
    return QOpenGLWidget::grabFramebuffer();
}

ImagingGLWidget::Complexity
ImagingGLWidget::complexity() const
{
    return p->d.complexity;
}

void
ImagingGLWidget::setComplexity(ImagingGLWidget::Complexity complexity)
{
    if (p->d.complexity != complexity) {
        p->d.complexity = complexity;
        update();
    }
}

ImagingGLWidget::DrawMode
ImagingGLWidget::drawMode() const
{
    return p->d.drawMode;
}

void
ImagingGLWidget::setDrawMode(DrawMode drawMode)
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
ImagingGLWidget::setDefaultCameraLightEnabled(bool enabled)
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
ImagingGLWidget::setSceneLightsEnabled(bool enabled)
{
    if (enabled != p->d.defaultCameraLightEnabled) {
        p->d.defaultCameraLightEnabled = enabled;
        update();
    }
}

bool
ImagingGLWidget::sceneMaterialsEnabled() const
{
    return p->d.sceneMaterialsEnabled;
}

void
ImagingGLWidget::setSceneMaterialsEnabled(bool enabled)
{
    if (enabled != p->d.sceneMaterialsEnabled) {
        p->d.sceneMaterialsEnabled = enabled;
        update();
    }
}

QList<QString>
ImagingGLWidget::rendererAovs() const
{
    Q_ASSERT("gl engine is not inititialized" && p->d.glEngine);
    return TfTokenVector_QList(p->d.glEngine->GetRendererAovs());
}

void
ImagingGLWidget::setRendererAov(const QString& aov)
{
    if (p->d.aov != aov) {
        p->d.aov = aov;
        update();
    }
}

StageModel*
ImagingGLWidget::stageModel() const
{
    return p->d.stageModel;
}

void
ImagingGLWidget::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
}

SelectionModel*
ImagingGLWidget::selectionModel()
{
    return p->d.selectionModel;
}

void
ImagingGLWidget::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
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
}  // namespace usd
