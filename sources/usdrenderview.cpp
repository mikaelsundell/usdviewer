// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdrenderview.h"
#include <QPointer>

// generated files
#include "ui_usdrenderview.h"

namespace usd {
class RenderViewPrivate : public QObject {
public:
    void init();
    void initStageModel();
    void initSelection();
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
    void stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status);

public:
    struct Data {
        UsdStageRefPtr stage;
        QScopedPointer<Ui_UsdRenderView> ui;
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<RenderView> view;
    };
    Data d;
};

void
RenderViewPrivate::init()
{
    d.ui.reset(new Ui_UsdRenderView());
    d.ui->setupUi(d.view.data());
    connect(imageGLWidget(), &ImagingGLWidget::renderReady, d.view.data(), &RenderView::renderReady);
}

void
RenderViewPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &RenderViewPrivate::stageChanged);
    connect(d.stageModel.data(), &StageModel::maskChanged, this, &RenderViewPrivate::maskChanged);
    connect(d.stageModel.data(), &StageModel::primsChanged, this, &RenderViewPrivate::primsChanged);
}

void
RenderViewPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &RenderViewPrivate::selectionChanged);
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
    if (d.stageModel->isLoaded()) {
        camera().setBoundingBox(d.stageModel->boundingBox());
        camera().frameAll();
        imageGLWidget()->update();
    }
}

void
RenderViewPrivate::frameSelected()
{
    if (d.selectionModel->paths().size()) {
        camera().setBoundingBox(d.stageModel->boundingBox(d.selectionModel->paths()));
        camera().frameAll();
        imageGLWidget()->update();
    }
}

void
RenderViewPrivate::resetView()
{
    camera().resetView();
    imageGLWidget()->update();
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
RenderViewPrivate::stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status)
{
    if (status == StageModel::stage_loaded) {
        imageGLWidget()->updateStage(stage);
    }
    else {
        imageGLWidget()->close();
    }
}

RenderView::RenderView(QWidget* parent)
    : QWidget(parent)
    , p(new RenderViewPrivate())
{
    p->d.view = this;
    p->init();
}

RenderView::~RenderView() {}

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
    p->imageGLWidget()->setDefaultCameraLightEnabled(enabled);
}

bool
RenderView::sceneLightsEnabled() const
{
    return p->imageGLWidget()->sceneLightsEnabled();
}

void
RenderView::setSceneLightsEnabled(bool enabled)
{
    p->imageGLWidget()->setSceneLightsEnabled(enabled);
}

bool
RenderView::sceneMaterialsEnabled() const
{
    return p->imageGLWidget()->sceneMaterialsEnabled();
}

void
RenderView::setSceneMaterialsEnabled(bool enabled)
{
    p->imageGLWidget()->setSceneMaterialsEnabled(enabled);
}

bool
RenderView::statisticsEnabled() const
{
    return p->imageGLWidget()->statisticsEnabled();
}

void
RenderView::setStatisticsEnabled(bool enabled)
{
    p->imageGLWidget()->setStatisticsEnabled(enabled);
}

StageModel*
RenderView::stageModel() const
{
    return p->d.stageModel;
}

void
RenderView::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
}

SelectionModel*
RenderView::selectionModel()
{
    return p->d.selectionModel;
}

void
RenderView::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
}
}  // namespace usd
