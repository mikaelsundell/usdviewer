// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include <QObject>
#include <QPointer>

// generated files
#include "ui_usdviewer.h"

class UsdviewerPrivate : public QObject {
    Q_OBJECT
    public:
        UsdviewerPrivate();
        void init();
        struct Data {
            QStringList arguments;
            QScopedPointer<Ui_Usdviewer> ui;
            QPointer<Usdviewer> window;      
        };
        Data d;
};

UsdviewerPrivate::UsdviewerPrivate()
{
}

void
UsdviewerPrivate::init()
{
    d.ui.reset(new Ui_Usdviewer());
    d.ui->setupUi(d.window.data());
}

#include "usdviewer.moc"

Usdviewer::Usdviewer(QWidget* parent)
: QMainWindow(parent)
, p(new UsdviewerPrivate())
{
    p->d.window = this;
    p->init();
}

Usdviewer::~Usdviewer()
{
}

void
Usdviewer::set_arguments(const QStringList& arguments)
{
    p->d.arguments = arguments;

    for (int i = 0; i < arguments.size(); ++i) {
        if (arguments[i] == "--open" && i + 1 < arguments.size()) {
            QString filename = arguments[i + 1];
            if (!filename.isEmpty()) {
                qDebug() << "opening file:" << filename;
                if (!p->d.ui->renderer->load_file(filename)) {
                    qDebug() << "could not load usd file: " << filename;
                }
            }
            break;
        }
    }
}
