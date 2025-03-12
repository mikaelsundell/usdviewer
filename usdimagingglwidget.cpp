// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdimagingglwidget.h"
#include "usdutils.h"
#include "usdviewcamera.h"
#include <QColor>
#include <QMouseEvent>
#include <QObject>
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
class ImagingGLWidgetPrivate {
    public:
        void init();
        void initGL();
        void initCamera();
        void initStage(const Stage& stage);
        void paintGL();
        void mousePressEvent(QMouseEvent* event);
        void mouseMoveEvent(QMouseEvent* event);
        void mouseReleaseEvent(QMouseEvent* event);
        void wheelEvent(QWheelEvent* event);
        void pickEvent(QMouseEvent* event);
        void updateSelection();
        double complexityRefinement(ImagingGLWidget::Complexity complexity);
        QPoint deviceRatio(QPoint value) const;
        double deviceRatio(double value) const;
        double widgetAspectRatio() const;
        GfVec2i widgetSize() const;
        GfVec4d widgetViewport() const;
        void cleanUp();
        struct Data {
                QString aov;
                QColor clearColor;
                size_t count;
                qint64 frame;
                bool drag;
                QPoint mousepos;
                Stage stage;
                ViewCamera viewCamera;
                GfBBox3d selectionBBox;
                ImagingGLWidget::Complexity complexity = ImagingGLWidget::Low;
                UsdImagingGLRenderParams params;
                QScopedPointer<UsdImagingGLEngine> glEngine;
                QPointer<Selection> selection;
                QPointer<ImagingGLWidget> widget;
        };
        Data d;
};

void
ImagingGLWidgetPrivate::init()
{
    QSurfaceFormat format;
    format.setSamples(4);
    d.widget->setFormat(format);
    d.aov = "color";
    d.count = 0;
    d.frame = 0;
    d.drag = false;
}

void
ImagingGLWidgetPrivate::initGL()
{
    if (!d.glEngine) {
        d.glEngine.reset(new UsdImagingGLEngine());
        Hgi* hgi = d.glEngine->GetHgi();
        if (hgi) {
            TfToken driver = hgi->GetAPIName();
            qDebug() << "gl engine initialized, using hydra driver: " << driver.GetText();
        }
        else {
            qWarning() << "could not initialize gl engine, no hydra driver found.";
            d.glEngine.reset();
        }
        d.widget->rendererReady();
    }
}

