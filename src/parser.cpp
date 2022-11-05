// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cassert>
#include <limits>
#include <patch/hunk.h>
#include <patch/parser.h>
#include <patch/system.h>
#include <patch/utils.h>
#include <stdexcept>

namespace Patch {

class Parser {
public:
    explicit Parser(const std::string& line)
        : m_current(line.cbegin())
        , m_end(line.cend())
        , m_line(line)
    {
    }

    bool is_eof() const
    {
        return m_current == m_end;
    }

    char peek() const
    {
        if (m_current == m_end)
            return '\0';
        return *m_current;
    }

    char consume()
    {
        char c = *m_current;
        ++m_current;
        return c;
    }

    bool consume_specific(char c)
    {
        if (m_current == m_end)
            return false;
        if (*m_current != c)
            return false;

        ++m_current;
        return true;
    }

    bool consume_specific(const char* chars)
    {
        auto current = m_current;
        for (const char* c = chars; *c != '\0'; ++c) {
            if (!consume_specific(*c)) {
                m_current = current;
                return false;
            }
        }
        return true;
    }

    bool consume_uint()
    {
        if (m_current == m_end)
            return false;
        if (!is_digit(*m_current))
            return false;

        // Skip past this digit we just checked, and any remaining parts of the integer.
        ++m_current;
        m_current = std::find_if(m_current, m_end, is_not_digit);
        return true;
    }

    bool consume_line_number(LineNumber& output)
    {
        auto start = m_current;
        if (!consume_uint())
            return false;

        return string_to_line_number(std::string(start, m_current), output);
    }

    std::string::const_iterator current() const { return m_current; }
    std::string::const_iterator end() const { return m_end; }

    std::string parse_quoted_string()
    {
        std::string output;

        consume_specific('"');

        while (!is_eof()) {
            // Reached the end of the string
            if (peek() == '"')
                return output;

            // Some escaped character, peek past to determine the intended unescaped character.
            if (consume_specific('\\')) {
                if (is_eof())
                    throw std::invalid_argument("Invalid unterminated \\ in quoted path " + m_line);

                char c = consume();
                switch (c) {
                case '\\':
                    output += '\\';
                    break;
                case '"':
                    output += '"';
                    break;
                case 'n':
                    output += '\n';
                    break;
                case 't':
                    output += '\t';
                    break;
                // Octal encoding possibilities
                // Must be followed by 1, 2, or 3 octal digits '0' - '7' (inclusive)
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7': {
                    unsigned char result = c - '0';

                    for (int i = 1; i < 3; ++i) {
                        char octal_val = peek();
                        if (!is_octal(octal_val))
                            break;

                        uint8_t digit_val = octal_val - '0';
                        result = result * 8 + digit_val;

                        ++m_current;
                    }

                    output += static_cast<char>(result);
                    break;
                }
                default:
                    throw std::invalid_argument("Invalid or unsupported escape character in path " + m_line);
                }
            } else {
                // Normal case - a character of the path we can just add to our output.
                output += consume();
            }
        }

        throw std::invalid_argument("Failed to find terminating \" when parsing " + m_line);
    }

private:
    std::string::const_iterator m_current;
    std::string::const_iterator m_end;
    const std::string& m_line;
};

void parse_file_line(const std::string& input, int strip, std::string& path, std::string* timestamp)
{
    if (input.empty()) {
        path.clear();
        if (timestamp)
            timestamp->clear();
        return;
    }

    auto it = input.begin();

    if (*it == '"') {
        Parser parser(input);
        path = parser.parse_quoted_string();
        it = parser.current();
    } else {
        // In most patches, a \t is used to separate the path from
        // the timestamp. However, POSIX does not seem to specify one
        // way or another.
        //
        // GNU diff seems to always quote paths with whitespace in them,
        // however git does not. To implement this, if we find any tab
        // that is considered the end of the path. Otherwise if no tab
        // is found, we fall back on using a space.
        while (true) {
            if (it == input.end()) {
                path = input;
                break;
            }

            // Must have reached the end of the path.
            if (*it == '\t') {
                path = std::string(input.begin(), it);
                break;
            }

            // May have reached end of the path, but it also could be
            // a path with spaces. Look for a tab to find out.
            if (*it == ' ') {
                auto new_it = std::find(it, input.end(), '\t');
                if (new_it == input.end()) {
                    path = std::string(input.begin(), it);
                } else {
                    it = new_it;
                    path = std::string(input.begin(), new_it);
                }
                break;
            }

            ++it;
        }
    }

    // Anything after the path is considered the timestamp.
    // Currently this may also include whitespace! (which depends on
    // how the path and timestamp were separated in the patch).
    if (timestamp && it != input.end() && it + 1 != input.end())
        *timestamp = std::string(it + 1, input.end());

    // We don't want /dev/null to become stripped, as this is a magic
    // name which we use to determine whether a file has been deleted
    // or added.
    if (path != "/dev/null")
        path = strip_path(path, strip);
}

