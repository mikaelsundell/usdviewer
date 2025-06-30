// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdselection.h"
#include "usdstage.h"
#include <QTreeWidget>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class InspectorWidgetPrivate;
class InspectorWidget : public QTreeWidget {
    Q_OBJECT
public:
    InspectorWidget(QWidget* parent = nullptr);
    virtual ~InspectorWidget();

    Selection* selection();
    void setSelection(Selection* selection);
    
    Stage stage() const;
    bool setStage(const Stage& stage);

public Q_SLOTS:
    void updateSelection();

private:
    QScopedPointer<InspectorWidgetPrivate> p;
};
}  // namespace usd
