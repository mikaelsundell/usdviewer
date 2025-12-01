// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QObject>

namespace usd {
class SignalGuardPrivate;
class SignalGuard {
public:
    SignalGuard();
    ~SignalGuard();
    void attach(QObject* object);
    void beginGuard();
    void endGuard();
    bool isGuarding() const;

private:
    QScopedPointer<SignalGuardPrivate> p;
};
}  // namespace usd