bool string_to_line_number(const std::string& str, LineNumber& output)
{
    if (str.empty())
        return false;

    output = 0;
    for (char it : str) {
        if (!is_digit(it))
            return false;

        if (std::numeric_limits<LineNumber>::max() / 10 < output)
            return false;

        output *= 10;
        unsigned char c = it - '0';
        if (std::numeric_limits<LineNumber>::max() - c < output)
            return false;
        output += c;
    }

    return true;
}

bool parse_unified_range(Hunk& hunk, const std::string& line)
{
    Parser parser(line);

    auto consume_range = [&parser](Range& range) {
        if (!parser.consume_line_number(range.start_line))
            return false;

        if (parser.consume_specific(',')) {
            if (!parser.consume_line_number(range.number_of_lines))
                return false;
        } else {
            range.number_of_lines = 1;
        }
        return true;
    };

    if (!parser.consume_specific("@@ -"))
        return false;

    if (!consume_range(hunk.old_file_range))
        return false;

    if (!parser.consume_specific(" +"))
        return false;

    if (!consume_range(hunk.new_file_range))
        return false;

    if (!parser.consume_specific(" @@"))
        return false;

    return true;
}

// Match the line against the following possibilities, as specified by POSIX:
// "%d a %d            ", <num1>, <num2>
// "%d d %d            ", <num1>, <num2>
// "%d c %d            ", <num1>, <num2>
// "%d a %d , %d       ", <num1>, <num2>, <num3>
// "%d c %d , %d       ", <num1>, <num2>, <num3>
// "%d , %d d %d       ", <num1>, <num2>, <num3>
// "%d , %d c %d       ", <num1>, <num2>, <num3>
// "%d , %d c %d , %d  ", <num1>, <num2>, <num3>, <num4>
bool parse_normal_range(Hunk& hunk, const std::string& line)
{
    auto adjust_offsets = [&hunk](char command) {
        if (command == 'a') {
            hunk.new_file_range.number_of_lines += 1;
        } else if (command == 'd') {
            hunk.old_file_range.number_of_lines += 1;
        } else {
            hunk.new_file_range.number_of_lines += 1;
            hunk.old_file_range.number_of_lines += 1;
        }
    };

    Parser parser(line);

    // Ensure that the first character is an integer.
    if (!parser.consume_line_number(hunk.old_file_range.start_line))
        return false;

    // The next character must either be a ',' followed by a command, or just a command.
    // Skip any optional ',' - remembering whether we found it for later on.
    bool has_first_comma = parser.consume_specific(',');

    // If there is a second integer here, use it for the number of lines
    if (!parser.consume_line_number(hunk.old_file_range.number_of_lines))
        hunk.old_file_range.number_of_lines = 0;

    // Ensure we've now reached a valid normal command.
    char command;
    if (parser.consume_specific('c'))
        command = 'c';
    else if (parser.consume_specific('a'))
        command = 'a';
    else if (parser.consume_specific('d'))
        command = 'd';
    else
        return false;

    // All of the normal commands must have an integer once we've reached this point.
    if (!parser.consume_line_number(hunk.new_file_range.start_line))
        return false;

    // If we're at the end here, this is a valid normal diff command.
    if (parser.is_eof()) {
        hunk.new_file_range.number_of_lines = 0;
        adjust_offsets(command);
        return true;
    }

    // All of the remaining normal commands must have a ',' at this point.
    if (!parser.consume_specific(','))
        return false;

    // And if this is the second comma that was found, this must be a change command.
    if (has_first_comma && command != 'c')
        return false;

    // All remaining normal commands must finish with an integer here.
    if (!parser.consume_line_number(hunk.new_file_range.number_of_lines))
        return false;

    adjust_offsets(command);
    return parser.is_eof();
}