void
ImagingGLWidgetPrivate::initCamera()
{
    Q_ASSERT("stage is not set" && d.stage.isValid());
    d.viewCamera = ViewCamera();
    d.viewCamera.setBoundingBox(d.stage.boundingBox());
    TfToken upAxis = UsdGeomGetStageUpAxis(d.stage.stagePtr());
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
ImagingGLWidgetPrivate::initStage(const Stage& stage)
{
    d.stage = stage;
    initCamera();
    d.glEngine.reset();
    initGL();
}

void
ImagingGLWidgetPrivate::paintGL()
{
    glClearColor(d.clearColor.redF(), d.clearColor.greenF(), d.clearColor.blueF(),
                 d.clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (d.stage.isValid()) {
        if (d.glEngine) {
            Q_ASSERT("aov is not set and is required" && d.aov.size());
            TfToken aovtoken(QString_TfToken(d.aov));
            d.glEngine->SetRendererAov(aovtoken);
            GfVec4d viewport = widgetViewport();

            d.glEngine->SetRenderBufferSize(widgetSize());
            d.glEngine->SetFraming(CameraUtilFraming(GfRange2f(GfVec2i(), widgetSize()),
                                                     GfRect2i(GfVec2i(), widgetSize())));
            d.glEngine->SetWindowPolicy(CameraUtilMatchVertically);
            d.glEngine->SetRenderViewport(viewport);

#ifdef WIN32
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
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
            d.params.cullStyle = UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED;
            d.params.drawMode = UsdImagingGLDrawMode::
                DRAW_WIREFRAME_ON_SURFACE;  // todo: changed to DRAW_SHADED_SMOOTH;
            d.params.forceRefresh = true;
            d.params.enableLighting = false;
            d.params.enableSampleAlphaToCoverage = false;
            d.params.enableSceneMaterials = false;
            d.params.enableSceneLights = true;
            d.params.flipFrontFacing = true;
            d.params.gammaCorrectColors = false;
            d.params.highlight = true;
            d.params.showGuides = false;
            d.params.showProxy = true;
            d.params.showRender = true;

            TfErrorMark mark;
            Hgi* hgi = d.glEngine->GetHgi();
            hgi->StartFrame();
            d.glEngine->Render(d.stage.stagePtr()->GetPseudoRoot(), d.params);
            hgi->EndFrame();

            GLint viewportX[4];
            glGetIntegerv(GL_VIEWPORT, viewportX);
            if (!mark.IsClean()) {
                qWarning() << "gl engine errors occured during rendering";
            }
            d.count++;
        }
        else {
            qWarning() << "gl engine is not inititialized, render pass will be skipped";
        }
    }
    else {
        qWarning() << "stage not set, render pass will be skipped";
    }
}

void
ImagingGLWidgetPrivate::mousePressEvent(QMouseEvent* event)
{
    d.drag = true;
    if (event->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) {
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
        pickEvent(event);
    }
    d.mousepos = event->pos();
}

void
ImagingGLWidgetPrivate::mouseMoveEvent(QMouseEvent* event)
{
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
        d.widget->update();
    }
    d.mousepos = event->pos();
}

void
ImagingGLWidgetPrivate::mouseReleaseEvent(QMouseEvent* event)
{
    d.drag = false;
    d.viewCamera.setCameraMode(ViewCamera::None);
}

void
ImagingGLWidgetPrivate::wheelEvent(QWheelEvent* event)
{
    double delta = static_cast<double>(event->angleDelta().y()) / 1000.0;
    double clamped = std::max(-0.5, std::min(0.5, delta));
    double factor = 1.0 - clamped;
    d.viewCamera.distance(factor);
    d.widget->update();
}

void
ImagingGLWidgetPrivate::pickEvent(QMouseEvent* event)
{
    QPoint mousepos = deviceRatio(event->pos());
    GfVec4d viewport = widgetViewport();
    GfVec2d pos = GfVec2d((mousepos.x() - viewport[0]) / static_cast<double>(viewport[2]),
                          (mousepos.y() - viewport[1]) / static_cast<double>(viewport[3]));
    pos[0] = (pos[0] * 2.0 - 1.0);
    pos[1] = -1.0 * (pos[1] * 2.0 - 1.0);
    GfVec2d size(1.0 / static_cast<double>(viewport[2]), 1.0 / static_cast<double>(viewport[3]));
    GfCamera camera = d.viewCamera.camera();
    GfFrustum frustum = camera.GetFrustum();
    GfFrustum pickfrustum = frustum.ComputeNarrowedFrustum(pos, size);
    GfVec3d hitPoint, hitNormal;
    SdfPath hitPrimPath, hitInstancerPath;
    if (d.glEngine->TestIntersection(pickfrustum.ComputeViewMatrix(),
                                     pickfrustum.ComputeProjectionMatrix(),
                                     d.stage.stagePtr()->GetPseudoRoot(), d.params, &hitPoint,
                                     &hitNormal, &hitPrimPath, &hitInstancerPath)) {
        d.selection->replacePaths(QList<SdfPath>() << hitPrimPath);
    }
    else {
        d.selection->clear();
    }
}

void
ImagingGLWidgetPrivate::updateSelection()
{
    Q_ASSERT("gl engine is not set" && d.glEngine);
    d.glEngine->ClearSelected();
    for (SdfPath path : d.selection->paths()) {
        d.glEngine->AddSelected(path, UsdImagingDelegate::ALL_INSTANCES);
    }
    d.widget->update();
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
    return value * d.widget->devicePixelRatio();
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
    int w = deviceRatio(d.widget->width());
    int h = deviceRatio(d.widget->height());
    return GfVec2i(w, h);
}

GfVec4d
ImagingGLWidgetPrivate::widgetViewport() const
{
    GfVec2i size = widgetSize();
    return GfVec4d(0, 0, size[0], size[1]);
}

void
ImagingGLWidgetPrivate::cleanUp()
{}

// todo: not yet in use
// #include "usdimagingglwidget.moc"

ImagingGLWidget::ImagingGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , p(new ImagingGLWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

ImagingGLWidget::~ImagingGLWidget() { p->cleanUp(); }

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

Selection*
ImagingGLWidget::selection()
{
    return p->d.selection;
}

void
ImagingGLWidget::setSelection(Selection* selection)
{
    if (p->d.selection != selection) {
        p->d.selection = selection;
        update();
    }
}

Stage
ImagingGLWidget::stage() const
{
    Q_ASSERT("stage is not set" && p->d.stage.isValid());
    return p->d.stage;
}

bool
ImagingGLWidget::setStage(const Stage& stage)
{
    p->initStage(stage);
    update();
    return true;
}

void
ImagingGLWidget::updateSelection()
{
    p->updateSelection();
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
