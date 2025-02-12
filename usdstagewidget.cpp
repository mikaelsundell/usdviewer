// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstagewidget.h"
#include "usdviewcamera.h"
#include "usddebug.h"
#include "gldebugger.h"
#include <pxr/base/tf/error.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/imaging/glf/diagnostic.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <QObject>
#include <QPointer>

PXR_NAMESPACE_USING_DIRECTIVE

class UsdStageWidgetPrivate {
    public:
        enum CameraMode
        {
            TRUCK, TUMBLE, ZOOM, PICK
        };
        void init();
        void init_gl();
        void pre_glpass();
        void render_glpass();
        void post_glpass();
        void update();
        GfVec4d get_viewport(double aspectratio);
        GfVec4d get_integralcentered(const GfVec4d& viewport);
        CameraUtilFraming get_framing(const GfVec4d& viewport, const GfVec2i& devicesize);
        GfVec2i get_surfacesize();
        struct ViewSettings {
            
        };
        ViewSettings v;
        struct SelectionModel {
            
        };
        SelectionModel s;
        struct Data {
            size_t pass;
            qint64 frame;
            UsdViewCamera viewcamera;
            UsdStageRefPtr stage;
            UsdImagingGLRenderParams params;
            QScopedPointer<UsdImagingGLEngine> glengine;
            QScopedPointer<GLDebugger> gldebugger;
            QPointer<UsdStageWidget> widget;
        };
        Data d;
};

void
UsdStageWidgetPrivate::init()
{
    QSurfaceFormat format;
    const char* msaaEnv = std::getenv("USDVIEW_ENABLE_MSAA");
    if (!msaaEnv || std::string(msaaEnv) == "1") {
        format.setSamples(4);
        qDebug() << "MSAA Enabled with 4x Samples";
    } else {
        qDebug() << "MSAA Disabled";
    }
    d.widget->setFormat(format);
    update();
}

void
UsdStageWidgetPrivate::init_gl()
{
    Q_ASSERT("can not request renderer without a GL context" && QOpenGLContext::currentContext()->isValid());
    if (!d.glengine) {
        UsdImagingGLEngine::Parameters params;
        
        // todo: take care of these
        //params.allowAsynchronousSceneProcessing = _allowAsync;
        //params.displayUnloadedPrimsWithBounds = _bboxStandin;
        
        d.glengine.reset(new UsdImagingGLEngine(params));
    }
    GlfRegisterDefaultDebugOutputMessageCallback();
}

void
UsdStageWidgetPrivate::pre_glpass()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    
    glClearColor(0.5, 0.5, 0.5, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    GfCamera camera = d.viewcamera.camera();
    GfVec4d viewport = get_viewport(camera.GetAspectRatio());
    //d.glengine->SetRenderViewport(viewport);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    qDebug() << "-viewport: " << viewport;
    
    GfVec2i devicesize = get_surfacesize();
    d.glengine->SetRenderBufferSize(devicesize);
    qDebug() << "-device size: " << devicesize;

    CameraUtilFraming framing = get_framing(viewport, devicesize);
    d.glengine->SetFraming(framing);
    qDebug() << "-framing: " << framing;
    
    GfFrustum frustum = camera.GetFrustum();
    d.glengine->SetCameraState(frustum.ComputeViewMatrix(), frustum.ComputeProjectionMatrix());
    qDebug() << "-set camera state: " << frustum.ComputeViewMatrix() << ", " << frustum.ComputeProjectionMatrix();
}

void
UsdStageWidgetPrivate::render_glpass()
{
    try {
        qDebug() << "-render pseudoroot";
        qDebug() << "-params: " <<  d.params;
        d.glengine->Render(d.stage->GetPseudoRoot(), d.params);
        CHECK_GL_ERROR();
    }
    catch (const std::exception& e) {
        Q_ASSERT("render pass failed with exception" && 0);
    }
}

void
UsdStageWidgetPrivate::post_glpass()
{
}

void
UsdStageWidgetPrivate::update()
{
}

