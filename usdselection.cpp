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
    struct Data
    {
        QList<SdfPath> selection;
    };
    Data d;
};

SelectionPrivate::SelectionPrivate()
{
}

SelectionPrivate::~SelectionPrivate()
{
}

Selection::Selection(QObject* parent)
: QObject(parent)
, p(new SelectionPrivate())
{
}

bool
Selection::isSelected(const SdfPath& path) const
{
    return p->d.selection.contains(path);
}

void
Selection::addItem(const SdfPath& path)
{
    if (!p->d.selection.contains(path)) {
        p->d.selection.append(path);
        selectionChanged();
    }
}

void
Selection::removeItem(const SdfPath& path)
{
    Q_ASSERT("item is not selected" && isSelected(path));
    p->d.selection.removeOne(path);
    selectionChanged();
}

QList<SdfPath>
Selection::selection() const
{
    return p->d.selection;
}

void
Selection::clear()
{
    if (p->d.selection.size()) {
        p->d.selection.clear();
        selectionChanged();
    }
}

Selection::~Selection()
{
}

bool
Selection::isValid() const
{
    return true;
}
}
