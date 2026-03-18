// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "renderview.h"
#include "application.h"
#include "stageutils.h"
#include <QPointer>

// generated files
#include "ui_renderview.h"

namespace usdviewer {
class RenderViewPrivate : public QObject {
public:
    void init();
    ImagingGLWidget* imageGLWidget();
    ViewCamera camera();
    void frameAll();
    void frameSelected();
    void resetView();

public Q_SLOTS:
    void boundingBoxChanged(const GfBBox3d& bbox);
    void maskChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status);
    void renderReady(qint64 elapsed);

public:
    struct Data {
        QScopedPointer<Ui_RenderView> ui;
        QPointer<RenderView> view;
    };
    Data d;
};

void
RenderViewPrivate::init()
{
    d.ui.reset(new Ui_RenderView());
    d.ui->setupUi(d.view.data());
    // connect
    connect(imageGLWidget(), &ImagingGLWidget::renderReady, this, &RenderViewPrivate::renderReady);
    connect(dataModel(), &DataModel::boundingBoxChanged, this, &RenderViewPrivate::boundingBoxChanged);
    connect(dataModel(), &DataModel::maskChanged, this, &RenderViewPrivate::maskChanged);
    connect(dataModel(), &DataModel::primsChanged, this, &RenderViewPrivate::primsChanged);
    connect(dataModel(), &DataModel::stageChanged, this, &RenderViewPrivate::stageChanged);
    connect(selectionModel(), &SelectionModel::selectionChanged, this, &RenderViewPrivate::selectionChanged);
}

ImagingGLWidget*
RenderViewPrivate::imageGLWidget()
{
    return d.ui->imagingGLWidget;
}

ViewCamera
RenderViewPrivate::camera()
{
    return imageGLWidget()->viewCamera();
}

void
RenderViewPrivate::frameAll()
{
    if (dataModel()->isLoaded()) {
        imageGLWidget()->frame(dataModel()->boundingBox());
    }
}

void
RenderViewPrivate::frameSelected()
{
    if (selectionModel()->paths().size()) {
        imageGLWidget()->frame(stage::boundingBox(dataModel()->stage(), selectionModel()->paths()));
    }
}

void
RenderViewPrivate::resetView()
{
    if (dataModel()->isLoaded()) {
        imageGLWidget()->resetView();
    }
}

void
RenderViewPrivate::boundingBoxChanged(const GfBBox3d& bbox)
{
    imageGLWidget()->updateBoundingBox(bbox);
}

void
RenderViewPrivate::maskChanged(const QList<SdfPath>& paths)
{
    imageGLWidget()->updateMask(paths);
}

void
RenderViewPrivate::primsChanged(const QList<SdfPath>& paths)
{
    imageGLWidget()->updatePrims(paths);
}

void
RenderViewPrivate::selectionChanged(const QList<SdfPath>& paths)
{
    imageGLWidget()->updateSelection(paths);
}

void
RenderViewPrivate::stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status)
{
    if (status == DataModel::StageLoaded) {
        imageGLWidget()->updateStage(dataModel()->stage());
    }
    else {
        imageGLWidget()->close();
    }
}

void
RenderViewPrivate::renderReady(qint64 elapsed)
{
    const qint64 thresholdMs = 500;
    if (elapsed > thresholdMs) {
        if (dataModel()) {
            QString msg = QStringLiteral("Warning: Render time %1 ms").arg(elapsed);
            dataModel()->setStatus(msg);
        }
    }
}

RenderView::RenderView(QWidget* parent)
    : QWidget(parent)
    , p(new RenderViewPrivate())
{
    p->d.view = this;
    p->init();
}

RenderView::~RenderView() = default;

QImage
RenderView::captureImage()
{
    return p->imageGLWidget()->captureImage();
}

void
RenderView::frameAll()
{
    p->frameAll();
}

void
RenderView::frameSelected()
{
    p->frameSelected();
}

void
RenderView::resetView()
{
    p->resetView();
}

QColor
RenderView::backgroundColor() const
{
    return p->imageGLWidget()->clearColor();
}

void
RenderView::setBackgroundColor(const QColor& color)
{
    p->imageGLWidget()->setClearColor(color);
}

RenderView::render_mode
RenderView::renderMode() const
{
    ImagingGLWidget::draw_mode drawMode = p->imageGLWidget()->drawMode();
    switch (drawMode) {
    case ImagingGLWidget::draw_wireframe:
    case ImagingGLWidget::draw_wireframeonsurface: return render_wireframe;

    default: return render_shaded;
    }
}

void
RenderView::setDrawMode(RenderView::render_mode renderMode)
{
    switch (renderMode) {
    case render_shaded: p->imageGLWidget()->setDrawMode(ImagingGLWidget::draw_shadedsmooth); break;

    case render_wireframe: p->imageGLWidget()->setDrawMode(ImagingGLWidget::draw_wireframeonsurface); break;
    }
    p->imageGLWidget()->update();
}

bool
RenderView::defaultCameraLightEnabled() const
{
    return p->imageGLWidget()->defaultCameraLightEnabled();
}
void
RenderView::setDefaultCameraLightEnabled(bool enabled)
{
    p->imageGLWidget()->enableDefaultCameraLight(enabled);
}

bool
RenderView::sceneLightsEnabled() const
{
    return p->imageGLWidget()->sceneLightsEnabled();
}

void
RenderView::setSceneLightsEnabled(bool enabled)
{
    p->imageGLWidget()->enableSceneLights(enabled);
}

bool
RenderView::sceneMaterialsEnabled() const
{
    return p->imageGLWidget()->sceneShadersEnabled();
}

void
RenderView::setSceneMaterialsEnabled(bool enabled)
{
    p->imageGLWidget()->enableSceneShaders(enabled);
}

bool
RenderView::sceneTreeEnabled() const
{
    return p->imageGLWidget()->sceneTreeEnabled();
}

void
RenderView::enableSceneTree(bool enabled)
{
    p->imageGLWidget()->enableSceneTree(enabled);
}

bool
RenderView::gpuPerformanceEnabled() const
{
    return p->imageGLWidget()->gpuPerformanceEnabled();
}

void
RenderView::enableGpuPerformance(bool enabled)
{
    p->imageGLWidget()->enableGpuPerformance(enabled);
}

bool
RenderView::cameraAxisEnabled() const
{
    return p->imageGLWidget()->cameraAxisEnabled();
}

void
RenderView::enableCameraAxis(bool enabled)
{
    p->imageGLWidget()->enableCameraAxis(enabled);
}
}  // namespace usdviewer
