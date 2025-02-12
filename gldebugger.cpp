// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "gldebugger.h"
#include <QDebug>

#include "gldebugger.moc"

GLDebugger::GLDebugger(QOpenGLContext* context, QObject* parent)
: QObject(parent), logger(new QOpenGLDebugLogger(this))
{
    if (logger->initialize()) {
        connect(logger, &QOpenGLDebugLogger::messageLogged, this, &GLDebugger::handleLoggedMessage);
        logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
        qDebug() << "OpenGL Debug Logger initialized";
    }
    else {
        qDebug() << "Failed to initialize OpenGL Debug Logger.";
    }
}

void
GLDebugger::handleLoggedMessage(const QOpenGLDebugMessage &message) {
    qDebug() << "OpenGL Debug Message:" << message.message();
}
