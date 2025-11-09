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
class OutlinerWidgetPrivate;
class OutlinerWidget : public QTreeWidget {
    Q_OBJECT
public:
    OutlinerWidget(QWidget* parent = nullptr);
    virtual ~OutlinerWidget();
    void collapse();
    void expand();

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

    QString filter() const;
    void setFilter(const QString& filter);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QScopedPointer<OutlinerWidgetPrivate> p;
};
}  // namespace usd
