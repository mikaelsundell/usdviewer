#!/bin/bash
##  Copyright 2022-present Contributors to the usdviewer project.
##  SPDX-License-Identifier: BSD-3-Clause
##  https://github.com/mikaelsundell/usdviewer

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
machine_arch=$(uname -m)
macos_version=$(sw_vers -productVersion)
major_version=$(echo "$macos_version" | cut -d '.' -f 1)
app_name="USDViewer"
pkg_name="usdviewer"

# signing
sign_code=OFF
developerid_identity=""

# permissions
permission_app() {
    local bundle_path="$1"
    find "$bundle_path" -type f \( -name "*.dylib" -o -name "*.so" -o -name "*.bundle" -o -name "*.framework" -o -perm +111 \) | while read -r file; do
        echo "setting permissions for $file..."
        chmod o+r "$file"
    done
}

# check signing
parse_args() {
    while [[ "$#" -gt 0 ]]; do
        case $1 in
            --target=*) 
                major_version="${1#*=}" ;;
            --sign)
                sign_code=ON ;;
            --github)
                github=ON ;;
            *)
                build_type="$1" # save it in build_type if it's not a recognized flag
                ;;
        esac
        shift
    done
}
parse_args "$@"

# target
if [ -z "$major_version" ]; then
    macos_version=$(sw_vers -productVersion)
    major_version=$(echo "$macos_version" | cut -d '.' -f 1)
fi
export MACOSX_DEPLOYMENT_TARGET=$major_version
export CMAKE_OSX_DEPLOYMENT_TARGET=$major_version

# exit on error
set -e 

# clear
clear

# build type
if [ "$build_type" != "debug" ] && [ "$build_type" != "release" ] && [ "$build_type" != "all" ]; then
    echo "invalid build type: $build_type (use 'debug', 'release', or 'all')"
    exit 1
fi

echo "Building usdviewer for $build_type"
echo "---------------------------------"

# signing
if [ "$sign_code" == "ON" ]; then
    default_developerid_identity=${DEVELOPERID_IDENTITY:-}

    read -p "enter Developer ID certificate identity [$default_developerid_identity]: " input_developerid_identity
    developerid_identity=${input_developerid_identity:-$default_developerid_identity}

    if [[ ! "$developerid_identity" == *"Developer ID"* ]]; then
        echo "Developer ID certificate identity must contain 'Developer ID', required for github distribution."
    fi

    echo ""
fi

# check if cmake is in the path
if ! command -v cmake &> /dev/null; then
    echo "cmake not found in the PATH, will try to set to /Applications/CMake.app/Contents/bin"
    export PATH=$PATH:/Applications/CMake.app/Contents/bin
    if ! command -v cmake &> /dev/null; then
        echo "cmake could not be found, please make sure it's installed"
        exit 1
    fi
fi

# check if cmake version is compatible
if ! [[ $(cmake --version | grep -o '[0-9]\+\(\.[0-9]\+\)*' | head -n1) < "3.28.0" ]]; then
    echo "cmake version is not compatible with Qt, must be before 3.28.0 for multi configuration"
    exit 1;
fi

# build usdviewer
build_usdviewer() {
    local build_type="$1"

    # cmake
    export PATH=$PATH:/Applications/CMake.app/Contents/bin &&

    # script dir
    cd "$script_dir"

    # clean dir
    build_dir="$script_dir/build.$build_type"
    if [ -d "$build_dir" ]; then
        rm -rf "$build_dir"
    fi

    # build dir
    mkdir -p "build.$build_type"
    cd "build.$build_type"

    # prefix dir
    if ! [ -d "$THIRDPARTY_DIR" ]; then
        echo "could not find 3rdparty project in: $THIRDPARTY_DIR"
        exit 1
    fi
    prefix="$THIRDPARTY_DIR"

    # xcode build
    xcode_type=$(echo "$build_type" | awk '{ print toupper(substr($0, 1, 1)) tolower(substr($0, 2)) }')

    # build
    cmake .. -DDEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" -DCMAKE_MODULE_PATH="$script_dir/modules" -DCMAKE_PREFIX_PATH="$prefix" -G Xcode
    cmake --build . --config $xcode_type --parallel &&

    if [ "$github" == "ON" ]; then
        dmg_file="$script_dir/${pkg_name}_macOS${major_version}_${machine_arch}_${build_type}.dmg"
        if [ -f "$dmg_file" ]; then
            rm -f "$dmg_file"
        fi

        # deploy
        $script_dir/scripts/macdeploy.sh -b "$xcode_type/${app_name}.app" -m "$prefix/bin/macdeployqt"

        # python
        $script_dir/scripts//macxcode.sh -b "$xcode_type/${app_name}.app" -f "/Applications/Xcode.app/Contents/Developer/Library/Frameworks" -f "Python3.framework"

        # usd plugins
        plugins="$prefix/lib/usd"
        app_plugins="$xcode_type/${app_name}.app/Contents/Frameworks"

        if [ -d "$plugins" ]; then
            echo "Copy usd plugins to bundle $app_plugins"
            mkdir -p "$app_plugins"
            cp -R "$plugins" "$app_plugins"
        else
            echo "Could not find usd plugins, will be skipped."
        fi

        # todo: temp
        codesign --force --deep --sign - "$xcode_type/${app_name}.app"

        # sign
        if [ -n "$developerid_identity" ]; then
            if [ "$sign_code" == "ON" ]; then
                codesign --force --deep --sign "$developerid_identity" --timestamp --options runtime "$xcode_type/${app_name}.app"
            fi
        else 
            echo "Developer ID identity must be set for github distribution, sign will be skipped."
        fi

        # deploydmg
        #$script_dir/scripts/macdmg.sh -b "$xcode_type/${app_name}.app" -d "$dmg_file"
        #if [ -n "$developerid_identity" ]; then
        #    if [ "$sign_code" == "ON" ]; then
        #        codesign --force --deep --sign "$developerid_identity" --timestamp --options runtime --verbose "$dmg_file"
        #    fi
        #else 
        #    echo "Developer ID identity must be set for github distribution, sign will be skipped."
        #fi
    fi
}

# build types
if [ "$build_type" == "all" ]; then
    build_usdviewer "debug"
    build_usdviewer "release"
else
    build_usdviewer "$build_type"
fi