// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "test.h"
#include "usdviewer.h"
#include <QApplication>

int
main(int argc, char* argv[])
{
    if (0) {
        test();
    }
    QApplication app(argc, argv);
    usd::Viewer viewer;
    viewer.setArguments(QCoreApplication::arguments());
    viewer.show();
    return app.exec();
}
