// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022 - present Mikael Sundell.
// https://github.com/mikaelsundell/flipman

#include "pythoninterpreter.h"
#include <QPointer>

namespace usdviewer {
class PythonInterpreterPrivate {
public:
    PythonInterpreterPrivate();
    ~PythonInterpreterPrivate();
    void init();

public:
    struct Data {
        QPointer<PythonInterpreter> object;
    };
    Data d;
};

PythonInterpreterPrivate::PythonInterpreterPrivate() {}

PythonInterpreterPrivate::~PythonInterpreterPrivate() {}

void
PythonInterpreterPrivate::init()
{
}

PythonInterpreter::PythonInterpreter(QObject* parent)
    : QObject(parent)
    , p(new PythonInterpreterPrivate())
{
    p->d.object = this;
    p->init();
    
}

PythonInterpreter::~PythonInterpreter() {}
}  // namespace usdviewer
