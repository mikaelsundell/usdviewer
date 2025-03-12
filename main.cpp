// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "test.h"
#include "usdviewer.h"
#include <QApplication>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>

#include <iostream>

int
main(int argc, char* argv[])
{
    pxr::PlugRegistry& instance = pxr::PlugRegistry::GetInstance();
    pxr::PlugPluginPtrVector plugins = instance.GetAllPlugins();
    std::cout << "LIST PLUGINS: START";
    for (const auto& plugin : plugins) {
        std::cout << "  Path: " << plugin->GetPath() << std::endl;
    }
    std::cout << "LIST PLUGINS: END";

    if (0) {
        test();
    }
    QApplication app(argc, argv);
    usd::Viewer viewer;
    viewer.setArguments(QCoreApplication::arguments());
    viewer.show();
    return app.exec();
}
