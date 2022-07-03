// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <patch/patch.h>
#include <patch/hunk.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Patch {

struct Hunk;

struct Location {
    Location() = default;
    Location(LineNumber l, LineNumber f, LineNumber o)
        : line_number(l)
        , fuzz(f)
        , offset(o)
    {
    }

    bool is_found() const { return line_number != -1; }

    LineNumber line_number { -1 };
    LineNumber fuzz { -1 };
    LineNumber offset { -1 };
};

Location locate_hunk(const std::vector<Line>& content, const Hunk& hunk, bool ignore_whitespace = false, LineNumber offset = 0, LineNumber max_fuzz = 2);

bool matches_ignoring_whitespace(const std::string& as, const std::string& bs);

bool matches(const Line& line1, const Line& line2, bool ignore_whitespace);

} // namespace Patch
