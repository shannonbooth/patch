// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <patch/file.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Patch {

using LineNumber = int64_t;

struct Range {
    LineNumber start_line { -1 };
    LineNumber number_of_lines { -1 };
};

enum class Format {
    Context,
    Unified,
    Git,
    Ed,
    Normal,
    Unknown,
};

struct Line {
    Line() = default;

    Line(const std::string& content_, NewLine newline_)
        : content(content_)
        , newline(newline_)
    {
    }

    Line(std::string&& content_, NewLine newline_)
        : content(std::move(content_))
        , newline(newline_)
    {
    }

    std::string content;
    NewLine newline { NewLine::LF };
};

struct PatchLine {
    PatchLine(char op, const Line& l)
        : operation(op)
        , line(l)
    {
    }

    PatchLine(char op, Line&& l)
        : operation(op)
        , line(std::move(l))
    {
    }

    PatchLine(char op, const std::string& l)
        : operation(op)
        , line(l, NewLine::LF)
    {
    }

    PatchLine(char op, std::string&& l)
        : operation(op)
        , line(std::move(l), NewLine::LF)
    {
    }

    char operation;
    Line line;
};

struct Hunk {
    Range old_file_range;
    Range new_file_range;
    std::vector<PatchLine> lines;
};

enum class Operation {
    Change,
    Rename,
    Copy,
    Delete,
    Add,
    Binary,
};

struct Patch {
    Patch() = default;

    explicit Patch(Format format_)
        : format(format_)
    {
    }

    Format format { Format::Unknown };
    Operation operation { Operation::Change };

    std::string index_file_path;
    std::string prerequisite;

    std::string old_file_path;
    std::string new_file_path;

    // NOTE: We currently do not do anything with this timestamp (yet). However,
    //       we still keep this information for use in reject file output. In
    //       the future we should parse this information as a proper timestamp.
    std::string old_file_time;
    std::string new_file_time;

    uint16_t old_file_mode { 0 };
    uint16_t new_file_mode { 0 };

    std::vector<Hunk> hunks;
};

} // namespace Patch
