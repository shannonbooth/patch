// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <ostream>
#include <string>

namespace Patch {

enum class Format;
class File;
struct Options;
struct Patch;
struct PatchHeaderInfo;

std::string to_string(Format format);

enum class Default {
    True,
    False,
};

bool check_with_user(const std::string& question, std::ostream& out, Default default_response);

int process_patch(const Options& options);

} // namespace Patch
