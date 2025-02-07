// Copyright 2024-present Rapid Images AB
// https://gitlab.rapidimages.se/one-cx/pipeline/usdviewer

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
