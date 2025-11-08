// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdselection.h"
#include "usdstagemodel.h"
#include "usdviewcamera.h"
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

namespace usd {
class ImagingGLWidgetPrivate;
class ImagingGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    enum Complexity { Low, Medium, High, VeryHigh };
    enum DrawMode { Points, Wireframe, WireframeOnSurface, ShadedFlat, ShadedSmooth, GeomOnly, GeomFlat, GeomSmooth };

public:
    ImagingGLWidget(QWidget* parent = nullptr);
    virtual ~ImagingGLWidget();
    ViewCamera viewCamera() const;
    QImage image();

    ImagingGLWidget::Complexity complexity() const;
    void setComplexity(ImagingGLWidget::Complexity complexity);

    DrawMode drawMode() const;
    void setDrawMode(ImagingGLWidget::DrawMode drawMode);

    QColor clearColor() const;
    void setClearColor(const QColor& color);

    bool defaultCameraLightEnabled() const;
    void setDefaultCameraLightEnabled(bool enabled);

    bool sceneLightsEnabled() const;
    void setSceneLightsEnabled(bool enabled);

    bool sceneMaterialsEnabled() const;
    void setSceneMaterialsEnabled(bool enabled);

    QList<QString> rendererAovs() const;
    void setRendererAov(const QString& aov);

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    Selection* selection();
    void setSelection(Selection* selection);

Q_SIGNALS:
    void rendererReady();

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QScopedPointer<ImagingGLWidgetPrivate> p;
};
}  // namespace usd
