# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

cmake_minimum_required(VERSION 3.15)

project(patch_install_consumer LANGUAGES CXX)

find_package(patch CONFIG REQUIRED)

add_executable(patch_install_consumer main.cpp)
target_link_libraries(patch_install_consumer PRIVATE patch::patch)
