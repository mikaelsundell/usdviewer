// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer


#include "commandstack.h"
#include "application.h"
#include "command.h"
#include <QPointer>
#include <QVector>

namespace usdviewer {
class CommandStackPrivate {
public:
    CommandStackPrivate();
    ~CommandStackPrivate();
    void push(Command* command);

public:
    struct Data {
        qsizetype index = -1;
        QVector<Command*> stack;
    };
    Data d;
};

CommandStackPrivate::CommandStackPrivate() {}

CommandStackPrivate::~CommandStackPrivate()
{
    for (Command* cmd : d.stack)
        delete cmd;
}

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

CommandStack::~CommandStack() = default;

void
CommandStack::execute(Command* command)
{
    if (!command)
        return;

    const bool prevCanUndo = canUndo();
    const bool prevCanRedo = canRedo();
    const bool prevCanClear = canClear();
    
    command->execute(dataModel(), selectionModel());
    p->push(command);

    Q_EMIT commandExecuted(command);
    Q_EMIT changed();

    if (prevCanUndo != canUndo())
        Q_EMIT canUndoChanged(canUndo());

    if (prevCanRedo != canRedo())
        Q_EMIT canRedoChanged(canRedo());

    if (prevCanClear != canClear())
        Q_EMIT canClearChanged(canClear());
}

bool
CommandStack::canUndo() const
{
    return p->d.index >= 0;
}

bool
CommandStack::canClear() const
{
    return !p->d.stack.isEmpty();
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

    const bool prevCanUndo = canUndo();
    const bool prevCanRedo = canRedo();
    const bool prevCanClear = canClear();

    qDebug() << "CommandStack::undo";

    Command* cmd = p->d.stack[p->d.index];
    cmd->undo(dataModel(), selectionModel());
    p->d.index--;

    Q_EMIT changed();

    if (prevCanUndo != canUndo())
        Q_EMIT canUndoChanged(canUndo());

    if (prevCanRedo != canRedo())
        Q_EMIT canRedoChanged(canRedo());

    if (prevCanClear != canClear())
        Q_EMIT canClearChanged(canClear());
}

void
CommandStack::redo()
{
    if (!canRedo())
        return;

    const bool prevCanUndo = canUndo();
    const bool prevCanRedo = canRedo();
    const bool prevCanClear = canClear();

    qDebug() << "CommandStack::redo";

    p->d.index++;
    Command* cmd = p->d.stack[p->d.index];
    cmd->execute(dataModel(), selectionModel());

    Q_EMIT changed();

    if (prevCanUndo != canUndo())
        Q_EMIT canUndoChanged(canUndo());

    if (prevCanRedo != canRedo())
        Q_EMIT canRedoChanged(canRedo());

    if (prevCanClear != canClear())
        Q_EMIT canClearChanged(canClear());
}

void
CommandStack::clear()
{
    const bool prevCanUndo = canUndo();
    const bool prevCanRedo = canRedo();
    const bool prevCanClear = canClear();

    qDebug() << "CommandStack::clear";

    for (Command* cmd : p->d.stack)
        delete cmd;

    p->d.stack.clear();
    p->d.index = -1;

    Q_EMIT changed();

    if (prevCanUndo != canUndo())
        Q_EMIT canUndoChanged(canUndo());

    if (prevCanRedo != canRedo())
        Q_EMIT canRedoChanged(canRedo());

    if (prevCanClear != canClear())
        Q_EMIT canClearChanged(canClear());
}

}  // namespace usdviewer
