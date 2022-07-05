# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

cmake_minimum_required(VERSION 3.14)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(patch
  DESCRIPTION "Patch library"
  LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(patch
  src/applier.cpp
  src/cmdline.cpp
  src/formatter.cpp
  src/locator.cpp
  src/parser.cpp
  src/patch.cpp
  src/system.cpp
)

target_include_directories(patch
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(patch::patch ALIAS patch)

install(TARGETS patch
  EXPORT patch-config
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(EXPORT patch-config
  NAMESPACE patch::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/patch
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/patch/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/patch
)

add_subdirectory(app)

option(BUILD_TESTING "Build the tests" OFF)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
