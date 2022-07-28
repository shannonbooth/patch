# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

execute_process(COMMAND ${PATCH}
  TIMEOUT 2
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if(NOT result EQUAL 2)
  message(FATAL_ERROR "${PATCH} returned ${result}")
endif()

if(NOT output STREQUAL "")
  message(FATAL_ERROR "${PATCH} returned ${output}")
endif()

if(NOT error STREQUAL "patch: **** out of memory\n")
  message(FATAL_ERROR "${PATCH} returned '${error}'")
endif()
