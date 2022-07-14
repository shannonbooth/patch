// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <iostream>
#include <istream>
#include <patch/options.h>
#include <vector>

namespace Patch {

struct Patch;
struct Hunk;

struct Result {
    int failed_hunks;
    bool was_skipped;
    bool all_hunks_applied_perfectly;
};

Result apply_patch(std::ostream& out_file, std::ostream& reject_file, std::iostream& input_file, Patch& patch, const Options& options = {}, std::ostream& out = std::cout);

void reverse(Patch& patch);

void reverse(Hunk& hunk);

} // namespace Patch