static uint16_t parse_mode(const std::string& mode_str)
{
    if (mode_str.size() != 6)
        throw std::invalid_argument("mode string not of correct size: " + mode_str);

    return static_cast<uint16_t>(std::stoul(mode_str, nullptr, 8));
}

static bool parse_git_extended_info(Patch& patch, const std::string& line, int strip)
{
    auto parse_filename = [&](std::string& output, const std::string& name, const std::string& prefix) {
        // NOTE: we do 'strip - 1' here as the extended headers do not come with a leading
        // "a/" or "b/" prefix - strip the filename as if this part is already stripped.
        if (!name.empty() && name[0] == '"') {
            Parser parser(name);
            output = parser.parse_quoted_string();
            output = strip_path(output, strip - 1);
        } else {
            output = strip_path(name, strip - 1);
        }

        // Special case - we're not stripping at all. So make sure to add on the "a/" or "b/" prefix.
        if (strip == 0)
            output = prefix + output;
    };

    if (starts_with(line, "rename from ")) {
        patch.operation = Operation::Rename;
        parse_filename(patch.old_file_path, line.substr(12, line.size() - 12), "a/");
        return true;
    }

    if (starts_with(line, "rename to ")) {
        patch.operation = Operation::Rename;
        parse_filename(patch.new_file_path, line.substr(10, line.size() - 10), "b/");
        return true;
    }

    if (starts_with(line, "copy to ")) {
        patch.operation = Operation::Copy;
        parse_filename(patch.new_file_path, line.substr(8, line.size() - 8), "b/");
        return true;
    }

    if (starts_with(line, "copy from ")) {
        patch.operation = Operation::Copy;
        parse_filename(patch.old_file_path, line.substr(10, line.size() - 10), "a/");
        return true;
    }

    if (starts_with(line, "deleted file mode ")) {
        patch.operation = Operation::Delete;
        patch.old_file_mode = parse_mode(line.substr(18, line.size() - 18));
        return true;
    }

    if (starts_with(line, "new file mode ")) {
        patch.operation = Operation::Add;
        patch.new_file_mode = parse_mode(line.substr(14, line.size() - 14));
        return true;
    }

    if (starts_with(line, "old mode ")) {
        patch.old_file_mode = parse_mode(line.substr(9, line.size() - 9));
        return true;
    }

    if (starts_with(line, "new mode ")) {
        patch.new_file_mode = parse_mode(line.substr(9, line.size() - 9));
        return true;
    }

    if (starts_with(line, "index ")) {
        return true;
    }

    // NOTE: GIT binary patch line not included as part of header info.
    if (starts_with(line, "GIT binary patch")) {
        patch.operation = Operation::Binary;
        return false;
    }

    return false;
}

static void parse_git_header_name(const std::string& line, Patch& patch, int strip)
{
    Parser parser(line);
    if (!parser.consume_specific("diff --git "))
        return;

    std::string name;
    if (parser.peek() == '"') {
        name = parser.parse_quoted_string();
    } else {
        while (!parser.is_eof()) {
            if (parser.consume_specific(" b/"))
                break;
            name += parser.consume();
        }
    }

    name = strip_path(name, strip);
    patch.old_file_path = name;
    patch.new_file_path = name;
}

