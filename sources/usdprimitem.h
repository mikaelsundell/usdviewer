// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidgetItem>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class PrimItemPrivate;
class PrimItem : public QTreeWidgetItem {
public:
    enum Column { Name = 0, Type = 1, Vis = 2 };
    PrimItem(QTreeWidget* parent, const UsdStageRefPtr& stage, const SdfPath& path);
    PrimItem(QTreeWidgetItem* parent, const UsdStageRefPtr& stage, const SdfPath& path);
    virtual ~PrimItem();
    QVariant data(int column, int role) const override;
    bool isVisible() const;

private:
    QScopedPointer<PrimItemPrivate> p;
};
}  // namespace usd
