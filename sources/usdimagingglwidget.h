// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdselectionmodel.h"
#include "usdstagemodel.h"
#include "usdviewcamera.h"
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

namespace usd {
class ImagingGLWidgetPrivate;
class ImagingGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    enum complexity_level { complexity_low, complexity_medium, complexity_high, complexity_veryhigh };
    enum draw_mode {
        draw_points,
        draw_wireframe,
        draw_wireframeonsurface,
        draw_shadedflat,
        draw_shadedsmooth,
        draw_geomonly,
        draw_geomflat,
        draw_geomsmooth
    };

public:
    ImagingGLWidget(QWidget* parent = nullptr);
    virtual ~ImagingGLWidget();
    ViewCamera viewCamera() const;
    QImage captureImage();
    void close();

    draw_mode drawMode() const;
    void setDrawMode(draw_mode drawMode);

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

    void updateStage(UsdStageRefPtr stage);
    void updateBoundingBox(const GfBBox3d& bbox);
    void updateMask(const QList<SdfPath>& paths);
    void updatePrims(const QList<SdfPath>& paths);
    void updateSelection(const QList<SdfPath>& paths);

Q_SIGNALS:
    void renderReady();

protected:
    void initializeGL() override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QScopedPointer<ImagingGLWidgetPrivate> p;
};
}  // namespace usd
