#!/bin/bash
##  Copyright 2022-present Contributors to the usdviewer project.
##  SPDX-License-Identifier: BSD-3-Clause
##  https://github.com/mikaelsundell/usdviewer

# usage
usage()
{
cat << EOF
macpython.sh -- Deploy a python mac bundle

usage: $0 [options]

Options:
   -h, --help              Print help message
   -v, --verbose           Print verbose output
   -b, --bundle            Path to the .app bundle (required)
   -x, --xcode             Path to the xcode location (required)
   -f, --framework         Path to the framework (required)
   -o, --overwrite         Overwrite existing files (default: false)
EOF
}

overwrite=0
processed_paths=""

# parse arguments
i=0; argv=()
for ARG in "$@"; do
    argv[$i]="${ARG}"
    i=$((i + 1))
done

i=0
while test $i -lt $# ; do
    ARG="${argv[$i]}"
    case "${ARG}" in
        -h|--help) 
            usage
            exit;;
        -v|--verbose)
            verbose=1;;
        -b|--bundle) 
            i=$((i + 1)) 
            bundle=${argv[$i]};;
        -x|--xcode) 
            i=$((i + 1)) 
            xcode=${argv[$i]};;
        -f|--framework) 
            i=$((i + 1)) 
            framework=${argv[$i]};;
        -o|--overwrite)
            overwrite=1;;
        *) 
            echo "Unknown argument '${ARG}'"
            exit 1;;
    esac
    i=$((i + 1))
done

# test arguments
if [ -z "${bundle}" ]; then
    usage
    exit 1
fi

if [ -z "${xcode}" ]; then
    usage
    exit 1
fi

if [ -z "${framework}" ]; then
    usage
    exit 1
fi

if [ ! -d "${bundle}" ]; then
    echo "App bundle path '${bundle}' is not a directory."
    exit 1
fi

if [ ! -d "${xcode}" ]; then
    echo "Xcode path '${framework}' is not a directory."
    exit 1
fi

IFS=',' read -r -a path_list <<< "$paths"

# framework
function copy_framework() {
    local xcode_path="${1}"
    local framework_path="${2}"
    local deploy_path="${3}"

    local copy_path="${xcode_path}/${framework_path}"
    local framework_name=$(basename "${copy_path}")
    local framework_base=${framework_name%.framework}
    echo "Copying framework '${framework_name}' from '${copy_path}' to '${deploy_path}'"

    local deploy_framework_path="${deploy_path}/${framework_name}"
    local deploy_versions_path="${deploy_framework_path}/Versions"
    mkdir -p "${deploy_versions_path}"

    local version_folder=$(ls -1 "${copy_path}/Versions" | grep -E '^[0-9]+(\.[0-9]+)?$' | head -n 1)
    if [ -z "${version_folder}" ]; then
        echo "Error: No versioned folder found in ${copy_path}/Versions"
        return 1
    fi

    local deploy_version_path="${deploy_versions_path}/${version_folder}"
    local source_version_path="${copy_path}/Versions/${version_folder}"
    mkdir -p "${deploy_version_path}"
    cp "${source_version_path}/${framework_base}" "${deploy_version_path}/"

    local deploy_resources_path="${deploy_version_path}/Resources"
    mkdir -p "${deploy_resources_path}"
    cp "${source_version_path}/Resources/Info.plist" "${deploy_resources_path}/"

    ln -sfn "${version_folder}" "${deploy_versions_path}/Current"
    ln -sfn "Versions/Current/Resources" "${deploy_framework_path}/Resources"
    ln -sfn "Versions/Current/${framework_base}" "${deploy_framework_path}/${framework_base}"
}

function deploy_framework() {
    local deploy_path="${1}"
    local xcode_path="${2}"
    local framework_path="${3}"
    local framework_dir_path="${4}"

    local dependency_paths=$(otool -L "${deploy_path}" | tail -n+2 | awk '{print $1}')
    local dependency_path

    for dependency_path in ${dependency_paths[@]}; do
        if [[ "${dependency_path}" == *"${framework_path}"* ]]; then
            local deploy_id="@executable_path/../Frameworks/${framework_path}/Versions/Current/${framework_base}"
            local change_id="${dependency_path/@rpath/@executable_path/../Frameworks}"

            # delete old rpaths
            local rpaths
            rpaths=$(otool -l "${deploy_path}" | awk '/cmd LC_RPATH/ {getline; getline; print $2}')
            while IFS= read -r rpath; do
                if [[ "$rpath" == "${xcode_path}"* ]]; then
                    echo "- deleting existing rpaths: $rpath"
                    install_name_tool -delete_rpath "$rpath" "$deploy_path"
                fi
            done <<< "$rpaths"

            # add new rpath
            local bundle_rpath="@executable_path/../Frameworks"
            if otool -l "$deploy_path" | grep -q "path $bundle_rpath"; then
                echo "- rpath already exists: $bundle_rpath"
            else
                echo "- adding new rpath: $bundle_rpath"
                install_name_tool -add_rpath "${bundle_rpath}" "${deploy_path}" 
            fi

            continue
        else 
            depedency_absolute_path="${dependency_path/@executable_path\/..\/Frameworks/${framework_dir_path}}"
            if [[ -f "${depedency_absolute_path}" && `file "${depedency_absolute_path}" | grep 'shared library'` ]]; then
                if echo "$processed_paths" | grep -q "|$depedency_absolute_path|"; then
                    continue
                fi
                echo "deploy for ${deploy_path}"
                processed_paths="${processed_paths}|$depedency_absolute_path|"
                deploy_framework "${depedency_absolute_path}" "${xcode_path}" "${framework_path}" "${framework_dir_path}"
            fi
        fi 
    done
}

# main
function main {
    frameworks_dir="${bundle}/Contents/Frameworks"
    if [ ! -d "$frameworks_dir" ]; then
        echo "Creating Frameworks directory: $frameworks_dir"
        mkdir -p "$frameworks_dir"
    fi

    copy_framework "${xcode}" "${framework}" "${frameworks_dir}"

    macos_dir="${bundle}/Contents/MacOS"
    executables=($(find "$macos_dir" -type f -perm +111))
    if [ ${#executables[@]} -eq 0 ]; then
        echo "No executables found in ${macos_dir}"
        return 1
    fi

    for exe in "${executables[@]}"; do
        deploy_framework "$exe" "${xcode}" "${framework}" "${frameworks_dir}"
    done
}

main
