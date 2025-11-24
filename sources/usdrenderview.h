// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdselectionmodel.h"
#include "usdstagemodel.h"
#include <QTreeWidget>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class RenderViewPrivate;
class RenderView : public QWidget {
    Q_OBJECT
public:
    enum render_mode {
        render_shaded,
        render_wireframe,
    };

public:
    RenderView(QWidget* parent = nullptr);
    virtual ~RenderView();
    QImage captureImage();
    void frameAll();
    void frameSelected();
    void resetView();

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& color);

    bool defaultCameraLightEnabled() const;
    void setDefaultCameraLightEnabled(bool enabled);

    bool sceneLightsEnabled() const;
    void setSceneLightsEnabled(bool enabled);

    bool sceneMaterialsEnabled() const;
    void setSceneMaterialsEnabled(bool enabled);

    bool statisticsEnabled() const;
    void setStatisticsEnabled(bool enabled);

    render_mode renderMode() const;
    void setDrawMode(render_mode renderMode);

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

Q_SIGNALS:
    void renderReady();

private:
    QScopedPointer<RenderViewPrivate> p;
};
}  // namespace usd
