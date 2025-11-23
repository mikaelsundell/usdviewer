# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2024 - present Mikael Sundell
# https://github.com/mikaelsundell/usdviewer

cmake_minimum_required(VERSION 3.14)

set(OpenUSD_COMPONENTS
  AR
  ARCH
  BOOST
  PYTHON
  CAMERAUTIL
  GF
  GLF
  HGIGL
  PLUG
  SDF
  TF
  USD
  USDGEOM
  USDIMAGING
  USDIMAGINGGL
  VT
  WORK
)

find_path(OpenUSD_INCLUDE_DIR
  NAMES pxr/usd/usd/api.h
  PATH_SUFFIXES include
  PATHS ${CMAKE_PREFIX_PATH}
)

set(OpenUSD_LIBRARIES "")
set(OpenUSD_ALL_FOUND TRUE)

foreach(component ${OpenUSD_COMPONENTS})
  string(TOLOWER ${component} component_name)
  find_library(OpenUSD_${component}_LIBRARY
    NAMES usd_${component_name}
    PATH_SUFFIXES lib
    PATHS ${CMAKE_PREFIX_PATH}
  )

  if (NOT OpenUSD_${component}_LIBRARY)
    message(WARNING "OpenUSD: Missing library usd_${component_name}")
    set(OpenUSD_ALL_FOUND FALSE)
  else()
    list(APPEND OpenUSD_LIBRARIES ${OpenUSD_${component}_LIBRARY})
  endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenUSD
  REQUIRED_VARS OpenUSD_INCLUDE_DIR
  HANDLE_COMPONENTS
)

if (OpenUSD_INCLUDE_DIR AND OpenUSD_ALL_FOUND)
  set(OpenUSD_FOUND TRUE)
  set(OpenUSD_INCLUDE_DIRS ${OpenUSD_INCLUDE_DIR})
  add_library(OpenUSD::OpenUSD INTERFACE IMPORTED GLOBAL)
  set_target_properties(OpenUSD::OpenUSD PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OpenUSD_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${OpenUSD_LIBRARIES}"
  )

else()
  message(FATAL_ERROR "OpenUSD: Required components missing. Ensure all libraries are installed.")
endif()
