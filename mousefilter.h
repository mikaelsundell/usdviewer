// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once
#include <QObject>

class Mousefilter : public QObject
{
    Q_OBJECT
    public:
        Mousefilter(QObject* object = nullptr);
        virtual ~Mousefilter();

    Q_SIGNALS:
        void pressed();
    
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override;
};
