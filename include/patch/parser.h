// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <cstdint>
#include <istream>
#include <patch/file.h>
#include <patch/hunk.h>
#include <string>
#include <vector>

namespace Patch {

struct PatchHeaderInfo {
    fpos_t patch_start {};
    size_t lines_till_first_hunk { 0 };
    Format format { Format::Unknown };
};

class Parser {
public:
    explicit Parser(File& file);

    bool parse_patch_header(Patch& patch, PatchHeaderInfo& header_info, int strip = -1);
    void parse_patch_body(Patch& patch);

    size_t line_number() const { return m_line_number; }

    void print_header_info(const PatchHeaderInfo& header_info, std::ostream& out);

    bool is_eof() const { return m_file.eof(); }

private:
    bool get_line(std::string& line, NewLine* newline = nullptr);

    Patch parse_context_patch(Patch& patch);
    Patch parse_unified_patch(Patch& patch);
    Patch parse_normal_patch(Patch& patch);
    void parse_context_hunk(std::vector<PatchLine>& old_lines, LineNumber& old_start_line, std::vector<PatchLine>& new_lines, LineNumber& new_start_line);

    size_t m_line_number { 1 };
    File& m_file;
};

class LineParser {
public:
    explicit LineParser(const std::string& line)
        : m_current(line.cbegin())
        , m_end(line.cend())
    {
    }

    bool is_eof() const;

    char peek() const;

    char consume();

    bool consume_specific(char c);

    bool consume_specific(const char* chars);

    bool consume_uint();

    bool consume_line_number(LineNumber& output);

    std::string parse_quoted_string();

    void parse_file_line(int strip, std::string& path, std::string* timestamp = nullptr);

    void parse_git_header_name(Patch& patch, int strip);

    bool parse_git_extended_info(Patch& patch, int strip);

private:
    std::string::const_iterator m_current;
    std::string::const_iterator m_end;
};

Patch parse_patch(File& file, Format format = Format::Unknown, int strip = -1);

bool parse_unified_range(Hunk& hunk, const std::string& line);
bool parse_normal_range(Hunk& hunk, const std::string& line);

std::string strip_path(const std::string& path, int amount);
std::string parse_path(const std::string& input, int strip);

bool string_to_line_number(const std::string& str, LineNumber& output);

} // namespace Patch
