// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <ostream>
#include <string>

namespace Patch {

enum class Format;
struct Options;
struct Patch;
struct PatchHeaderInfo;

std::string to_string(Format format);

void print_header_info(std::istream& patch, PatchHeaderInfo& header_info, std::ostream& out);

enum class Default {
    True,
    False,
};

bool check_with_user(const std::string& question, Default default_response);

int process_patch(const Options& options);

} //namespace Patch
