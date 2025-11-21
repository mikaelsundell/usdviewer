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
class PayloadWidgetPrivate;
class PayloadWidget : public QTreeWidget {
    Q_OBJECT
public:
    PayloadWidget(QWidget* parent = nullptr);
    virtual ~PayloadWidget();

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

private:
    QScopedPointer<PayloadWidgetPrivate> p;
};
}  // namespace usd
