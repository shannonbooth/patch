# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

function(run_checked)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Command failed (${result}): ${ARGN}\n${output}${error}")
  endif()
endfunction()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")

set(generator_arguments)
if(TEST_GENERATOR)
  list(APPEND generator_arguments -G "${TEST_GENERATOR}")
endif()
if(TEST_GENERATOR_PLATFORM)
  list(APPEND generator_arguments -A "${TEST_GENERATOR_PLATFORM}")
endif()
if(TEST_GENERATOR_TOOLSET)
  list(APPEND generator_arguments -T "${TEST_GENERATOR_TOOLSET}")
endif()

set(project_build_dir "${TEST_BINARY_DIR}/project")
set(install_dir "${TEST_BINARY_DIR}/install")
run_checked(
  "${CMAKE_COMMAND}"
  -S "${TEST_SOURCE_DIR}"
  -B "${project_build_dir}"
  ${generator_arguments}
  "-DBUILD_TESTING=OFF"
  "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
  "-DCMAKE_INSTALL_PREFIX=${install_dir}"
  "-DCMAKE_INSTALL_LIBDIR=lib64"
  "-DCMAKE_INSTALL_INCLUDEDIR=headers"
)

set(build_arguments --build "${project_build_dir}")
if(TEST_CONFIG)
  list(APPEND build_arguments --config "${TEST_CONFIG}")
endif()
run_checked("${CMAKE_COMMAND}" ${build_arguments})

set(install_arguments --install "${project_build_dir}")
if(TEST_CONFIG)
  list(APPEND install_arguments --config "${TEST_CONFIG}")
endif()
run_checked("${CMAKE_COMMAND}" ${install_arguments})

set(consumer_build_dir "${TEST_BINARY_DIR}/consumer")
run_checked(
  "${CMAKE_COMMAND}"
  -S "${CMAKE_CURRENT_LIST_DIR}/install_consumer"
  -B "${consumer_build_dir}"
  ${generator_arguments}
  "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
  "-Dpatch_DIR=${install_dir}/lib64/cmake/patch"
)

set(consumer_build_arguments --build "${consumer_build_dir}")
if(TEST_CONFIG)
  list(APPEND consumer_build_arguments --config "${TEST_CONFIG}")
endif()
run_checked("${CMAKE_COMMAND}" ${consumer_build_arguments})
