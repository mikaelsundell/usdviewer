// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "datamodel.h"
#include "selectionmodel.h"
#include "viewcamera.h"
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

namespace usd {

class ImagingGLWidgetPrivate;

/**
 * @class ImagingGLWidget
 * @brief OpenGL-based viewport for rendering USD scenes.
 *
 * Provides a GPU accelerated rendering widget built on
 * QOpenGLWidget. The widget renders USD stages using the
 * USD ImagingGL renderer and supports interactive camera
 * navigation, draw modes, lighting options, and scene updates.
 *
 * The widget integrates with DataModel and SelectionModel
 * to reflect scene data and selection changes.
 */
class ImagingGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    /**
     * @brief Rendering complexity levels.
     */
    enum complexity_level { complexity_low, complexity_medium, complexity_high, complexity_veryhigh };

    /**
     * @brief Supported rendering draw modes.
     */
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
    /**
     * @brief Constructs the OpenGL imaging widget.
     *
     * @param parent Optional parent widget.
     */
    ImagingGLWidget(QWidget* parent = nullptr);

    /**
     * @brief Destroys the ImagingGLWidget instance.
     */
    virtual ~ImagingGLWidget();

    /** @name Camera */
    ///@{

    /**
     * @brief Returns the current view camera.
     */
    ViewCamera viewCamera() const;

    /**
     * @brief Frames the specified bounding box.
     *
     * @param bbox Bounding box to frame.
     */
    void frame(const GfBBox3d& bbox);

    /**
     * @brief Resets the camera view.
     */
    void resetView();

    ///@}

    /** @name Capture */
    ///@{

    /**
     * @brief Captures the current rendered image.
     *
     * @return Image of the current viewport.
     */
    QImage captureImage();

    ///@}

    /** @name Lifecycle */
    ///@{

    /**
     * @brief Clears the current rendering state.
     */
    void close();

    ///@}

    /** @name Rendering Options */
    ///@{

    /**
     * @brief Returns the current draw mode.
     */
    draw_mode drawMode() const;

    /**
     * @brief Sets the draw mode.
     *
     * @param drawMode Rendering mode.
     */
    void setDrawMode(draw_mode drawMode);

    /**
     * @brief Returns the viewport clear color.
     */
    QColor clearColor() const;

    /**
     * @brief Sets the viewport clear color.
     *
     * @param color Background color.
     */
    void setClearColor(const QColor& color);

    /**
     * @brief Returns whether the default camera light is enabled.
     */
    bool defaultCameraLightEnabled() const;

    /**
     * @brief Enables or disables the default camera light.
     *
     * @param enabled Light state.
     */
    void setDefaultCameraLightEnabled(bool enabled);

    /**
     * @brief Returns whether scene lights are enabled.
     */
    bool sceneLightsEnabled() const;

    /**
     * @brief Enables or disables lights defined in the USD scene.
     *
     * @param enabled Light state.
     */
    void setSceneLightsEnabled(bool enabled);

    /**
     * @brief Returns whether scene materials are enabled.
     */
    bool sceneMaterialsEnabled() const;

    /**
     * @brief Enables or disables USD materials during rendering.
     *
     * @param enabled Material state.
     */
    void setSceneMaterialsEnabled(bool enabled);

    /**
     * @brief Returns whether rendering statistics are displayed.
     */
    bool statisticsEnabled() const;

    /**
     * @brief Enables or disables rendering statistics.
     *
     * @param enabled Statistics display state.
     */
    void setStatisticsEnabled(bool enabled);

    ///@}

    /** @name Renderer Outputs */
    ///@{

    /**
     * @brief Returns available renderer AOVs.
     */
    QList<QString> rendererAovs() const;

    /**
     * @brief Sets the active renderer AOV.
     *
     * @param aov Name of the AOV to display.
     */
    void setRendererAov(const QString& aov);

    ///@}

    /** @name Scene Updates */
    ///@{

    /**
     * @brief Updates the stage used for rendering.
     *
     * @param stage USD stage to render.
     */
    void updateStage(UsdStageRefPtr stage);

    /**
     * @brief Updates the scene bounding box.
     *
     * @param bbox Scene bounding box.
     */
    void updateBoundingBox(const GfBBox3d& bbox);

    /**
     * @brief Updates the visibility mask for prims.
     *
     * @param paths Prim paths included in the mask.
     */
    void updateMask(const QList<SdfPath>& paths);

    /**
     * @brief Updates specific prims in the renderer.
     *
     * @param paths Prim paths to refresh.
     */
    void updatePrims(const QList<SdfPath>& paths);

    /**
     * @brief Updates the current selection.
     *
     * @param paths Selected prim paths.
     */
    void updateSelection(const QList<SdfPath>& paths);

    ///@}

Q_SIGNALS:

    /**
     * @brief Emitted when a frame has finished rendering.
     *
     * @param elapsed Rendering time in milliseconds.
     */
    void renderReady(qint64 elapsed);

protected:
    /** @name OpenGL Events */
    ///@{

    /**
     * @brief Initializes OpenGL resources.
     */
    void initializeGL() override;

    /**
     * @brief Performs OpenGL rendering.
     */
    void paintGL() override;

    /**
     * @brief Handles Qt paint events.
     */
    void paintEvent(QPaintEvent* event) override;

    ///@}

    /** @name Mouse Interaction */
    ///@{

    /**
     * @brief Handles mouse double-click events.
     */
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse press events.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse move events.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse release events.
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse wheel events.
     */
    void wheelEvent(QWheelEvent* event) override;

    ///@}

private:
    QScopedPointer<ImagingGLWidgetPrivate> p;
};

}  // namespace usd
