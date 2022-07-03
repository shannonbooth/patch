// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <ostream>

namespace Patch {

struct Hunk;
struct Patch;

void write_hunk_as_unified(const Hunk& hunk, std::ostream& out);
void write_hunk_as_context(const Hunk& hunk, std::ostream& out);
void write_patch_header_as_unified(const Patch& patch, std::ostream& out);
void write_patch_header_as_context(const Patch& patch, std::ostream& out);

} // namespace Patch
