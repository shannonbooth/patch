# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2026 Shannon Booth <shannon.ml.booth@gmail.com>

include(CheckSymbolExists)

set(CMAKE_REQUIRED_LIBRARIES "util")
check_symbol_exists(forkpty "pty.h" HAVE_FORKPTY_PTY)
if(NOT HAVE_FORKPTY_PTY)
  check_symbol_exists(forkpty "util.h" HAVE_FORKPTY_UTIL)
endif()
unset(CMAKE_REQUIRED_LIBRARIES)

configure_file(lib/src/config.h.in lib/src/config.h)

add_library(patch_test lib/src/test.cpp)
patch_enable_warnings(patch_test)
target_link_libraries(patch_test PUBLIC patch)
target_include_directories(patch_test
  PUBLIC
    lib/include
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/lib/src
    ${PROJECT_SOURCE_DIR}/src
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

function(read_test_names_for_file filename test_names compat_test_names)
  file(STRINGS ${filename} contents NEWLINE_CONSUME)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${filename})
  set(found_tests)
  set(found_compat_tests)
  # FIXME: there are a bunch of scenarios where this parsing may go wrong.
  string(REGEX MATCHALL "(COMPAT_TEST|PATCH_TEST|TEST)\\(([_a-zA-Z][_a-zA-Z0-9]*)\\)" tests "${contents}")
  foreach(test ${tests})
    string(REGEX MATCH "(COMPAT_TEST|PATCH_TEST|TEST)\\(([_a-zA-Z][_a-zA-Z0-9]*)\\)" dummy "${test}")
    set(test_type ${CMAKE_MATCH_1})
    set(test_name ${CMAKE_MATCH_2})
    list(APPEND found_tests ${test_name})
    if(test_type STREQUAL "COMPAT_TEST")
      list(APPEND found_compat_tests ${test_name})
    endif()
  endforeach()
  set(${test_names} ${found_tests} PARENT_SCOPE)
  set(${compat_test_names} ${found_compat_tests} PARENT_SCOPE)
endfunction()

function(patch_add_tests target_name patch_target)
  get_target_property(files ${target_name} SOURCES)
  foreach(file ${files})
    read_test_names_for_file(${file} tests compat_tests)
    foreach(test ${tests})
      add_test(NAME ${test} COMMAND ${target_name} $<TARGET_FILE:${patch_target}> ${test})
      # A test exits with a magic number 77 to report that it skipped itself.
      set_tests_properties("${test}" PROPERTIES SKIP_RETURN_CODE 77)
      if("${test}" IN_LIST compat_tests AND GNU_PATCH)
        add_test(NAME compat.${test} COMMAND ${target_name} ${GNU_PATCH} "compat.${test}")
        set_tests_properties("compat.${test}" PROPERTIES SKIP_RETURN_CODE 77)
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

add_executable(patch_oom
  stubs/stub_patch_oom.cpp
  ${PROJECT_SOURCE_DIR}/src/options.cpp
  ${PROJECT_SOURCE_DIR}/app/main.cpp
)
patch_enable_warnings(patch_oom)
target_include_directories(patch_oom PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_oom test_oom.cpp)
patch_enable_warnings(test_oom)
target_link_libraries(test_oom PRIVATE patch_test)
patch_add_tests(test_oom patch_oom)

add_executable(patch_tests
  test_basic.cpp
  test_normal.cpp
  test_context.cpp
  test_defines.cpp
  test_applier.cpp
  test_quoting.cpp
  test_cmdline.cpp
  test_determine_format.cpp
  test_ed.cpp
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
patch_enable_warnings(patch_tests)
target_link_libraries(patch_tests PRIVATE patch_test)

if(HAVE_FORKPTY_PTY OR HAVE_FORKPTY_UTIL)
  target_sources(patch_tests PRIVATE
    test_pty.cpp
    test_prerequisite.cpp
    lib/src/pty_spawn.cpp
  )
  target_link_libraries(patch_tests PRIVATE util)
  target_include_directories(patch_tests PRIVATE lib/src ${CMAKE_CURRENT_BINARY_DIR}/lib/src)
endif()

patch_add_tests(patch_tests sb_patch)

add_test(NAME cmake.install_consumer
  COMMAND ${CMAKE_COMMAND}
    "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/install-consumer-$<CONFIG>"
    "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
    "-DTEST_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DTEST_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DTEST_CONFIG=$<CONFIG>"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_install_consumer.cmake
)
