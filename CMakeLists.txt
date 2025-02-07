# Copyright 2024-present Rapid Images AB
# https://gitlab.rapidimages.se/one-cx/pipeline/usdviewer

cmake_minimum_required (VERSION 3.27)
set (project_name "usdviewer")

project (${project_name})

# packages
set (qt6_modules Core Concurrent Gui OpenGLWidgets Widgets)
find_package (Qt6 COMPONENTS ${qt6_modules} CONFIG REQUIRED)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(OpenUSD REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# app
set (app_name "usdviewer")

# sources
set (app_sources
    hydrarenderer.h
    hydrarenderer.cpp
    usdviewer.h
    usdviewer.cpp
    main.cpp
    test.h
    test.cpp
    test.h
    usdviewer.ui
)

set(app_name "usdviewer")
set(APP_VERSION "0.0.1")
set(APP_COPYRIGHT "Copyright 2024-present Contributors to the ${app_name} project")
set(GITHUB_URL "https://github.com/mikaelsundell/flipman")

if (APPLE)
    set(app_icon "resources/usdviewer.icns")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.mikaelsundell.${APP_NAME}")

    add_executable(${project_name} MACOSX_BUNDLE ${app_sources} ${app_icon})
    target_include_directories(${project_name} PRIVATE ${Python3_INCLUDE_DIRS})

    #set_target_properties(${project_name} PROPERTIES
    #    MACOSX_BUNDLE_GUI_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
    #    MACOSX_BUNDLE_INFO_PLIST ${bundle_sources}
    #)

    target_link_libraries(${project_name} 
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate Qt6::OpenGLWidgets Qt6::Widgets
        Python3::Python
        OpenUSD::OpenUSD
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework AppKit")

elseif (WIN32)
    set(APP_ICON "AppIcon.ico")

    add_executable(${project_name} WIN32 ${app_sources} ${APP_ICON})

    #set_target_properties(${project_name} PROPERTIES
    #    OUTPUT_NAME "${APP_NAME}"
    #    VERSION "${APP_VERSION}"
    #)

    # Windows-specific dependencies
    target_link_libraries(${project_name}
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate Qt6::OpenGLWidgets Qt6::Widgets
        d3d11 dxgi dwmapi)

elseif (UNIX)
    add_executable(${project_name} ${APP_SOURCES})

    target_link_libraries(${project_name}
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate Qt6::OpenGLWidgets Qt6::Widgets
        pthread dl)

else()
    message(WARNING "Unknown platform. ${project_name} may not be built correctly.")
endif()

target_compile_definitions(${project_name} PRIVATE
    -DAPP_NAME="${APP_NAME}"
    -DAPP_VERSION="${APP_VERSION}"
    -DAPP_COPYRIGHT="${APP_COPYRIGHT}"
    -DGITHUB_URL="${GITHUB_URL}"
)

target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR})
