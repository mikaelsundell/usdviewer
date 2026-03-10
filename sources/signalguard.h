// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QObject>

namespace usd {

class SignalGuardPrivate;

/**
 * @class SignalGuard
 * @brief Utility for temporarily blocking signals on QObject instances.
 *
 * Provides a simple mechanism to guard sections of code where Qt signals
 * should be suppressed. This is typically used to prevent recursive updates
 * or unwanted signal emissions while programmatically modifying widgets
 * or data models.
 *
 * The guard can be attached to a QObject and activated using
 * beginGuard() and endGuard().
 */
class SignalGuard {
public:
    /**
     * @brief Constructs a SignalGuard.
     */
    SignalGuard();

    /**
     * @brief Destroys the SignalGuard.
     */
    ~SignalGuard();

    /** @name Guard Management */
    ///@{

    /**
     * @brief Attaches the guard to a QObject.
     *
     * The object's signals will be blocked while the guard
     * is active.
     *
     * @param object QObject to guard.
     */
    void attach(QObject* object);

    /**
     * @brief Begins the guarded section.
     *
     * Blocks signals on the attached object.
     */
    void beginGuard();

    /**
     * @brief Ends the guarded section.
     *
     * Restores the original signal state of the object.
     */
    void endGuard();

    /**
     * @brief Returns whether the guard is currently active.
     */
    bool isGuarding() const;

    ///@}

private:
    QScopedPointer<SignalGuardPrivate> p;
};

}  // namespace usd
