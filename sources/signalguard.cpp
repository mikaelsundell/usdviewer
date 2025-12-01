// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "signalguard.h"

namespace usd {
class SignalGuardPrivate : public QObject {
public:
    SignalGuardPrivate();
    struct Data {
        QObject* object = nullptr;
        bool guarding = false;
    };
    Data d;
};

SignalGuardPrivate::SignalGuardPrivate() {}

SignalGuard::SignalGuard()
    : p(new SignalGuardPrivate)
{}

SignalGuard::~SignalGuard() = default;

void
SignalGuard::attach(QObject* object)
{
    p->d.object = object;
}

void
SignalGuard::beginGuard()
{
    if (p->d.guarding)
        return;
    p->d.guarding = true;
    if (p->d.object)
        p->d.object->blockSignals(true);
}

void
SignalGuard::endGuard()
{
    if (!p->d.guarding)
        return;
    if (p->d.object)
        p->d.object->blockSignals(false);
    p->d.guarding = false;
}

bool
SignalGuard::isGuarding() const
{
    return p->d.guarding;
}
}  // namespace usd
