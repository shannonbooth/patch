# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

function(add_coverage_flags)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage" PARENT_SCOPE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage" PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage" PARENT_SCOPE)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fno-elide-constructors -fprofile-instr-generate -fcoverage-mapping -fprofile-arcs" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage -fprofile-instr-generate -fcoverage-mapping -fprofile-arcs" PARENT_SCOPE)
  endif()
endfunction()

function(add_coverage_target)
  find_program(GCOVR gcovr)
  if(NOT GCOVR)
    message(FATAL_ERROR "gcovr not found, unable to run code coverage!")
  endif()

  set(EXCLUDES --exclude ${PROJECT_SOURCE_DIR}/tests --exclude ${PROJECT_BINARY_DIR}/CMakeFiles)
  add_custom_target(coverage
    COMMAND ${GCOVR} --exclude-unreachable-branches --exclude-throw-branches --xml coverage.xml -r ${PROJECT_SOURCE_DIR} ${EXCLUDES}
    COMMAND ${CMAKE_COMMAND} -E make_directory coverage
    COMMAND ${GCOVR} --exclude-unreachable-branches --exclude-throw-branches --html coverage/index.html --html-details -r ${PROJECT_SOURCE_DIR} ${EXCLUDES}
    BYPRODUCTS coverage.xml coverage/index.html
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    VERBATIM
  )
endfunction()
