// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdimagingglwidget.h"
#include "usdviewcamera.h"
#include "usdutils.h"

#include <pxr/base/tf/error.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/imaging/glf/diagnostic.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usd/primRange.h>

#include <QColor>
#include <QObject>
#include <QPointer>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class ImagingGLWidgetPrivate {
public:
    void init();
    void initGL();
    void paintGL();
    void preGLPass();
    void renderGLPass();
    void postGLPass();
    void initCamera();
    void initStage(const Stage& stage);
    double deviceRatio(int value) const;
    GfVec2i widgetSize() const;
    GfVec4d widgetViewport() const;
    void cleanUp();
    struct ViewSettings {
        QString aov = "color";
        QColor clearColor = QColor::fromRgbF(0.18, 0.18, 0.18);
        float complexity = 1.0;
    };
    ViewSettings v;
    struct SelectionModel {
    };
    SelectionModel s;
    struct Data {
        size_t count = 0;
        qint64 frame = 0;
        bool drag = false;
        QPoint mousepos;
        Stage stage;
        ViewCamera viewCamera;
        GfBBox3d selectionBBox;
        UsdImagingGLRenderParams params;
        QScopedPointer<UsdImagingGLEngine> glEngine;
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
ImagingGLWidgetPrivate::paintGL()
{
    preGLPass();
    renderGLPass();
    postGLPass();
}

void
ImagingGLWidgetPrivate::preGLPass()
{
}

void
ImagingGLWidgetPrivate::renderGLPass()
{
    if (d.stage.isValid()) {
        if (d.glEngine) {
            Q_ASSERT("aov is not set and is required" && v.aov.size());
            TfToken aovtoken(QString_TfToken(v.aov));
            d.glEngine->SetRendererAov(aovtoken);
            GfVec4d viewport = widgetViewport();
            d.glEngine->SetRenderBufferSize(widgetSize());
            d.glEngine->SetFraming(CameraUtilFraming(GfRange2f(GfVec2i(), widgetSize()), GfRect2i(GfVec2i(), widgetSize())));
            d.glEngine->SetWindowPolicy(CameraUtilMatchVertically);
            d.glEngine->SetRenderViewport(viewport);
            
            GfCamera camera = d.viewCamera.camera();
            GfFrustum frustum = camera.GetFrustum();
            GfMatrix4d viewModel = frustum.ComputeViewMatrix();
            GfMatrix4d projectionMatrix = frustum.ComputeProjectionMatrix();
            d.glEngine->SetCameraState(viewModel, projectionMatrix);
            
            UsdImagingGLRenderParams params;
            params.clearColor = QColor_GfVec4f(v.clearColor);
            params.complexity = v.complexity;
            params.cullStyle = UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED;
            params.drawMode = UsdImagingGLDrawMode::DRAW_WIREFRAME_ON_SURFACE; // todo: changed to DRAW_SHADED_SMOOTH;
            params.forceRefresh = true;
            params.enableLighting = false;
            params.enableIdRender = false;
            params.enableSampleAlphaToCoverage = false;
            params.enableSceneMaterials = false;
            params.enableSceneLights = true;
            params.flipFrontFacing = true;
            params.gammaCorrectColors = false;
            params.highlight = false;
            params.showGuides = false;
            params.showProxy = true;
            params.showRender = true;
            
            TfErrorMark mark;
            Hgi* hgi = d.glEngine->GetHgi();
            hgi->StartFrame();
            d.glEngine->Render(d.stage.stagePtr()->GetPseudoRoot(), params);
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
ImagingGLWidgetPrivate::postGLPass()
{
}

void
ImagingGLWidgetPrivate::initCamera()
{
    Q_ASSERT("stage is not set" && d.stage.isValid());
    d.viewCamera.setBoundingBox(d.stage.boundingBox());
    d.viewCamera.frameAll();
}

void
ImagingGLWidgetPrivate::initStage(const Stage& stage)
{
    d.stage = stage;
    initCamera();
}

qreal
ImagingGLWidgetPrivate::deviceRatio(int value) const
{
    return value * d.widget->devicePixelRatio();
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
{
}

#include "usdimagingglwidget.moc"

ImagingGLWidget::ImagingGLWidget(QWidget* parent)
: QOpenGLWidget(parent)
, p(new ImagingGLWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

ImagingGLWidget::~ImagingGLWidget()
{
    p->cleanUp();
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
}

float
ImagingGLWidget::complexity() const
{
    return p->v.complexity;
}

void
ImagingGLWidget::setComplexity(float complexity)
{
    if (!qFuzzyCompare(complexity, p->v.complexity)) {
        p->v.complexity = complexity;
        update();
    }
}

QColor
ImagingGLWidget::clearColor() const
{
    return p->v.clearColor;
}

void
ImagingGLWidget::setClearColor(const QColor& color)
{
    if (color != p->v.clearColor) {
        p->v.clearColor = color;
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
    if (p->v.aov != aov) {
        p->v.aov = aov;
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
ImagingGLWidget::mousePressEvent(QMouseEvent* event)
{
    p->d.drag = true;
    if (event->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) {
        if (event->button() == Qt::LeftButton) {
            if (event->modifiers() & Qt::ControlModifier) {
                p->d.viewCamera.setCameraMode(ViewCamera::Truck);
            }
            else {
                p->d.viewCamera.setCameraMode(ViewCamera::Tumble);
            }
        }
        else if (event->button() == Qt::MiddleButton) {
            p->d.viewCamera.setCameraMode(ViewCamera::Tumble);
        }
        else if (event->button() == Qt::RightButton) {
            p->d.viewCamera.setCameraMode(ViewCamera::Zoom);
        }
    }
    else {
        p->d.viewCamera.setCameraMode(ViewCamera::Pick);
        // todo: pick object
        // todo: pickObject(mousepos);

    }
    p->d.mousepos = event->pos();
}

void
ImagingGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();
    if (p->d.drag) {
        QPoint delta = p->d.mousepos - pos;
        p->d.viewCamera.tumble(0.25 * p->deviceRatio(delta.x()), 0.25 * p->deviceRatio(delta.y()));
        update();
    }
}

void
ImagingGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    p->d.drag = false;
    p->d.viewCamera.setCameraMode(ViewCamera::None);
}

void
ImagingGLWidget::wheelEvent(QWheelEvent* event)
{
    QOpenGLWidget::wheelEvent(event);
}
}
