// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "datamodel.h"
#include "selectionmodel.h"
#include <QTreeWidget>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class StageTreePrivate;
class StageTree : public QTreeWidget {
    Q_OBJECT
public:
    StageTree(QWidget* parent = nullptr);
    virtual ~StageTree();
    void close();
    void collapse();
    void expand();

    QString filter() const;
    void setFilter(const QString& filter);

    bool payloadEnabled() const;
    void setPayloadEnabled(bool enabled);

    void updateStage(UsdStageRefPtr stage);
    void updatePrims(const QList<SdfPath>& paths);
    void updateSelection(const QList<SdfPath>& paths);

Q_SIGNALS:
    void primSelectionChanged(const QList<SdfPath>& paths);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QScopedPointer<StageTreePrivate> p;
};
}  // namespace usd
