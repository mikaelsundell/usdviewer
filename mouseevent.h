// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once
#include <QObject>

class MouseEvent : public QObject {
        Q_OBJECT
    public:
        MouseEvent(QObject* object = nullptr);
        virtual ~MouseEvent();

    Q_SIGNALS:
        void pressed();

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override;
};
