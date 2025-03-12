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
class OutlinerWidgetPrivate;
class OutlinerWidget : public QTreeWidget {
        Q_OBJECT
    public:
        OutlinerWidget(QWidget* parent = nullptr);
        virtual ~OutlinerWidget();

        Selection* selection();
        void setSelection(Selection* selection);

        Stage stage() const;
        bool setStage(const Stage& stage);

    public Q_SLOTS:
        void updateSelection();

    protected:
        void keyPressEvent(QKeyEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:
        QScopedPointer<OutlinerWidgetPrivate> p;
};
}  // namespace usd
