// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdselectionmodel.h"
#include "usdstagemodel.h"
#include <pxr/usd/usd/stage.h>
#include <QTreeWidget>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class InspectorWidgetPrivate;
class InspectorWidget : public QTreeWidget {
    Q_OBJECT
public:
    InspectorWidget(QWidget* parent = nullptr);
    virtual ~InspectorWidget();

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

private:
    QScopedPointer<InspectorWidgetPrivate> p;
};
}  // namespace usd
