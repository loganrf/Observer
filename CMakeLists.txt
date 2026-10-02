# Observer — wildlife sighting log for OpenCPN
# Copyright (C) 2026 SIGE
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Resolves OBSERVER_VERSION (YYYY.M.N) before project() so the project
# carries the calendar version.
include(ObserverVersion)

project(
  observer_pi
  VERSION ${OBSERVER_VERSION_CORE}
  DESCRIPTION "Log wildlife sightings from the chart"
  LANGUAGES CXX
)

# -------- Plugin identity --------
set(PACKAGE_NAME ${PROJECT_NAME})        # Library, data dir and catalog name
set(PLUGIN_API_NAME "Observer")          # GetCommonName() and metadata <name>
set(PKG_SUMMARY "Log wildlife sightings with position, time and media")
set(PKG_DESCRIPTION
    "Observer records wildlife sightings while under way. One click captures the time, position, course, speed, heading, depth, water temperature and wind; add the species, count, a short description and an optional photo or video. Sightings are kept in a plain CSV log, shown on the chart and exported as CSV, GeoJSON or GPX."
)
set(PKG_AUTHOR "SIGE")
set(PKG_HOMEPAGE "https://github.com/loganrf/Observer")
set(PKG_INFO_URL "https://github.com/loganrf/Observer#readme")
set(PKG_IS_OPEN_SOURCE "yes")

# The OpenCPN plugin API this build targets; see opencpn-libs/api-*.
# API 1.18 loads in OpenCPN 5.8 and every later 5.x release.
set(OBSERVER_API_LIB "api-18" CACHE STRING "API directory in opencpn-libs")

option(OBSERVER_BUILD_TESTS "Build the unit tests" ON)

# -------- Compiler setup --------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif ()
if (MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
  add_compile_options(/utf-8)
endif ()

# Stay within the wxWidgets 3.2.2 ABI so one build runs against any
# 3.2.x the host OpenCPN ships with.
add_compile_definitions(wxABI_VERSION=30202)

include(ObserverTarget)  # plugin_target, plugin_target_version, plugin_arch

# -------- wxWidgets --------
if (APPLE)
  set(wxWidgets_USE_STATIC OFF)
endif ()
set(wxWidgets_USE_UNICODE ON)
set(wxWidgets_USE_DEBUG OFF)
find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# -------- Core library (no OpenCPN dependency, unit tested) --------
add_library(
  observer_core STATIC
  src/core/csv.cpp
  src/core/csv.h
  src/core/geo.cpp
  src/core/geo.h
  src/core/nmea.cpp
  src/core/nmea.h
  src/core/observation.cpp
  src/core/observation.h
  src/core/store.cpp
  src/core/store.h
  src/core/export.cpp
  src/core/export.h
  src/core/time_util.cpp
  src/core/time_util.h
)
target_include_directories(observer_core PUBLIC src/core)
target_link_libraries(observer_core PUBLIC ${wxWidgets_LIBRARIES})

# -------- The plugin --------
add_library(
  ${PACKAGE_NAME} SHARED
  src/observer_pi.cpp
  src/observer_pi.h
  src/brand.cpp
  src/brand.h
  src/chart_marks.cpp
  src/chart_marks.h
  src/log_dialog.cpp
  src/log_dialog.h
  src/list_dialog.cpp
  src/list_dialog.h
  src/prefs_dialog.cpp
  src/prefs_dialog.h
  src/readout.cpp
  src/readout.h
  src/settings.cpp
  src/settings.h
)

add_subdirectory(opencpn-libs/${OBSERVER_API_LIB})
string(REPLACE "." ";" _api_parts "${API_VERSION}")
list(GET _api_parts 0 OCPN_API_MAJOR)
list(GET _api_parts 1 OCPN_API_MINOR)
target_link_libraries(${PACKAGE_NAME} PRIVATE observer_core ocpn::api)
target_include_directories(${PACKAGE_NAME} PRIVATE src
                                                   ${CMAKE_BINARY_DIR}/include)

configure_file(src/config.h.in ${CMAKE_BINARY_DIR}/include/config.h @ONLY)

if (APPLE)
  # Symbols from the OpenCPN executable resolve when the plugin loads.
  target_link_options(${PACKAGE_NAME} PRIVATE -undefined dynamic_lookup)
elseif (UNIX)
  target_link_options(${PACKAGE_NAME} PRIVATE -Wl,-Bsymbolic)
  set_target_properties(${PACKAGE_NAME} PROPERTIES INSTALL_RPATH
                                                   "$ORIGIN:$ORIGIN/..")
endif ()

include(ObserverPackage)  # install layout, metadata.xml and `tarball`

if (OBSERVER_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_subdirectory(tests)
endif ()