bool parse_patch_header(Patch& patch, File& file, PatchHeaderInfo& header_info, int strip)
{
    header_info.patch_start = file.tellg();

    auto this_line_looks_like = Format::Unknown;

    std::string line;

    size_t lines = 0;
    bool is_git_patch = false;
    bool should_parse_body = true;
    Hunk hunk;

    // Iterate through the input file looking for lines that look like a context, normal or unified diff.
    // If we do not know what the format is already, we use this information as a heuristic to determine
    // what the patch should be. Even if we already are told the format of the input patch, we still need
    // to parse this patch header to determine the path of the file to patch, and leave the parsing
    // of the hunks for later on.
    //
    // Once the format is determine, we continue parsing until the beginning of the first hunk is found.
    while (file.get_line(line)) {
        ++lines;

        auto last_line_looks_like = this_line_looks_like;
        this_line_looks_like = Format::Unknown;

        // Look for any file headers in the patch header telling up what the old and new file names are.
        if ((last_line_looks_like != Format::Context && starts_with(line, "*** "))
            || starts_with(line, "+++ ")) {
            parse_file_line(line.substr(4, line.size() - 4), strip, patch.old_file_path, &patch.old_file_time);
            continue;
        }

        if (starts_with(line, "--- ")) {
            parse_file_line(line.substr(4, line.size() - 4), strip, patch.new_file_path, &patch.new_file_time);
            continue;
        }

        if (starts_with(line, "Index: ")) {
            parse_file_line(line.substr(7, line.size() - 7), strip, patch.index_file_path);
            continue;
        }

        // Git diffs sometimes have some extended information in them which can express some
        // operations in a more terse manner. If we recognise a git diff, try and look for
        // these extensions lines in the patch.
        if (starts_with(line, "diff --git ")) {
            // We have already parsed the patch header but have found the next patch! This
            // must mean that we have not found any hunk to parse for the patch body.
            if (is_git_patch) {
                should_parse_body = false;
                break;
            }

            parse_git_header_name(line, patch, strip);
            is_git_patch = true;
            patch.format = Format::Unified;
            continue;
        }

        // Consider any extended info line as part of the hunk as renames and copies may not
        // have any hunk - and we need to advance parsing past this informational section.
        if (is_git_patch && parse_git_extended_info(patch, line, strip)) {
            header_info.lines_till_first_hunk = lines + 1;
            continue;
        }

        // Try and determine where the fist hunk starts from. If we do not already know the format, also
        // make an attempt to determine what format this is.

        if (patch.format == Format::Unknown || patch.format == Format::Unified) {
            if (last_line_looks_like == Format::Unified && (starts_with(line, "+") || starts_with(line, "-") || starts_with(line, " "))) {
                // NOTE: We need to swap back the old and new lines. The old line was parsed as a new
                //       line above since both context patches and unified use '---' for a path
                //       header, but mean different things. Implement this in the simplest way (instead
                //       of storing more names) by storing unified paths the wrong way around and
                //       switching them back so that there is no overlap.
                std::swap(patch.old_file_path, patch.new_file_path);
                std::swap(patch.old_file_time, patch.new_file_time);
                patch.format = Format::Unified;
                break;
            }

            if (parse_unified_range(hunk, line)) {
                this_line_looks_like = Format::Unified;
                header_info.lines_till_first_hunk = lines;
                continue;
            }
        }

        if (patch.format == Format::Unknown || patch.format == Format::Normal) {
            // If we parsed a valid normal range, the next line _should_ be the diff markers.
            if (last_line_looks_like == Format::Normal && (starts_with(line, "> ") || starts_with(line, "< "))) {
                patch.format = Format::Normal;
                patch.new_file_path.clear();
                patch.old_file_path.clear();
                break;
            }

            // If we parse a normal range, it's _probably_ a normal line, and the next line is the beginning of
            // a hunk. However, just in case we are wrong and it's just part of a commit message or something -
            // leave a marker, and keep going to valid that we can find a '<' or '>' marker next.
            if (parse_normal_range(hunk, line)) {
                this_line_looks_like = Format::Normal;
                header_info.lines_till_first_hunk = lines;
                continue;
            }
        }

        if (patch.format == Format::Unknown || patch.format == Format::Context) {
            if (last_line_looks_like == Format::Context && starts_with(line, "*** ")) {
                patch.format = Format::Context;
                break;
            }

            if (starts_with(line, "***************")) {
                this_line_looks_like = Format::Context;
                header_info.lines_till_first_hunk = lines;
                continue;
            }
        }
    }

    file.clear();
    file.seekg(header_info.patch_start);

    header_info.format = patch.format;
    size_t my_lines = header_info.lines_till_first_hunk;
    while (my_lines > 1) {
        --my_lines;
        if (!file.get_line(line))
            throw std::runtime_error("Failure reading line from file parsing patch header");
    }

    if (patch.operation == Operation::Change) {
        if (patch.new_file_path == "/dev/null")
            patch.operation = Operation::Delete;
        else if (patch.old_file_path == "/dev/null")
            patch.operation = Operation::Add;
    }

    return should_parse_body;
}

