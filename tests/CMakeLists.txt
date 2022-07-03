# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

include(FetchContent)
include(GoogleTest)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.0
  GIT_PROGRESS TRUE
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

macro(patch_add_test TESTNAME)
  add_executable(${TESTNAME} ${ARGN})
  target_link_libraries(${TESTNAME} gtest gmock gtest_main patch)
  gtest_discover_tests(${TESTNAME})
endmacro()

patch_add_test(test_patch
  test_applier.cpp
  test_cmdline.cpp
  test_defines.cpp
  test_determine_format.cpp
  test_formatter.cpp
  test_locator.cpp
  test_mutlipatches.cpp
  test_parser.cpp
  test_reject.cpp
  test_strip.cpp
  test_system.cpp
)

find_package(Python3 REQUIRED)
add_test(NAME Python.Full COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_full.py $<TARGET_FILE:sb_patch>)
