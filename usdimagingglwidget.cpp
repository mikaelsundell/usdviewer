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

class UsdImagingGLWidgetPrivate {
    public:
        enum CameraMode
        {
            Truck,
            Tumble,
            Zoom,
            Pick
        };
        void init();
        void init_gl();
        void paint_gl();
        void pre_glpass();
        void render_glpass();
        void post_glpass();
        void cleanup();
        struct ViewSettings {
            QString aov = "color";
            QColor clearcolor = QColor(0, 0, 0);
            float complexity = 1.0;
        };
        ViewSettings v;
        struct SelectionModel {
        };
        SelectionModel s;
        struct Data {
            size_t count;
            qint64 frame;
            UsdViewCamera viewcamera;
            UsdStageRefPtr stage;
            UsdImagingGLRenderParams params;
            QScopedPointer<UsdImagingGLEngine> glengine;
            QPointer<UsdImagingGLWidget> widget;
        };
        Data d;
};

void
UsdImagingGLWidgetPrivate::init()
{
    QSurfaceFormat format;
    format.setSamples(4);
    d.widget->setFormat(format);
}

void
UsdImagingGLWidgetPrivate::init_gl()
{
    qDebug() << "init_gl()";
    if (!d.glengine) {
        d.glengine.reset(new UsdImagingGLEngine());
        Hgi* hgi = d.glengine->GetHgi();
        if (hgi) {
            TfToken driver = hgi->GetAPIName();
            qDebug() << "gl engine initialized, using hydra driver: " << driver.GetText();
        }
        else {
            qWarning() << "could not initialize gl engine, no hydra driver found.";
            d.glengine.reset();
        }
    }
}

void
UsdImagingGLWidgetPrivate::paint_gl()
{
    qDebug() << "paint_gl()";
    pre_glpass();
    render_glpass();
    post_glpass();
}

void
UsdImagingGLWidgetPrivate::pre_glpass()
{
    qDebug() << "-pre_glpass()";
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glClearColor(v.clearcolor.redF(), v.clearcolor.greenF(), v.clearcolor.blueF(), 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void
UsdImagingGLWidgetPrivate::render_glpass()
{
    if (d.stage) {
        if (d.glengine) {
            qDebug() << "-render_glpass()";
            qDebug() << "-aov: " << v.aov;
            qDebug() << "-available aovs: " << d.glengine->GetRendererAovs();
            qDebug() << "-count: " << d.count;
            
            Q_ASSERT("aov is not set and is required" && v.aov.size());
            TfToken aovtoken(QString_TfToken(v.aov));
            d.glengine->SetRendererAov(aovtoken);
            
            // todo: stand-in code will be replaced
            
            GfFrustum frustum;
            float near = 1.0;
            float far = 2000000;
            int w = d.widget->width() * d.widget->devicePixelRatio();
            int h = d.widget->height() * d.widget->devicePixelRatio();
            GfVec4d viewport(0, 0, w, h);

            d.glengine->SetCameraState(
                GfMatrix4d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -100, 1),
                GfMatrix4d(4.0, 0, 0, 0, 0, 6.5, 0, 0, 0, 0, -1.0000010000005, -1, 0, 0, -2.0000010000005, 0)
            );
            
            d.glengine->SetRenderBufferSize(GfVec2i(w, h));
            d.glengine->SetFraming(CameraUtilFraming(
                GfRange2f(GfVec2f(0, 0), GfVec2f(w, h)),
                GfRect2i(GfVec2i(0, 0), GfVec2i(w, h))
            ));
            d.glengine->SetWindowPolicy(CameraUtilMatchVertically);
            d.glengine->SetRenderViewport(viewport);
            
            // todo: stand-in code will be replaced
            
            UsdImagingGLRenderParams params;
            params.clearColor = QColor_GfVec4f(v.clearcolor);
            params.complexity = v.complexity;
            params.cullStyle = pxr::UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED;
            params.drawMode = pxr::UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
            params.enableLighting = false;
            params.enableIdRender = false;
            params.enableSampleAlphaToCoverage = true;
            params.enableSceneMaterials = false;
            params.enableSceneLights = true;
            params.flipFrontFacing = true;
            params.forceRefresh = true;
            params.gammaCorrectColors = true;
            params.highlight = false;
            params.showGuides = false;
            params.showProxy = true;
            params.showRender = true;
            
            TfErrorMark mark;
            Hgi* hgi = d.glengine->GetHgi();
            hgi->StartFrame();
            d.glengine->Render(d.stage->GetPseudoRoot(), params);
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
    else {
        qWarning() << "stage not set, render pass will be skipped";
    }
}

void
UsdImagingGLWidgetPrivate::post_glpass()
{
    qDebug() << "-post_glpass()";
}

void
UsdImagingGLWidgetPrivate::cleanup()
{
    qDebug() << "-cleanup";
}

#include "usdimagingglwidget.moc"

UsdImagingGLWidget::UsdImagingGLWidget(QWidget* parent)
: QOpenGLWidget(parent)
, p(new UsdImagingGLWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

UsdImagingGLWidget::~UsdImagingGLWidget()
{
    p->cleanup();
}

float
UsdImagingGLWidget::complexity() const
{
    return p->v.complexity;
}

void
UsdImagingGLWidget::set_complexity(float complexity)
{
    if (!qFuzzyCompare(complexity, p->v.complexity)) {
        p->v.complexity = complexity;
        update();
    }
}

QColor
UsdImagingGLWidget::clearcolor() const
{
    return p->v.clearcolor;
}

void
UsdImagingGLWidget::set_clearcolor(const QColor& color)
{
    if (color != p->v.clearcolor) {
        p->v.clearcolor = color;
        update();
    }
}

QList<QString>
UsdImagingGLWidget::rendereraovs() const
{
    return TfTokenVector_QList(p->d.glengine->GetRendererAovs());
}

void
UsdImagingGLWidget::set_rendereraov(const QString& aov)
{
    if (p->v.aov != aov) {
        p->v.aov = aov;
        update();
    }
}

void
UsdImagingGLWidget::initializeGL()
{
    qDebug() << "initializeGL()";
    initializeOpenGLFunctions();
    p->init_gl();
}

void
UsdImagingGLWidget::paintGL()
{
    qDebug() << "paintGL()";
    p->paint_gl();
}

void
UsdImagingGLWidget::mousePressEvent(QMouseEvent* event)
{
}

void
UsdImagingGLWidget::mouseMoveEvent(QMouseEvent* event)
{
}

void
UsdImagingGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
}

void
UsdImagingGLWidget::wheelEvent(QWheelEvent* event)
{
}

bool
UsdImagingGLWidget::load_file(const QString& filename)
{
    p->d.stage = UsdStage::Open(filename.toStdString());
    if (p->d.stage) {
        return true;
    }
    else {
        return false;
    }
}
