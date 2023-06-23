# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

include(CheckSymbolExists)

set(CMAKE_REQUIRED_LIBRARIES "util")
check_symbol_exists(forkpty "pty.h" HAVE_FORKPTY_PTY)
if(NOT HAVE_FORKPTY_PTY)
  check_symbol_exists(forkpty "util.h" HAVE_FORKPTY_UTIL)
endif()
unset(CMAKE_REQUIRED_LIBRARIES)

configure_file(lib/src/config.h.in lib/src/config.h)

add_library(patch_test lib/src/test.cpp)
target_link_libraries(patch_test PUBLIC patch)
target_include_directories(patch_test
  PUBLIC
    lib/include
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/lib/src
)

find_program(GNU_PATCH patch)
if(GNU_PATCH)
  execute_process(COMMAND ${GNU_PATCH} --version OUTPUT_VARIABLE patch_version)
  if(patch_version MATCHES "GNU patch 2.7.6")
    message(STATUS "Will test for compatibility with GNU patch 2.7.6")
  else()
    set(GNU_PATCH FALSE) # not of the correct version, remove it.
  endif()
endif()

function(read_test_names_for_file filename test_names)
  file(STRINGS ${filename} contents NEWLINE_CONSUME)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${filename})
  set(found_tests)
  # FIXME: there are a bunch of scenarios where this parsing may go wrong.
  string(REGEX MATCHALL "(PATCH_TEST|TEST)\\(([_a-zA-Z][_a-zA-Z0-9]*)\\)" tests "${contents}")
  foreach(test ${tests})
    string(REGEX MATCHALL "(PATCH_TEST|TEST)\\(([_a-zA-Z][_a-zA-Z0-9]*)\\)" dummy "${test}")
    set(test_name ${CMAKE_MATCH_2})
    list(APPEND found_tests ${test_name})
  endforeach()
  set(${test_names} ${found_tests} PARENT_SCOPE)
endfunction()

function(patch_add_tests target_name patch_target test_compat_gnu_patch)
  get_target_property(files ${target_name} SOURCES)
  foreach(file ${files})
    read_test_names_for_file(${file} tests)
    foreach(test ${tests})
      add_test(NAME ${test} COMMAND ${target_name} $<TARGET_FILE:${patch_target}> ${test})
      if(test_compat_gnu_patch AND GNU_PATCH)
        add_test(NAME compat.${test} COMMAND ${target_name} ${GNU_PATCH} "compat.${test}")
      endif()

      if (${test} MATCHES "^DISABLED_" AND ${CMAKE_VERSION} VERSION_GREATER "3.9")
        set_tests_properties("${test}" PROPERTIES DISABLED ON)
      endif()
    endforeach()
  endforeach()
endfunction()

if(WIN32)
  target_sources(patch_test PRIVATE lib/src/windows_process.cpp)
else()
  target_sources(patch_test PRIVATE lib/src/unix_process.cpp)
endif()

if(HAVE_FORKPTY_PTY OR HAVE_FORKPTY_UTIL)
  add_executable(test_pty test_pty.cpp test_prerequisite.cpp lib/src/pty_spawn.cpp)
  target_link_libraries(test_pty PRIVATE patch_test util)
  target_include_directories(test_pty PRIVATE lib/src ${CMAKE_CURRENT_BINARY_DIR}/lib/src)
  patch_add_tests(test_pty sb_patch ON)
endif()

add_executable(patch_oom
  stubs/stub_patch_oom.cpp
  ${PROJECT_SOURCE_DIR}/src/options.cpp
  ${PROJECT_SOURCE_DIR}/app/main.cpp
)
target_include_directories(patch_oom PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_oom test_oom.cpp)
target_link_libraries(test_oom PRIVATE patch_test)
patch_add_tests(test_oom patch_oom OFF)

add_executable(test_basic
  test_basic.cpp
  test_normal.cpp
  test_context.cpp
  test_defines.cpp
  test_applier.cpp
  test_quoting.cpp
)
target_link_libraries(test_basic PRIVATE patch_test)
patch_add_tests(test_basic sb_patch ON)

add_executable(test_unit
  test_cmdline.cpp
  test_determine_format.cpp
  test_file.cpp
  test_formatter.cpp
  test_locator.cpp
  test_misc.cpp
  test_mutlipatches.cpp
  test_newlines.cpp
  test_parser.cpp
  test_reject.cpp
  test_strip.cpp
)
target_link_libraries(test_unit PRIVATE patch_test)
patch_add_tests(test_unit sb_patch OFF)
