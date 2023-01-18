// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <patch/hunk.h>
#include <patch/locator.h>
#include <patch/utils.h>

namespace Patch {

bool matches_ignoring_whitespace(const std::string& as, const std::string& bs)
{
    auto a = as.begin();
    auto b = bs.begin();

    while (true) {
        if (b == bs.end() || is_whitespace(*b)) {
            // Strip leading whitespace off the second line (if any)
            while (b != bs.end() && is_whitespace(*b))
                ++b;

            // Now that we've stripped all leading whitespace off the second line,
            // strip all leading whitespace for the first line so that each
            // line should both be beginning with a non-whitespace character or
            // alternatively be at the end of the line.
            if (a != as.end()) {
                if (!is_whitespace(*a))
                    return false;

                do {
                    a++;
                } while (a != as.end() && is_whitespace(*a));
            }

            // Whitespace has been stripped off the beginning of both lines.
            // If either of the lines are at the end, the other line should
            // also be at the end! Any trailing whitespace should be trimmed
            // resulting in us reaching the end of the line by here.
            if (a == as.end())
                return b == bs.end();
            if (b == bs.end())
                return a == as.end();

            continue;
        }

        // We know from the above check that 'b' was not at the end of the line.
        // Therefore - 'a' must also not be at the end of a line for the two
        // lines to be considered equal.
        if (a == as.end())
            return false;

        // Whitespace is now normalized - we can (finally) perform the actual check!
        if (*a != *b)
            return false;

        // Both characters are equal! Proceed to the next char for both lines.
        ++a;
        ++b;
    }
}

bool matches(const Line& line1, const Line& line2, bool ignore_whitespace)
{
    bool newline_match = line1.newline == line2.newline;
    bool content_match = line1.content == line2.content;

    // Happy path - a perfect match
    if (newline_match && content_match)
        return true;

    // Not an exact match - check if we need to try see if ignoring whitespace will help.
    if (!ignore_whitespace)
        return false;

    // Fast path - content matches but newlines do not.
    if (content_match)
        return true;

    return matches_ignoring_whitespace(line1.content, line2.content);
}

static LineNumber expected_line_number(const Hunk& hunk)
{
    auto line = hunk.old_file_range.start_line;
    if (hunk.old_file_range.number_of_lines == 0)
        ++line;
    return line;
}

Location locate_hunk(const std::vector<Line>& content, const Hunk& hunk, bool ignore_whitespace, LineNumber offset, LineNumber max_fuzz)
{
    // Make a first best guess at where the from-file range is telling us where the hunk should be.
    LineNumber offset_guess = expected_line_number(hunk) - 1 + offset;

    LineNumber patch_prefix_content = 0;
    for (const auto& line : hunk.lines) {
        if (line.operation != ' ')
            break;
        ++patch_prefix_content;
    }

    LineNumber patch_suffix_content = 0;
    for (auto it = hunk.lines.rbegin(); it != hunk.lines.rend(); ++it) {
        if (it->operation != ' ')
            break;
        ++patch_suffix_content;
    }

    LineNumber context = std::max(patch_prefix_content, patch_suffix_content);

    // If there's no lines surrounding this hunk - it will always succeed,
    // so there is no point in checking any further. Note that this check is
    // also what makes matching against an empty 'from file' work (with no lines),
    // as in that case there is no content for us to even match against in the
    // first place!
    if (hunk.old_file_range.number_of_lines == 0)
        return { offset_guess, 0, 0 };

    for (LineNumber fuzz = 0; fuzz <= max_fuzz; ++fuzz) {

        auto suffix_fuzz = std::max<LineNumber>(fuzz + patch_suffix_content - context, 0);
        auto prefix_fuzz = std::max<LineNumber>(fuzz + patch_prefix_content - context, 0);

        // If the fuzz is greater than the total number of lines for a hunk,
        // then it may be possible for the hunk to match anything - so ignore
        // this case.
        if (static_cast<size_t>(suffix_fuzz) + static_cast<size_t>(prefix_fuzz) >= hunk.lines.size())
            return {};

        auto hunk_matches_starting_from_line = [&](LineNumber line) {
            line += prefix_fuzz;

            // Ensure that all of the lines in the hunk match starting from 'line'
            return std::all_of(hunk.lines.begin() + prefix_fuzz, hunk.lines.end() - suffix_fuzz, [&](const PatchLine& hunk_line) {
                // Ignore additions in our increment of line and
                // comparison as they are not part of the 'original file'
                if (hunk_line.operation == '+')
                    return true;

                if (static_cast<size_t>(line) >= content.size())
                    return false;

                // Check whether this line matches what is specified in this part of the hunk.
                if (!matches(content[static_cast<size_t>(line)], hunk_line.line, ignore_whitespace))
                    return false;

                // Proceed to the next line.
                ++line;
                return true;
            });
        };

        // First look for the hunk in the forward direction
        for (LineNumber line = offset_guess; static_cast<size_t>(line) < content.size(); ++line) {
            if (hunk_matches_starting_from_line(line))
                return { line, fuzz, line - offset_guess };
        }

        // Then look for it in the negative direction
        for (LineNumber line = offset_guess - 1; line >= 0; --line) {
            if (hunk_matches_starting_from_line(line))
                return { line, fuzz, line - offset_guess };
        }
    }

    // No bueno.
    return {};
}

bool has_prerequisite(const Line& line, const std::string& prerequisite)
{
    return line.content.find(prerequisite) != std::string::npos;
}

bool has_prerequisite(const std::vector<Line>& lines, const std::string& prerequisite)
{
    return std::any_of(lines.begin(), lines.end(), [&prerequisite](const Line& line) {
        return has_prerequisite(line, prerequisite);
    });
}

} // namespace Patch
