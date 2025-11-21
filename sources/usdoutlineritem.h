// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <pxr/usd/usd/prim.h>
#include <QTreeWidgetItem>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerItemPrivate;
class OutlinerItem : public QTreeWidgetItem {
public:
    enum Column { Name = 0, Type = 1, Visible = 2 };
    OutlinerItem(QTreeWidget* parent, const UsdStageRefPtr& stage, const SdfPath& path);
    OutlinerItem(QTreeWidgetItem* parent, const UsdStageRefPtr& stage, const SdfPath& path);
    virtual ~OutlinerItem();
    QVariant data(int column, int role) const override;
    bool isVisible() const;

private:
    QScopedPointer<OutlinerItemPrivate> p;
};
}  // namespace usd
