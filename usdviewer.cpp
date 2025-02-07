// Copyright 2024-present Rapid Images AB
// https://gitlab.rapidimages.se/one-cx/pipeline/usdviewer

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
                qDebug() << "Opening file:" << filename;
                p->d.ui->renderer->load_file(filename);
            }
            break;
        }
    }
}
