// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "application.h"
#include "viewer.h"

int
main(int argc, char* argv[])
{
    usdviewer::Application app(argc, argv);
    usdviewer::Viewer viewer;
    viewer.setArguments(QCoreApplication::arguments());
    viewer.show();
    return app.exec();
}