static Hunk hunk_from_context_parts(LineNumber old_start_line, const std::vector<PatchLine>& old_lines,
    LineNumber new_start_line, const std::vector<PatchLine>& new_lines)
{
    Hunk unified_hunk;
    unified_hunk.old_file_range.start_line = old_start_line;
    unified_hunk.old_file_range.number_of_lines = 0;
    unified_hunk.new_file_range.start_line = new_start_line;
    unified_hunk.new_file_range.number_of_lines = 0;

    size_t old_line_number = 0;
    size_t new_line_number = 0;

    while (old_line_number < old_lines.size() || new_line_number < new_lines.size()) {
        const auto* old_line = old_line_number < old_lines.size() ? &old_lines[old_line_number] : nullptr;
        const auto* new_line = new_line_number < new_lines.size() ? &new_lines[new_line_number] : nullptr;
        if (old_line && old_line->operation == '-') {
            unified_hunk.lines.emplace_back(*old_line);
            unified_hunk.old_file_range.number_of_lines++;
            old_line_number++;
        } else if (new_line && new_line->operation == '+') {
            unified_hunk.lines.emplace_back(*new_line);
            unified_hunk.new_file_range.number_of_lines++;
            new_line_number++;
        } else if (old_line && old_line->operation == '!') {
            unified_hunk.lines.emplace_back('-', old_line->line);
            unified_hunk.old_file_range.number_of_lines++;
            old_line_number++;
        } else if (new_line && new_line->operation == '!') {
            unified_hunk.lines.emplace_back('+', new_line->line);
            unified_hunk.new_file_range.number_of_lines++;
            new_line_number++;
        } else if (old_line && old_line->operation == ' ' && new_line && new_line->operation == ' ') {
            if (old_line->line.content != new_line->line.content)
                throw std::invalid_argument("Context patch line " + old_line->line.content + " does not match " + new_line->line.content);
            unified_hunk.lines.emplace_back(*old_line);
            unified_hunk.old_file_range.number_of_lines++;
            unified_hunk.new_file_range.number_of_lines++;
            old_line_number++;
            new_line_number++;
        } else if (old_line && old_line->operation == ' ') {
            unified_hunk.lines.emplace_back(*old_line);
            unified_hunk.old_file_range.number_of_lines++;
            unified_hunk.new_file_range.number_of_lines++;
            old_line_number++;
        } else if (new_line && new_line->operation == ' ') {
            unified_hunk.lines.emplace_back(*new_line);
            unified_hunk.old_file_range.number_of_lines++;
            unified_hunk.new_file_range.number_of_lines++;
            new_line_number++;
        } else {
            throw std::invalid_argument("Invalid context patch");
        }
    }

    return unified_hunk;
}

static bool parse_context_range(LineNumber& start_line, LineNumber& end_line, const std::string& context_string)
{
    Parser parser(context_string);

    if (!parser.consume_line_number(start_line))
        return false;

    if (!parser.consume_specific(',')) {
        end_line = start_line;
        return true;
    }

    if (!parser.consume_line_number(end_line))
        return false;

    return true;
}

