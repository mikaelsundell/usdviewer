// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdselectionmodel.h"
#include "usdutils.h"
#include <QList>

namespace usd {
class SelectionModelPrivate {
public:
    SelectionModelPrivate();
    ~SelectionModelPrivate();
    struct Data {
        QList<SdfPath> paths;
    };
    Data d;
};

SelectionModelPrivate::SelectionModelPrivate() {}

SelectionModelPrivate::~SelectionModelPrivate() {}

SelectionModel::SelectionModel(QObject* parent)
    : QObject(parent)
    , p(new SelectionModelPrivate())
{}

bool
SelectionModel::isSelected(const SdfPath& path) const
{
    return p->d.paths.contains(path);
}

void
SelectionModel::addPath(const SdfPath& path)
{
    if (!p->d.paths.contains(path)) {
        p->d.paths.append(path);
        selectionChanged();
    }
}

void
SelectionModel::replacePaths(const QList<SdfPath>& paths)
{
    p->d.paths = paths;
    selectionChanged();
}

void
SelectionModel::removePath(const SdfPath& path)
{
    Q_ASSERT("item is not selected" && isSelected(path));
    p->d.paths.removeOne(path);
    selectionChanged();
}

QList<SdfPath>
SelectionModel::paths() const
{
    return p->d.paths;
}

void
SelectionModel::clear()
{
    if (p->d.paths.size()) {
        p->d.paths.clear();
        selectionChanged();
    }
}

SelectionModel::~SelectionModel() {}

bool
SelectionModel::isValid() const
{
    return true;
}
}  // namespace usd
