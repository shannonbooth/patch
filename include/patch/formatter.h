// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

namespace Patch {

class File;
struct Hunk;
struct Patch;

void write_hunk_as_unified(const Hunk& hunk, File& out);
void write_hunk_as_context(const Hunk& hunk, File& out);
void write_patch_header_as_unified(const Patch& patch, File& out);
void write_patch_header_as_context(const Patch& patch, File& out);

} // namespace Patch