static void parse_context_hunk(std::vector<PatchLine>& old_lines, LineNumber& old_start_line, std::vector<PatchLine>& new_lines, LineNumber& new_start_line, File& file)
{
    std::string line;

    LineNumber old_end_line = 0;
    LineNumber new_end_line = 0;

    auto append_line = [](std::vector<PatchLine>& lines, const std::string& content, NewLine newline) {
        if (content.size() < 2)
            throw std::invalid_argument("Unexpected empty patch line");
        lines.emplace_back(content[0], Line(content.substr(2, content.size()), newline));
    };

    auto append_content = [&](std::vector<PatchLine>& lines, LineNumber start_line, LineNumber end_line) {
        NewLine newline;
        for (LineNumber i = start_line + lines.size(); i <= end_line; ++i) {
            if (!file.get_line(line, &newline))
                throw std::runtime_error("Invalid context patch, unable to retrieve expected number of lines");
            append_line(lines, line, newline);
        }
    };

    auto check_for_no_newline = [&](std::vector<PatchLine>& lines) {
        if (!lines.empty() && file.peek() == '\\') {
            file.get_line(line);
            lines.back().line.newline = NewLine::None;
        }
    };

    auto parse_range = [&](LineNumber& start_line, LineNumber& end_line) {
        if (!starts_with(line, "--- ") || !ends_with(line, " ----"))
            return false;
        if (!parse_context_range(start_line, end_line, line.substr(4, line.size() - 9)))
            throw std::runtime_error("Invalid patch, unable to parse context range");
        return true;
    };

    // Skip over the patch until we find the old file range.
    NewLine newline;
    while (file.get_line(line, &newline)) {
        if (line.rfind("*** ", 0) == 0 && ends_with(line, " ****")) {
            parse_context_range(old_start_line, old_end_line, line.substr(4, line.size() - 9));
            break;
        }
    }

    // If we didn't find any leading hunk, there must have been some garbage
    // at the end of the patch, or we were already there.
    if (file.eof())
        return;

    // We've now parse the range of the 'old-file', but do not know whether the
    // old file contents were omitted in the hunk. Peek ahead a line to check if
    // we can spot the 'to-file' range which looks something like:
    // --- 5,10 ----
    //
    // If we found this, we can skip looking for any old file lines, and just
    // parse that range instead.
    if (!file.get_line(line, &newline))
        throw std::runtime_error("Unable to retrieve line for context range");

    if (!parse_range(new_start_line, new_end_line)) {
        // Append in all of the expected lines that the range header told us to parse.
        append_line(old_lines, line, newline);
        append_content(old_lines, old_start_line, old_end_line);
        check_for_no_newline(old_lines);

        file.get_line(line, &newline);
        if (!parse_range(new_start_line, new_end_line))
            throw std::runtime_error("Could not parse expected range!");

        file.get_line(line, &newline);
        if (file.eof())
            return;

        // Check if we have a 'to-file' that has been omitted, and we have reached the next patch.
        if (starts_with(line, "**********"))
            return;
        append_line(new_lines, line, newline);
    }

    append_content(new_lines, new_start_line, new_end_line);
    check_for_no_newline(new_lines);
}

Patch parse_context_patch(Patch& patch, File& file)
{
    while (true) {
        std::vector<PatchLine> old_lines;
        std::vector<PatchLine> new_lines;
        LineNumber old_start_line = 0;
        LineNumber new_start_line = 0;
        if (file.eof())
            return patch;

        parse_context_hunk(old_lines, old_start_line, new_lines, new_start_line, file);

        if (old_lines.empty() && new_lines.empty())
            return patch;

        Hunk hunk = hunk_from_context_parts(old_start_line, old_lines, new_start_line, new_lines);
        patch.hunks.push_back(hunk);

        auto pos = file.tellg();
        std::string line;
        file.get_line(line);
        file.seekg(pos);

        if (!starts_with(line, "***"))
            return patch;
    }
}

void parse_patch_body(Patch& patch, File& file)
{
    if (patch.format == Format::Unified)
        parse_unified_patch(patch, file);
    else if (patch.format == Format::Context)
        parse_context_patch(patch, file);
    else if (patch.format == Format::Normal)
        parse_normal_patch(patch, file);
    else
        throw std::runtime_error("Unable to determine patch format");
}

