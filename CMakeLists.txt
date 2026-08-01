# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2026 Shannon Booth <shannon.ml.booth@gmail.com>

cmake_minimum_required(VERSION 3.15)
cmake_policy(VERSION 3.15)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(patch
  DESCRIPTION "Patch library"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(coverage)

option(PATCH_ENABLE_COVERAGE "Build with gcov support" OFF)
option(PATCH_WARNINGS_AS_ERRORS "Enable strict compiler warnings and treat them as errors" OFF)
option(BUILD_TESTING "Build the tests" OFF)

function(patch_enable_warnings target_name)
  if(NOT PATCH_WARNINGS_AS_ERRORS)
    return()
  endif()

  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /WX)
    target_compile_definitions(${target_name} PRIVATE
      _CRT_NONSTDC_NO_WARNINGS
      _CRT_SECURE_NO_WARNINGS
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Werror
    )
  else()
    message(FATAL_ERROR "PATCH_WARNINGS_AS_ERRORS does not support ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()

if(PATCH_ENABLE_COVERAGE)
  add_coverage_flags()
endif()

add_library(patch
  src/applier.cpp
  src/cmdline.cpp
  src/formatter.cpp
  src/locator.cpp
  src/options.cpp
  src/parser.cpp
  src/patch.cpp
  src/system.cpp
  src/file.cpp
)
patch_enable_warnings(patch)

target_compile_features(patch PUBLIC cxx_std_11)

target_include_directories(patch
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(patch::patch ALIAS patch)

install(TARGETS patch
  EXPORT patch-config
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT patch-config
  NAMESPACE patch::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/patch
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/patch/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/patch
)

add_subdirectory(app)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
  if(PATCH_ENABLE_COVERAGE)
    add_coverage_target()
  endif()
endif()
