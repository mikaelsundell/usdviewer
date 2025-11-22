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
class PayloadViewPrivate;
class PayloadView : public QWidget {
    Q_OBJECT
public:
    PayloadView(QWidget* parent = nullptr);
    virtual ~PayloadView();

    StageModel* stageModel() const;
    void setStageModel(StageModel* stageModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

private:
    QScopedPointer<PayloadViewPrivate> p;
};
}  // namespace usd
