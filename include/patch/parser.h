// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <cstdint>
#include <istream>
#include <patch/hunk.h>
#include <string>
#include <vector>

namespace Patch {

bool parse_unified_range(Hunk& hunk, const std::string& line);
bool parse_normal_range(Hunk& hunk, const std::string& line);

struct PatchHeaderInfo {
    std::istream::pos_type patch_start;
    size_t lines_till_first_hunk { 0 };
    Format format { Format::Unknown };
};

bool parse_patch_header(Patch& patch, std::istream& file, PatchHeaderInfo& header_info, int strip = -1);

void parse_patch_body(Patch& patch, std::istream& file);

Patch parse_patch(std::istream& file, Format format = Format::Unknown, int strip = -1);

Patch parse_context_patch(Patch& patch, std::istream& file);
Patch parse_unified_patch(Patch& patch, std::istream& file);
Patch parse_normal_patch(Patch& patch, std::istream& file);

void parse_file_line(const std::string& input, int strip, std::string& path, std::string* timestamp = nullptr);

std::string strip_path(const std::string& path, int amount);
std::string parse_path(const std::string& input, int strip);

Hunk hunk_from_context_parts(LineNumber old_start_line, const std::vector<PatchLine>& old_lines, LineNumber new_start_line, const std::vector<PatchLine>& new_lines);

bool string_to_line_number(const std::string& str, LineNumber& output);

} // namespace Patch
