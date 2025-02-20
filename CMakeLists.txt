# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2024 - present Mikael Sundell
# https://github.com/mikaelsundell/usdviewer

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
find_package (OpenUSD REQUIRED)
find_package (Python3 COMPONENTS Interpreter Development REQUIRED)

file (GLOB project_sources
    "${CMAKE_SOURCE_DIR}/*.h"
    "${CMAKE_SOURCE_DIR}/*.cpp"
    "${CMAKE_SOURCE_DIR}/*.ui"
)

set (project_bundle
    "${CMAKE_SOURCE_DIR}/resources/MacOSXBundle.plist.in"
)

set (project_version "0.0.0")
set (project_copyright "Copyright 2025-present Contributors to the ${app_name} project")
set (project_identifier "com.github.mikaelsundell.${project_name}")
set (project_url "https://github.com/mikaelsundell/usdviewer")

if (APPLE)
    file (GLOB project_resources
        "${CMAKE_SOURCE_DIR}/resources/*.icns"
    )
    set (MACOSX_BUNDLE_EXECUTABLE_NAME ${project_name})
    set (MACOSX_BUNDLE_INFO_STRING ${project_name})
    set (MACOSX_BUNDLE_BUNDLE_NAME ${project_name})
    set (MACOSX_BUNDLE_GUI_IDENTIFIER ${project_identifier})
    set (MACOSX_BUNDLE_ICON_FILE usdviewer.icns)
    set (MACOSX_BUNDLE_LONG_VERSION_STRING "${project_version}")
    set (MACOSX_BUNDLE_SHORT_VERSION_STRING "${project_version}")
    set (MACOSX_BUNDLE_BUNDLE_VERSION ${MACOSX_BUNDLE_LONG_VERSION_STRING})
    set (MACOSX_BUNDLE_COPYRIGHT "Copyright 2022-present Contributors to the ${app_name} project")
    set_source_files_properties(${project_resources} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    add_executable (${project_name} MACOSX_BUNDLE ${project_sources} ${project_icon} ${project_resources})

    set_target_properties (${project_name} PROPERTIES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
        XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${MACOSX_DEPLOYMENT_TARGET}"
        MACOSX_BUNDLE_INFO_PLIST "${project_bundle}"
    )

    if (NOT DEFINED MACOSX_DEPLOYMENT_TARGET)
        set(MACOSX_DEPLOYMENT_TARGET "15.2")
    endif()

    target_link_libraries (${project_name} 
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate Qt6::OpenGLWidgets Qt6::Widgets
        Python3::Python
        OpenUSD::OpenUSD
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework AppKit")

elseif (WIN32)
    set (APP_ICON "AppIcon.ico")
    add_executable (${project_name} WIN32 ${app_sources} ${APP_ICON})

    target_link_libraries (${project_name}
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate Qt6::OpenGLWidgets Qt6::Widgets
        d3d11 dxgi dwmapi)

elseif (UNIX)
    add_executable (${project_name} ${APP_SOURCES})
    target_link_libraries (${project_name}
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate Qt6::OpenGLWidgets Qt6::Widgets
        pthread dl)

else()
    message(WARNING "Unknown platform. ${project_name} may not be built correctly.")
endif()

target_compile_definitions(${project_name} PRIVATE
    -DPROJECT_NAME="${project_name}"
    -DPROJECT_VERSION="${project_version}"
    -DPROJECT_COPYRIGHT="${project_copyright}"
    -DPROJECT_IDENTIFIER="${project_identifier}"
    -DGITHUB_URL="${project_url}"
)

target_include_directories(${project_name} PRIVATE ${CMAKE_SOURCE_DIR})
