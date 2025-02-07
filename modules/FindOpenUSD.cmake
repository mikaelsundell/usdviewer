# Copyright 2024-present Rapid Images AB
# https://gitlab.rapidimages.se/one-cx/pipeline/usdviewer

find_path(OpenUSD_INCLUDE_DIR
  NAMES pxr/usd/usd/api.h
  PATH_SUFFIXES include
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_BOOST_LIBRARY
  NAMES usd_boost
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_PYTHON_LIBRARY
  NAMES usd_python
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_USD_LIBRARY
  NAMES usd_usd
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_USD_GEOM_LIBRARY
  NAMES usd_usdGeom
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_USD_SDF_LIBRARY
  NAMES usd_sdf
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_USD_GF_LIBRARY
  NAMES usd_gf
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_USD_TF_LIBRARY
  NAMES usd_tf
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

find_library(OpenUSD_USD_VT_LIBRARY
  NAMES usd_vt
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)


find_library(OpenUSD_USD_USDIMAGINGGL_LIBRARY
  NAMES usd_usdImagingGL
  PATH_SUFFIXES lib
  PATHS ${CMAKE_PREFIX_PATH}
)

if (OpenUSD_INCLUDE_DIR AND OpenUSD_USD_LIBRARY)
  set(OpenUSD_FOUND TRUE)
  set(OpenUSD_LIBRARIES 
    ${OpenUSD_BOOST_LIBRARY}
    ${OpenUSD_PYTHON_LIBRARY}
    ${OpenUSD_USD_LIBRARY} 
    ${OpenUSD_USD_GEOM_LIBRARY} 
    ${OpenUSD_USD_SDF_LIBRARY}
    ${OpenUSD_USD_GF_LIBRARY}
    ${OpenUSD_USD_TF_LIBRARY}
    ${OpenUSD_USD_VT_LIBRARY}
    ${OpenUSD_USD_USDIMAGINGGL_LIBRARY}
  )
  set(OpenUSD_INCLUDE_DIRS ${OpenUSD_INCLUDE_DIR})

  # Report to the user
  message(STATUS "Found OpenUSD: ${OpenUSD_LIBRARIES}")
  message(STATUS "Include directory: ${OpenUSD_INCLUDE_DIR}")

else()
  set(OpenUSD_FOUND FALSE)
  message(WARNING "OpenUSD library not found.")
endif()

if (OpenUSD_FOUND)
  add_library(OpenUSD::OpenUSD INTERFACE IMPORTED)
  set_target_properties(OpenUSD::OpenUSD PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${OpenUSD_INCLUDE_DIRS}
    INTERFACE_LINK_LIBRARIES "${OpenUSD_LIBRARIES}"
  )
endif()
