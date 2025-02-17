// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include "mousefilter.h"
#include <QColorDialog>
#include <QObject>
#include <QPointer>

// generated files
#include "ui_usdviewer.h"

class UsdviewerPrivate : public QObject {
    Q_OBJECT
    public:
        UsdviewerPrivate();
        void init();
    public Q_SLOTS:
        void clearcolor();
    
    public:
        struct Data {
            QStringList arguments;
            QScopedPointer<Mousefilter> clearcolorfilter;
            QScopedPointer<Ui_Usdviewer> ui;
            QPointer<Usdviewer> viewer;
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
    d.ui->setupUi(d.viewer.data());
    d.clearcolorfilter.reset(new Mousefilter);
    d.ui->clearcolor->installEventFilter(d.clearcolorfilter.data());
    // connect
    connect(d.clearcolorfilter.data(), &Mousefilter::pressed, this, &UsdviewerPrivate::clearcolor);
}

void
UsdviewerPrivate::clearcolor()
{
    QColor color = QColorDialog::getColor(d.ui->imagingglwidget->clearcolor(), d.viewer.data(), "Select Color");
    if (color.isValid()) {
        d.ui->imagingglwidget->set_clearcolor(color);
        d.ui->clearcolor->setStyleSheet("background-color: " + color.name() + ";");
    }
}

#include "usdviewer.moc"

Usdviewer::Usdviewer(QWidget* parent)
: QMainWindow(parent)
, p(new UsdviewerPrivate())
{
    p->d.viewer = this;
    p->init();
}

Usdviewer::~Usdviewer()
{
}

void
Usdviewer::set_arguments(const QStringList& arguments)
{
    qDebug() << "arguments: " << arguments;
    
    p->d.arguments = arguments;
    for (int i = 0; i < arguments.size(); ++i) {
        if (arguments[i] == "--open" && i + 1 < arguments.size()) {
            QString filename = arguments[i + 1];
            if (!filename.isEmpty()) {
                qDebug() << "opening file:" << filename;
                if (!p->d.ui->imagingglwidget->load_file(filename)) {
                    qDebug() << "could not load usd file: " << filename;
                }
            }
            break;
        }
    }
}
