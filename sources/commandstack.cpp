// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer


#include "commandstack.h"
#include "command.h"
#include <QPointer>
#include <QVector>

namespace usd {
class CommandStackPrivate {
public:
    void push(Command* command);

public:
    struct Data {
        qsizetype index = -1;
        QVector<Command*> stack;
        QPointer<DataModel> dataModel;
        QPointer<SelectionModel> selectionModel;
    };
    Data d;
};

void
CommandStackPrivate::push(Command* command)
{
    if (d.index + 1 < d.stack.size()) {
        for (qsizetype i = d.index + 1; i < d.stack.size(); ++i)
            delete d.stack[i];
        d.stack.resize(d.index + 1);
    }
    d.stack.append(command);
    d.index = d.stack.size() - 1;
}

CommandStack::CommandStack(QObject* parent)
    : QObject(parent)
    , p(new CommandStackPrivate)
{}

CommandStack::~CommandStack() {}

void
CommandStack::execute(Command* command)
{
    if (!command)
        return;
    command->execute(p->d.dataModel, p->d.selectionModel);
    p->push(command);
    Q_EMIT commandExecuted(command);
    Q_EMIT changed();
}

bool
CommandStack::canUndo() const
{
    return p->d.index >= 0;
}

bool
CommandStack::canRedo() const
{
    return p->d.index + 1 < p->d.stack.size();
}

void
CommandStack::undo()
{
    if (!canUndo())
        return;

    Command* cmd = p->d.stack[p->d.index];
    cmd->undo(p->d.dataModel, p->d.selectionModel);
    p->d.index--;

    Q_EMIT changed();
}

void
CommandStack::redo()
{
    if (!canRedo())
        return;

    p->d.index++;
    Command* cmd = p->d.stack[p->d.index];
    cmd->execute(p->d.dataModel, p->d.selectionModel);

    Q_EMIT changed();
}

DataModel*
CommandStack::dataModel() const
{
    return p->d.dataModel;
}

void
CommandStack::setDataModel(DataModel* dataModel)
{
    if (p->d.dataModel != dataModel) {
        p->d.dataModel = dataModel;
    }
}

SelectionModel*
CommandStack::selectionModel()
{
    return p->d.selectionModel;
}

void
CommandStack::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
    }
}
}  // namespace usd
