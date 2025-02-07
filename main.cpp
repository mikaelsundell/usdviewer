// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/flipman

#include "usdviewer.h"
#include "test.h"
#include <QApplication>

int
main(int argc, char* argv[])
{
    if (0) {
        test();
    }
    QApplication app(argc, argv);
    Usdviewer* usdviewer = new Usdviewer();
    usdviewer->set_arguments(QCoreApplication::arguments());
    usdviewer->show();
    return app.exec();
}
