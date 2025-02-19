// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QMainWindow>
#include <QScopedPointer>

namespace usd {
class ViewerPrivate;
class Viewer : public QMainWindow {
    public:
        Viewer(QWidget* parent = nullptr);
        virtual ~Viewer();
        void setArguments(const QStringList& arguments);
        
    private:
        QScopedPointer<ViewerPrivate> p;
    };
}
