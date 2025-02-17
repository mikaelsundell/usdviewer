// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "mousefilter.h"
#include <QMouseEvent>

Mousefilter::Mousefilter(QObject* parent)
: QObject(parent)
{
}

Mousefilter::~Mousefilter()
{
}

bool
Mousefilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit pressed();
        }
    }
    return QObject::eventFilter(obj, event);
}