GfVec4d
UsdStageWidgetPrivate::get_viewport(double aspectratio)
{
    CameraUtilConformWindowPolicy windowpolicy = CameraUtilMatchVertically;
    double targetaspect = static_cast<double>(d.widget->size().width()) / std::max(1.0, static_cast<double>(d.widget->size().height()));
    if (targetaspect < aspectratio) {
        windowpolicy = CameraUtilMatchHorizontally;
    }
    GfRange2d viewport(GfVec2d(0, 0), get_surfacesize());
    viewport = CameraUtilConformedWindow(viewport, windowpolicy, aspectratio);
    GfVec4d finalviewport(
        viewport.GetMin()[0], viewport.GetMin()[1],
        viewport.GetSize()[0], viewport.GetSize()[1]
    );
    return get_integralcentered(finalviewport);
}

GfVec4d
UsdStageWidgetPrivate::get_integralcentered(const GfVec4d& viewport)
{
    double left = std::floor(viewport[0]);
    double bottom = std::floor(viewport[1]);
    double right = std::ceil(viewport[0] + viewport[2]);
    double top = std::ceil(viewport[1] + viewport[3]);
    double width = right - left;
    double height = top - bottom;
    if ((height - viewport[3]) > 1.0) {
        bottom += 1;
        height -= 2;
    }
    if ((width - viewport[2]) > 1.0) {
        left += 1;
        width -= 2;
    }
    return GfVec4d(left, bottom, width, height);
}

CameraUtilFraming
UsdStageWidgetPrivate::get_framing(const GfVec4d& viewport, const GfVec2i& surfacesize)
{
    double x = viewport[0];
    double y = viewport[1];
    double w = viewport[0];
    double h = viewport[1];
    int surfacewidth = surfacesize[0];
    int surfaceheight = surfacesize[1];
    GfRange2f displaywindow(
        GfVec2f(x, surfaceheight - y - h),
        GfVec2f(x + w, surfaceheight - y)
    );
    GfRect2i surfacerect(GfVec2i(0, 0), surfacewidth, surfaceheight);
    GfRect2i datawindow = surfacerect.GetIntersection(
        GfRect2i(GfVec2i(x, surfaceheight - y - h), w, h)
    );
    return CameraUtilFraming(displaywindow, datawindow);
}

GfVec2i
UsdStageWidgetPrivate::get_surfacesize()
{
    int w = d.widget->width() * d.widget->devicePixelRatioF();
    int h = d.widget->height() * d.widget->devicePixelRatioF();
    return GfVec2i(w, h);
}

#include "usdstagewidget.moc"

UsdStageWidget::UsdStageWidget(QWidget* parent)
: QOpenGLWidget(parent)
, p(new UsdStageWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

UsdStageWidget::~UsdStageWidget()
{
}

void
UsdStageWidget::initializeGL()
{
    initializeOpenGLFunctions();
    qDebug() << "OpenGL Initialized: version" << (const char*)glGetString(GL_VERSION);
    if (!p->d.gldebugger) {
        Q_ASSERT("OpenGL context is not valid" && QOpenGLContext::currentContext()->isValid());
        p->d.gldebugger.reset(new GLDebugger(QOpenGLContext::currentContext()));
    }
    if (QOpenGLContext::currentContext()->hasExtension("GL_KHR_debug")) { // todo: temporary for now
        qDebug() << "GL_KHR_debug is supported, gl debugging supported";
    } else {
        qWarning() << "GL_KHR_debug is NOT supported, gl debugging not supported";
    }
    
}

void
UsdStageWidget::paintGL()
{
    qDebug() << "paintGL: begin";
    qDebug() << "-pass: " << p->d.pass;
    p->d.pass++;
    
    p->init_gl();
    p->pre_glpass();
    p->render_glpass();
    p->post_glpass();
    
    qDebug() << "paintGL: end";
}

void
UsdStageWidget::mousePressEvent(QMouseEvent* event)
{
}

void
UsdStageWidget::mouseMoveEvent(QMouseEvent* event)
{
}

void
UsdStageWidget::mouseReleaseEvent(QMouseEvent* event)
{
}

void
UsdStageWidget::wheelEvent(QWheelEvent* event)
{
}

bool
UsdStageWidget::load_file(const QString& filename)
{
    p->d.stage = UsdStage::Open(filename.toStdString());
    if (p->d.stage) {
        return true;
    }
    else {
        return false;
    }
}
