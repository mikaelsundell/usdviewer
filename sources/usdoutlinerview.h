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
class OutlinerViewPrivate;
class OutlinerView : public QWidget {
    Q_OBJECT
public:
    OutlinerView(QWidget* parent = nullptr);
    virtual ~OutlinerView();

    DataModel* dataModel() const;
    void setDataModel(DataModel* dataModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

public Q_SLOTS:
    void collapse();
    void expand();

private:
    QScopedPointer<OutlinerViewPrivate> p;
};
}  // namespace usd
