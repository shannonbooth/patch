// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/formatter.h>
#include <patch/hunk.h>

namespace Patch {

void write_hunk_as_unified(const Hunk& hunk, File& out)
{
    // Write hunk range
    out << "@@ -" << hunk.old_file_range.start_line;
    if (hunk.old_file_range.number_of_lines != 1)
        out << ',' << hunk.old_file_range.number_of_lines;
    out << " +" << hunk.new_file_range.start_line;
    if (hunk.new_file_range.number_of_lines != 1)
        out << ',' << hunk.new_file_range.number_of_lines;
    out << " @@\n";

    // Then body
    for (const auto& patch_line : hunk.lines) {
        out << patch_line.operation << patch_line.line.content << '\n';

        if (patch_line.line.newline == NewLine::None)
            out << "\\ No newline at end of file\n";
    }
}

static void write_hunk_as_context(const std::vector<PatchLine>& old_lines, const Range& old_range,
    const std::vector<PatchLine>& new_lines, const Range& new_range,
    File& out)
{
    out << "*** " << old_range.start_line;
    if (old_range.number_of_lines > 1)
        out << ',' << (old_range.start_line + old_range.number_of_lines - 1);
    out << " ****\n";

    if (!old_lines.empty()) {
        for (const auto& line : old_lines)
            out << line.operation << ' ' << line.line.content << '\n';

        if (old_lines.back().line.newline == NewLine::None)
            out << "\\ No newline at end of file\n";
    }

    out << "--- " << new_range.start_line;
    if (new_range.number_of_lines > 1)
        out << ',' << (new_range.start_line + new_range.number_of_lines - 1);
    out << " ----\n";

    if (!new_lines.empty()) {
        for (const auto& line : new_lines)
            out << line.operation << ' ' << line.line.content << '\n';

        if (new_lines.back().line.newline == NewLine::None)
            out << "\\ No newline at end of file\n";
    }
}

void write_hunk_as_context(const Hunk& hunk, File& out)
{
    size_t new_lines_last = 0;
    size_t old_lines_last = 0;
    std::vector<PatchLine> new_lines;
    std::vector<PatchLine> old_lines;

    char operation = ' ';
    bool is_all_insertions = true;
    bool is_all_deletions = true;

    // The previous line in the uni diff was not an <op>, which means
    // that both new and old lines were changed in the context diff
    // (instead of just being added or removed).
    auto make_change_command_on_operation = [&](char op) {
        if (operation != op) {
            operation = '!';
            for (size_t i = new_lines_last; i < new_lines.size(); ++i)
                new_lines[i].operation = '!';
            for (size_t i = old_lines_last; i < old_lines.size(); ++i)
                old_lines[i].operation = '!';
        }
    };

    for (const auto& patch_line : hunk.lines) {
        switch (patch_line.operation) {
        case ' ':
            if (old_lines.size() == static_cast<size_t>(hunk.old_file_range.number_of_lines))
                throw std::runtime_error("Corrupt patch, more old lines than expected");

            if (new_lines.size() == static_cast<size_t>(hunk.new_file_range.number_of_lines))
                throw std::runtime_error("Corrupt patch, more new lines than expected");

            operation = ' ';
            new_lines.emplace_back(' ', patch_line.line);
            old_lines.emplace_back(' ', patch_line.line);
            new_lines_last = new_lines.size();
            old_lines_last = old_lines.size();
            break;
        case '+':
            if (new_lines.size() == static_cast<size_t>(hunk.new_file_range.number_of_lines))
                throw std::runtime_error("Corrupt patch, more new lines than expected");

            if (operation != ' ')
                make_change_command_on_operation('+');
            else
                operation = '+';

            new_lines.emplace_back(operation, patch_line.line);
            is_all_deletions = false;
            break;
        case '-':
            if (old_lines.size() == static_cast<size_t>(hunk.old_file_range.number_of_lines))
                throw std::runtime_error("Corrupt patch, more old lines than expected");

            if (operation != ' ')
                make_change_command_on_operation('-');
            else
                operation = '-';

            old_lines.emplace_back(operation, patch_line.line);
            is_all_insertions = false;
            break;
        default:
            throw std::runtime_error("Invalid patch operation given");
        }
    }

    if (new_lines.size() != static_cast<size_t>(hunk.new_file_range.number_of_lines)
        && old_lines.size() != static_cast<size_t>(hunk.old_file_range.number_of_lines)) {
        throw std::runtime_error("Corrupt patch, expected number of lines not given");
    }

    if (is_all_insertions)
        write_hunk_as_context({}, hunk.old_file_range, new_lines, hunk.new_file_range, out);
    else if (is_all_deletions)
        write_hunk_as_context(old_lines, hunk.old_file_range, {}, hunk.new_file_range, out);
    else
        write_hunk_as_context(old_lines, hunk.old_file_range, new_lines, hunk.new_file_range, out);
}

void write_patch_header_as_unified(const Patch& patch, File& out)
{
    out << "--- " << patch.old_file_path;
    if (!patch.old_file_time.empty())
        out << '\t' << patch.old_file_time;
    out << '\n';

    out << "+++ " << patch.new_file_path;
    if (!patch.new_file_time.empty())
        out << '\t' << patch.new_file_time;
    out << '\n';
}

void write_patch_header_as_context(const Patch& patch, File& out)
{
    out << "*** " << patch.old_file_path;
    if (!patch.old_file_time.empty())
        out << '\t' << patch.old_file_time;
    out << '\n';

    out << "--- " << patch.new_file_path;
    if (!patch.new_file_time.empty())
        out << '\t' << patch.new_file_time;
    out << '\n';

    out << "***************\n";
}

} // namespace Patch
