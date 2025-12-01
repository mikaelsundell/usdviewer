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
class PropertyTreePrivate;
class PropertyTree : public QTreeWidget {
    Q_OBJECT
public:
    PropertyTree(QWidget* parent = nullptr);
    virtual ~PropertyTree();
    void close();

    void updateStage(UsdStageRefPtr stage);
    void updatePrims(const QList<SdfPath>& paths);
    void updateSelection(const QList<SdfPath>& paths);

private:
    QScopedPointer<PropertyTreePrivate> p;
};
}  // namespace usd