Patch parse_unified_patch(Patch& patch, File& file)
{
    Hunk hunk;
    std::string line;

    enum class State {
        InitialHunkContext,
        Content,
    };

    State state = State::InitialHunkContext;

    LineNumber old_lines_expected = -1;
    LineNumber to_lines_expected = -1;

    while (true) {
        NewLine newline;
        auto pos = file.tellg();
        if (!file.get_line(line, &newline))
            break;

        switch (state) {
        case State::InitialHunkContext: {
            // If we find a leading file header in here - then we are are in a new patch.
            // unwind and return the patch which we have currently built. There is potentially
            // a better way of doing this to avoid doing tellg on each line, but for now
            // this seems simple enough.
            if (starts_with(line, "--- ")) {
                file.seekg(pos); // reset parsing the next patch.
                return patch;
            }

            if (parse_unified_range(hunk, line)) {
                state = State::Content;
                old_lines_expected = hunk.old_file_range.number_of_lines;
                to_lines_expected = hunk.new_file_range.number_of_lines;
            }
            break;
        }
        case State::Content: {
            if (line.empty())
                line = " ";

            char what = line.at(0);
            if (what != ' ' && what != '-' && what != '+')
                throw std::invalid_argument("malformed content line: " + line);

            hunk.lines.emplace_back(what, Line(line.substr(1), newline));

            if (what != '-') {
                --to_lines_expected;
                if (to_lines_expected == 0) {
                    // At end of file for 'to', and found a '\ No newline at end of file'
                    if (file.peek() == '\\') {
                        hunk.lines.back().line.newline = NewLine::None;
                        file.get_line(line);
                    }
                }
            }

            if (what != '+') {
                --old_lines_expected;
                if (old_lines_expected == 0) {
                    // At end of file for 'old', and found a '\ No newline at end of file'
                    if (file.peek() == '\\') {
                        hunk.lines.back().line.newline = NewLine::None;
                        file.get_line(line);
                    }
                }
            }

            // We've found everything in the hunk content that we expect.
            // Now loop back to either looking for another hunk in the patch
            // (@@ - ), or for another patch header, in which case we reset
            // and return.
            if (old_lines_expected == 0 && to_lines_expected == 0) {
                patch.hunks.push_back(hunk);
                state = State::InitialHunkContext;
                hunk.old_file_range = {};
                hunk.new_file_range = {};
                hunk.lines.clear();
            }

            break;
        }
        }
    }

    // It is okay to not find any hunks for certain extended format hunks, as the
    // extended format may be the only operation that is being undertaken for this
    // patch.
    if (state == State::InitialHunkContext && patch.hunks.empty())
        return patch;

    if (to_lines_expected != 0)
        throw std::invalid_argument("Expected 0 lines left in 'to', got " + std::to_string(to_lines_expected));
    if (old_lines_expected != 0)
        throw std::invalid_argument("Expected 0 lines left in 'old', got " + std::to_string(old_lines_expected));

    return patch;
}

Patch parse_normal_patch(Patch& patch, File& file)
{
    std::string patch_line;
    if (!file.get_line(patch_line))
        throw std::invalid_argument("Unable to get line for normal range");

    patch.hunks.emplace_back();
    auto* current_hunk = &patch.hunks.back();

    if (!parse_normal_range(*current_hunk, patch_line))
        throw std::invalid_argument("Unable to parse normal range command: " + patch_line);

    // We can simply add up '<' as minuses and '>' as additions
    // and skip over the separating ---
    //
    // If we see something else, it is the beginning of a new patch,
    // so we parse that.
    NewLine newline;
    while (file.get_line(patch_line, &newline)) {
        if (patch_line.empty())
            break;

        const char c = patch_line.at(0);
        if (c == '<') {
            current_hunk->lines.emplace_back('-', Line(patch_line.substr(2, patch_line.size()), newline));
        } else if (c == '>') {
            current_hunk->lines.emplace_back('+', Line(patch_line.substr(2, patch_line.size()), newline));
        } else if (c == '\\') {
            assert(!current_hunk->lines.empty()); // precondition checked by `parse_patch_header`
            current_hunk->lines.back().line.newline = NewLine::None;
        } else if (patch_line == "---") {
            // do nothing
        } else {
            patch.hunks.emplace_back();
            current_hunk = &patch.hunks.back();
            if (!parse_normal_range(*current_hunk, patch_line))
                throw std::invalid_argument("Failed parsing expected normal patch command: " + patch_line);
        }
    }

    return patch;
}

Patch parse_patch(File& file, Format format, int strip)
{
    Patch patch(format);
    PatchHeaderInfo info;
    bool should_parse_body = parse_patch_header(patch, file, info, strip);
    if (should_parse_body)
        parse_patch_body(patch, file);
    return patch;
}

std::string strip_path(const std::string& path, int amount)
{
    // A negative strip count (the default) indicates that we use the basename of the filepath.
    const bool strip_leading = amount < 0;
    if (strip_leading)
        return path.substr(path.find_last_of("/\\") + 1);

    int remaining_to_strip = amount;
    auto stripped_begin = path.begin();
    for (auto c = path.begin(); c != path.end(); ++c) {
        if (*c == '/' || *c == '\\') {
            // A double slash resolves as the same path as a single one does.
            ++c;
            if (c != path.end() && (*c == '/' || *c == '\\'))
                ++c;

            if (--remaining_to_strip >= 0)
                stripped_begin = c;
        }
    }

    // Ignore the name if we don't have enough to strip
    if (stripped_begin == path.end() || remaining_to_strip > 0)
        return {};

    return std::string(stripped_begin, path.end());
}

} // namespace Patch
