// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include <QOpenGLDebugLogger>
#include <QOpenGLContext>
#include <QDebug>

class GLDebugger : public QObject {
    Q_OBJECT
    public:
    explicit GLDebugger(QOpenGLContext *context, QObject *parent = nullptr);

    private Q_SLOTS:
        void handleLoggedMessage(const QOpenGLDebugMessage &message);

    private:
        QOpenGLDebugLogger* logger;
};
