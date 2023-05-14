// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <istream>
#include <limits>
#include <ostream>
#include <patch/applier.h>
#include <patch/file.h>
#include <patch/formatter.h>
#include <patch/hunk.h>
#include <patch/locator.h>
#include <patch/options.h>
#include <patch/patch.h>
#include <sstream>
#include <system_error>

namespace Patch {

class LineWriter {
public:
    LineWriter(File& file, const Options& options)
        : m_file(file)
        , m_options(options)
    {
    }

    LineWriter& operator<<(const Line& line)
    {
        m_file << line.content;
        *this << line.newline;
        return *this;
    }

    LineWriter& operator<<(const char* content)
    {
        m_file << content;
        return *this;
    }

    LineWriter& operator<<(const std::string& content)
    {
        m_file << content;
        return *this;
    }

    LineWriter& operator<<(NewLine newline)
    {
        if (newline == NewLine::None)
            return *this;

        if (m_options.newline_output == Options::NewlineOutput::Native
            || m_options.newline_output == Options::NewlineOutput::LF) {
            m_file << '\n';
            return *this;
        }

        if (m_options.newline_output == Options::NewlineOutput::CRLF) {
            m_file << "\r\n";
            return *this;
        }

        if (newline == NewLine::CRLF)
            m_file << "\r\n";
        else
            m_file << '\n';

        return *this;
    }

private:
    File& m_file;
    const Options& m_options;
};

static LineNumber write_define_hunk(LineWriter& output, const Hunk& hunk, const Location& location, const std::vector<Line>& lines, const std::string& define)
{
    enum class DefineState {
        Outside,
        InsideIFNDEF,
        InsideIFDEF,
        InsideELSE,
    };

    DefineState define_state = DefineState::Outside;
    auto line_number = static_cast<size_t>(location.line_number);

    for (const auto& patch_line : hunk.lines) {
        if (patch_line.operation == ' ') {
            const auto& line = lines.at(line_number);
            ++line_number;
            if (define_state != DefineState::Outside) {
                output << "#endif" << line.newline;
                define_state = DefineState::Outside;
            }
            output << line;
        } else if (patch_line.operation == '+') {
            if (define_state == DefineState::Outside) {
                define_state = DefineState::InsideIFDEF;
                output << "#ifdef " << define << patch_line.line.newline;
            } else if (define_state == DefineState::InsideIFNDEF) {
                define_state = DefineState::InsideELSE;
                output << "#else" << patch_line.line.newline;
            }
            output << patch_line.line;
        } else if (patch_line.operation == '-') {
            const auto& line = lines.at(line_number);
            ++line_number;

            if (define_state == DefineState::Outside) {
                define_state = DefineState::InsideIFNDEF;
                output << "#ifndef " << define << line.newline;
            } else if (define_state == DefineState::InsideIFDEF) {
                define_state = DefineState::InsideELSE;
                output << "#else" << line.newline;
            }
            output << line;
        }
    }

    if (define_state != DefineState::Outside)
        output << "#endif" << lines.at(lines.size() - 1).newline;

    return static_cast<LineNumber>(line_number);
}

static LineNumber write_hunk(LineWriter& output, const Hunk& hunk, const Location& location, const std::vector<Line>& lines, const std::string& define)
{
    if (!define.empty())
        return write_define_hunk(output, hunk, location, lines, define);

    auto line_number = static_cast<size_t>(location.line_number);

    for (const auto& patch_line : hunk.lines) {
        if (patch_line.operation == ' ') {
            output << lines.at(line_number);
            ++line_number;
        } else if (patch_line.operation == '+') {
            output << patch_line.line;
        } else if (patch_line.operation == '-') {
            ++line_number;
        }
    }

    return static_cast<LineNumber>(line_number);
}

static void print_hunk_statistics(std::ostream& out, size_t hunk_num, bool skipped, const Location& location, const Hunk& hunk, LineNumber offset_old_lines_to_new, LineNumber offset_error)
{
    out << "Hunk #" << hunk_num + 1;
    if (skipped)
        out << " skipped";
    else if (!location.is_found())
        out << " FAILED";
    else
        out << " succeeded";

    out << " at ";
    if (location.is_found()) {
        out << location.line_number + offset_old_lines_to_new + 1;
        if (location.fuzz != 0)
            out << " with fuzz " << location.fuzz;
        if (offset_error != 0) {
            out << " (offset " << offset_error << " line";
            if (offset_error > 1)
                out << "s";
            out << ")";
        }
        out << ".\n";
    } else {
        out << hunk.old_file_range.start_line + offset_old_lines_to_new << ".\n";
    }
}

static bool should_check_if_patch_is_reversed(const Location& location, const Options& options)
{
    // Hunk applied perfectly - there's no point.
    if (location.offset == 0 && location.fuzz == 0)
        return false;

    // Don't try to be smart, do what we are told.
    if (options.force)
        return false;

    // POSIX tells us to not check this if a patch is reversed if the reversed option
    // has been specified, but from testing on the command line against GNU patch, it
    // seems to check this anyway! For compatibility, lets follow this behaviour for now.

    return true;
}

enum class ReverseHandling {
    Reverse,
    Ignore,
    ApplyAnyway,
};

static ReverseHandling check_how_to_handle_reversed_patch(std::ostream& out, const Options& options)
{
    // We may have a reversed patch, tell the user and determine how to handle it.
    if (options.reverse_patch)
        out << "Unreversed";
    else
        out << "Reversed (or previously applied)";

    out << " patch detected!  ";

    // Check whether we've been told to ignore this on the command line.
    if (!options.ignore_reversed) {
        // Or if we have been told to assume patches have been reversed.
        if (options.batch) {
            out << "Assuming -R.\n";
            return ReverseHandling::Reverse;
        }

        // Otherwise we need to the user whether we should reverse it.
        if (check_with_user("Assume -R?", out, Default::False))
            return ReverseHandling::Reverse;

        if (check_with_user("Apply anyway?", out, Default::False))
            return ReverseHandling::ApplyAnyway;
    }

    out << "Skipping patch.\n";
    return ReverseHandling::Ignore;
}

void RejectWriter::write_reject_file(const Hunk& hunk)
{
    if (should_write_as_unified()) {
        if (m_rejected_hunks == 0)
            write_patch_header_as_unified(m_patch, m_reject_file);
        write_hunk_as_unified(hunk, m_reject_file);
    } else {
        if (m_rejected_hunks == 0)
            write_patch_header_as_context(m_patch, m_reject_file);
        write_hunk_as_context(hunk, m_reject_file);
    }
    ++m_rejected_hunks;
}

bool RejectWriter::should_write_as_unified() const
{
    // POSIX says that all reject files must be written in context
    // format. However, unified diffs are much more popular than
    // context diff these days, so we write unified diffs in unified
    // format to avoid confusion.
    return m_reject_format == Options::RejectFormat::Unified
        || (m_reject_format == Options::RejectFormat::Default && m_patch.format == Format::Unified);
}

Result apply_patch(File& out_file, RejectWriter& reject_writer, const std::vector<Line>& lines, Patch& patch, const Options& options, std::ostream& out)
{
    if (options.reverse_patch)
        reverse(patch);

    LineWriter output(out_file, options);
    LineNumber line_number = 0; // NOTE: relative to 'old' file.
    LineNumber offset_old_lines_to_new = 0;
    LineNumber offset_error = 0;

    bool skip_remaining_hunks = false;
    bool all_hunks_applied_perfectly = true;

    for (size_t hunk_num = 0; hunk_num < patch.hunks.size(); ++hunk_num) {
        auto& hunk = patch.hunks[hunk_num];

        auto location = locate_hunk(lines, hunk, options.ignore_whitespace, offset_error, options.max_fuzz);

        // POSIX specifies that until a hunk successfully applies, patch should check if the patch given is reversed.
        if (hunk_num == 0 && should_check_if_patch_is_reversed(location, options)) {
            // The first hunk is not applying perfectly. We need to verify whether it looks reversed.
            reverse(hunk);
            auto reversed_location = locate_hunk(lines, hunk, options.ignore_whitespace, offset_error, options.max_fuzz);

            // Consider the patch potentially reversed if:
            //  * The reversed hunk applied perfectly.
            //  * The non-reversed hunk could not be applied, but the reversed one can.
            // If either of these is true, check with the user how to handle this.
            auto reverse_handling = ReverseHandling::ApplyAnyway;
            if ((reversed_location.offset == 0 && reversed_location.fuzz == 0) || (!location.is_found() && reversed_location.is_found()))
                reverse_handling = check_how_to_handle_reversed_patch(out, options);

            switch (reverse_handling) {
            case ReverseHandling::Reverse:
                // Reverse the remainder of our hunks, and then apply those.
                for (size_t hunk_to_reverse = 1; hunk_to_reverse < patch.hunks.size(); ++hunk_to_reverse)
                    reverse(patch.hunks[hunk_to_reverse]);
                location = reversed_location;
                break;
            case ReverseHandling::Ignore:
                skip_remaining_hunks = true;
                reverse(hunk);
                break;
            case ReverseHandling::ApplyAnyway:
                // Undo our attempt to reverse the hunk,
                reverse(hunk);
                break;
            }
        }

        if (!skip_remaining_hunks && location.is_found()) {
            offset_error += location.offset;

            // Write up until where we have found this latest hunk from the old file.
            for (; line_number < location.line_number; ++line_number)
                output << lines.at(static_cast<size_t>(line_number));

            // Then output the hunk to what we hope is the correct location in the file.
            line_number = write_hunk(output, hunk, location, lines, options.define_macro);
        } else {
            // The hunk has failed to reply. We now need to write the hunk to the reject file.
            // Per POSIX, ensure offset relative to new file rather than old file.
            hunk.new_file_range.start_line += offset_old_lines_to_new;
            hunk.old_file_range.start_line += offset_old_lines_to_new;
            reject_writer.write_reject_file(hunk);
        }

        const bool hunk_applied_perfectly = location.fuzz == 0 && location.offset == 0;

        if (!hunk_applied_perfectly)
            all_hunks_applied_perfectly = false;

        if (options.verbose || (!hunk_applied_perfectly && !skip_remaining_hunks))
            print_hunk_statistics(out, hunk_num, skip_remaining_hunks, location, hunk, offset_old_lines_to_new, offset_error);

        if (location.is_found())
            offset_old_lines_to_new += hunk.new_file_range.number_of_lines - hunk.old_file_range.number_of_lines;
    }

    // We've finished applying all hunks, write out anything from the old file we haven't already.
    for (; static_cast<size_t>(line_number) < lines.size(); ++line_number)
        output << lines[static_cast<size_t>(line_number)];

    return { reject_writer.rejected_hunks(), skip_remaining_hunks, all_hunks_applied_perfectly };
}

void reverse(Patch& patch)
{
    std::swap(patch.old_file_path, patch.new_file_path);
    std::swap(patch.old_file_time, patch.new_file_time);
    std::swap(patch.old_file_mode, patch.new_file_mode);
    for (auto& hunk : patch.hunks)
        reverse(hunk);
}

void reverse(Hunk& hunk)
{
    std::swap(hunk.old_file_range, hunk.new_file_range);
    for (auto& line : hunk.lines) {
        if (line.operation == '+')
            line.operation = '-';
        else if (line.operation == '-')
            line.operation = '+';
    }
}

} // namespace Patch
