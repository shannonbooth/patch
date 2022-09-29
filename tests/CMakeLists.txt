# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

include(FetchContent)
include(GoogleTest)
include(CheckSymbolExists)

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
  test_file.cpp
)

set(CMAKE_REQUIRED_LIBRARIES "util")
check_symbol_exists(forkpty "pty.h" HAVE_FORKPTY_PTY)
check_symbol_exists(forkpty "util.h" HAVE_FORKPTY_UTIL)
unset(CMAKE_REQUIRED_LIBRARIES)

configure_file(src/config.h.in src/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

if(HAVE_FORKPTY_PTY OR HAVE_FORKPTY_UTIL)
  add_executable(test_pty test_pty.cpp src/pty_spawn.cpp)
  target_link_libraries(test_pty PRIVATE patch util)
  target_include_directories(test_pty PRIVATE src)
  add_test(NAME Patch.Pty COMMAND test_pty $<TARGET_FILE:sb_patch>)
endif()

add_executable(patch_oom
  stubs/stub_patch_oom.cpp
  ${PROJECT_SOURCE_DIR}/app/main.cpp
)
target_include_directories(patch_oom PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_test(NAME Patch.Oom COMMAND ${CMAKE_COMMAND} -DPATCH=$<TARGET_FILE:patch_oom> -P "${CMAKE_CURRENT_SOURCE_DIR}/test_oom.cmake")

find_package(Python3 REQUIRED)
add_test(NAME Python.Full COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_full.py $<TARGET_FILE:sb_patch>)
