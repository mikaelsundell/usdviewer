// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdselection.h"
#include "usdutils.h"
#include <QList>

namespace usd {
class SelectionPrivate {
public:
    SelectionPrivate();
    ~SelectionPrivate();
    struct Data {
        QList<SdfPath> paths;
    };
    Data d;
};

SelectionPrivate::SelectionPrivate() {}

SelectionPrivate::~SelectionPrivate() {}

Selection::Selection(QObject* parent)
    : QObject(parent)
    , p(new SelectionPrivate())
{}

bool
Selection::isSelected(const SdfPath& path) const
{
    return p->d.paths.contains(path);
}

void
Selection::addPath(const SdfPath& path)
{
    if (!p->d.paths.contains(path)) {
        p->d.paths.append(path);
        selectionChanged();
    }
}

void
Selection::replacePaths(const QList<SdfPath>& paths)
{
    p->d.paths = paths;
    selectionChanged();
}

void
Selection::removePath(const SdfPath& path)
{
    Q_ASSERT("item is not selected" && isSelected(path));
    p->d.paths.removeOne(path);
    selectionChanged();
}

QList<SdfPath>
Selection::paths() const
{
    return p->d.paths;
}

void
Selection::clear()
{
    if (p->d.paths.size()) {
        p->d.paths.clear();
        selectionChanged();
    }
}

Selection::~Selection() {}

bool
Selection::isValid() const
{
    return true;
}
}  // namespace usd
