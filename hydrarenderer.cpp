// Copyright 2024-present Rapid Images AB
// https://gitlab.rapidimages.se/one-cx/pipeline/usdviewer

#include "hydrarenderer.h"
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hdSt/renderDelegate.h>
#include <pxr/imaging/glf/drawTarget.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <QObject>
#include <QPointer>

PXR_NAMESPACE_USING_DIRECTIVE

class HydraRendererPrivate : public QObject {
    Q_OBJECT
    public:
        HydraRendererPrivate();
        ~HydraRendererPrivate();
        void init();
        void render(int width, int height);
        struct Data {
            pxr::UsdStageRefPtr stage;
            pxr::SdfPathVector excludePaths;
            pxr::UsdImagingGLEngine renderer;
            pxr::UsdImagingGLRenderParams params;
            pxr::GfCamera camera;
            QPointer<HydraRenderer> window;
        };
        Data d;
};

HydraRendererPrivate::HydraRendererPrivate()
{
}

HydraRendererPrivate::~HydraRendererPrivate()
{
}

void
HydraRendererPrivate::init()
{
    d.params.frame = 1;
}

void
HydraRendererPrivate::render(int width, int height)
{
    GfFrustum frustum = d.camera.GetFrustum();
    GfMatrix4d viewmatrix = frustum.ComputeViewMatrix();
    GfMatrix4d projectionmatrix = frustum.ComputeProjectionMatrix();
    
    const GfVec4d viewport(0, 0, width, height);
    d.renderer.SetRenderViewport(viewport);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    float position[4] = { 0.0, 0.5, 2,0 };
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    
    
    UsdPrim rootPrim = d.stage->GetDefaultPrim();
    if (!rootPrim) {
        rootPrim = d.stage->GetPseudoRoot(); // Fallback if no default prim
    }

    d.renderer.SetCameraState(viewmatrix, projectionmatrix);
    d.renderer.Render(rootPrim, d.params);
    
    while (glGetError() != GL_NO_ERROR) {
           printf("GL ERROR");
       }
    
}

#include "hydrarenderer.moc"

HydraRenderer::HydraRenderer(QWidget* parent)
: QOpenGLWidget(parent)
//, p(new HydraRendererPrivate())
{
    p = new HydraRendererPrivate();
    p->d.window = this;
    p->init();
}

HydraRenderer::~HydraRenderer()
{
}

void
HydraRenderer::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
}

void HydraRenderer::paintGL() {
    if (!p->d.stage) {
        return;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    p->render(width(), height());
    update();
}

void
HydraRenderer::load_file(const QString& filename)
{
    p->d.stage = UsdStage::Open(filename.toStdString());
    if (!p->d.stage) {
        qDebug() << "failed to open USD file: " << filename;
        return;
    }
}
