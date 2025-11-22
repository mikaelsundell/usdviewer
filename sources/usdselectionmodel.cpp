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
SelectionModel::addPaths(const QList<SdfPath>& paths)
{
    bool changed = false;
    for (const SdfPath& path : paths) {
        if (!p->d.paths.contains(path)) {
            p->d.paths.append(path);
            changed = true;
        }
    }
    if (changed)
        Q_EMIT selectionChanged(p->d.paths);
}

void
SelectionModel::removePaths(const QList<SdfPath>& paths)
{
    bool changed = false;
    for (const SdfPath& path : paths) {
        if (p->d.paths.removeOne(path))
            changed = true;
    }
    if (changed)
        Q_EMIT selectionChanged(p->d.paths);
}

void
SelectionModel::togglePaths(const QList<SdfPath>& paths)
{
    bool changed = false;
    for (const SdfPath& path : paths) {
        if (p->d.paths.contains(path)) {
            p->d.paths.removeOne(path);
            changed = true;
        }
        else {
            p->d.paths.append(path);
            changed = true;
        }
    }
    if (changed)
        Q_EMIT selectionChanged(p->d.paths);
}

void
SelectionModel::updatePaths(const QList<SdfPath>& paths)
{
    if (p->d.paths == paths)
        return;
    p->d.paths = paths;
    Q_EMIT selectionChanged(p->d.paths);
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
    }
    Q_EMIT selectionChanged(p->d.paths);
}

SelectionModel::~SelectionModel() {}

bool
SelectionModel::isEmpty() const
{
    return p->d.paths.size();
}

bool
SelectionModel::isValid() const
{
    return true;
}
}  // namespace usd
