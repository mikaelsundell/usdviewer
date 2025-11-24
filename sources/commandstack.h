// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "command.h"
#include "datamodel.h"
#include "selectionmodel.h"

namespace usd {
class CommandStackPrivate;
class CommandStack : public QObject {
    Q_OBJECT
public:
    CommandStack(QObject* parent = nullptr);
    virtual ~CommandStack();
    void execute(Command* command);
    bool canUndo() const;
    bool canRedo() const;

    DataModel* dataModel() const;
    void setDataModel(DataModel* dataModel);

    SelectionModel* selectionModel();
    void setSelectionModel(SelectionModel* selectionModel);

public Q_SLOTS:
    void undo();
    void redo();

Q_SIGNALS:
    void commandExecuted(Command* command);
    void changed();

private:
    QScopedPointer<CommandStackPrivate> p;
};
};  // namespace usd
