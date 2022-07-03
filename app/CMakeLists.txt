# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

add_executable(sb_patch main.cpp)
target_link_libraries(sb_patch PRIVATE patch::patch)

install(TARGETS sb_patch
  RUNTIME
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
