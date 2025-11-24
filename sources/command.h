// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "datamodel.h"
#include "selectionmodel.h"
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class Command {
public:
    using Func = std::function<void(DataModel*, SelectionModel*)>;
    Command(Func redo, Func undo = Func())
        : m_redo(std::move(redo))
        , m_undo(std::move(undo))
    {}
    void execute(DataModel* s, SelectionModel* sel)
    {
        if (m_redo)
            m_redo(s, sel);
    }
    void undo(DataModel* s, SelectionModel* sel)
    {
        if (m_undo)
            m_undo(s, sel);
    }

    bool isUndoable() const { return static_cast<bool>(m_undo); }

private:
    Func m_redo;
    Func m_undo;
};

Command
loadPayloads(const QList<SdfPath>& paths, const QString& variantSet = QString(),
             const QString& variantValue = QString());
Command
unloadPayloads(const QList<SdfPath>& paths);
Command
isolate(const QList<SdfPath>& paths);
Command
select(const QList<SdfPath>& paths);
Command
show(const QList<SdfPath>& paths, bool recursive);
Command
hide(const QList<SdfPath>& paths, bool recursive);
}  // namespace usd
