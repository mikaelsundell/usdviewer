// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QMainWindow>
#include <QScopedPointer>

class UsdviewerPrivate;
class Usdviewer : public QMainWindow {
    Q_OBJECT
    public:
        Usdviewer(QWidget* parent = nullptr);
        virtual ~Usdviewer();
        void set_arguments(const QStringList& arguments);

    private:
        QScopedPointer<UsdviewerPrivate> p;
};
