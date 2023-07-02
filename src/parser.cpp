// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cassert>
#include <limits>
#include <patch/hunk.h>
#include <patch/parser.h>
#include <patch/system.h>
#include <patch/utils.h>
#include <sstream>
#include <stdexcept>

namespace Patch {

class parser_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

bool LineParser::is_eof() const
{
    return m_current == m_end;
}

char LineParser::peek() const
{
    if (m_current == m_end)
        return '\0';
    return *m_current;
}

char LineParser::consume()
{
    if (m_current == m_end)
        return '\0';
    char c = *m_current;
    ++m_current;
    return c;
}

bool LineParser::consume_specific(char c)
{
    if (m_current == m_end)
        return false;
    if (*m_current != c)
        return false;

    ++m_current;
    return true;
}

bool LineParser::consume_specific(const char* chars)
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

bool LineParser::consume_uint()
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

bool LineParser::consume_line_number(LineNumber& output)
{
    auto start = m_current;
    if (!consume_uint())
        return false;

    return string_to_line_number(std::string(start, m_current), output);
}

std::string LineParser::parse_quoted_string()
{
    const auto begin = m_current;
    std::string output;

    consume_specific('"');

    while (!is_eof()) {
        // Reached the end of the string
        if (peek() == '"')
            return output;

        // Some escaped character, peek past to determine the intended unescaped character.
        if (consume_specific('\\')) {
            char c = consume();
            switch (c) {
            case '\0':
                throw std::invalid_argument("Invalid unterminated \\ in quoted path " + std::string(begin, m_end));
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
                unsigned char result = static_cast<unsigned char>(c) - '0';

                for (int i = 1; i < 3; ++i) {
                    char octal_val = peek();
                    if (!is_octal(octal_val))
                        break;

                    unsigned char digit_val = static_cast<unsigned char>(octal_val) - '0';
                    result = result * 8 + digit_val;

                    ++m_current;
                }

                output += static_cast<char>(result);
                break;
            }
            default:
                throw std::invalid_argument("Invalid or unsupported escape character in path " + std::string(begin, m_current));
            }
        } else {
            // Normal case - a character of the path we can just add to our output.
            output += consume();
        }
    }

    throw std::invalid_argument("Failed to find terminating \" when parsing " + std::string(begin, m_current));
}

void LineParser::parse_file_line(int strip, std::string& path, std::string* timestamp)
{
    if (is_eof()) {
        path.clear();
        if (timestamp)
            timestamp->clear();
        return;
    }

    auto it = m_current;

    if (*it == '"') {
        path = parse_quoted_string();
        it = m_current;
    } else {
        // In most patches, a \t is used to separate the path from
        // the timestamp. However, POSIX does not seem to specify one
        // way or another.
        //
        // GNU diff seems to always quote paths with whitespace in them,
        // however git does not. To implement this, if we find any tab
        // that is considered the end of the path. Otherwise if no tab
        // is found, we fall back on using a space.
        auto begin = m_current;

        while (true) {
            if (it == m_end) {
                path = std::string(begin, m_end);
                break;
            }

            // Must have reached the end of the path.
            if (*it == '\t') {
                path = std::string(begin, it);
                break;
            }

            // May have reached end of the path, but it also could be
            // a path with spaces. Look for a tab to find out.
            if (*it == ' ') {
                auto new_it = std::find(it, m_end, '\t');
                if (new_it == m_end) {
                    path = std::string(begin, it);
                } else {
                    it = new_it;
                    path = std::string(begin, new_it);
                }
                break;
            }

            ++it;
        }
    }

    // Anything after the path is considered the timestamp.
    // Currently this may also include whitespace! (which depends on
    // how the path and timestamp were separated in the patch).
    if (timestamp && it != m_end && it + 1 != m_end)
        *timestamp = std::string(it + 1, m_end);

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
        unsigned char c = static_cast<unsigned char>(it) - '0';
        if (std::numeric_limits<LineNumber>::max() - c < output)
            return false;
        output += c;
    }

    return true;
}

bool parse_unified_range(Hunk& hunk, const std::string& line)
{
    LineParser parser(line);

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
//
// <num> is used to specify the start and end lines of the two files being diffed.
bool parse_normal_range(Hunk& hunk, const std::string& line)
{
    LineParser parser(line);

    // Ensure that the first character is an integer.
    if (!parser.consume_line_number(hunk.old_file_range.start_line))
        return false;

    // The next character must either be a ',' followed by a number of lines, or just a command.
    // Skip any optional ',' - remembering whether we found it for later on.
    bool has_first_comma = parser.consume_specific(',');
    if (has_first_comma && !parser.consume_line_number(hunk.old_file_range.number_of_lines))
        return false;

    // Ensure we've now reached a valid normal command.
    char command = parser.consume();
    if (command != 'c' && command != 'a' && command != 'd')
        return false;

    // Only a single line between the start and end of the old file. If we are appending then
    // the old file must not have any lines in the diff. Otherwise there must be something which
    // is being changed or removed.
    if (!has_first_comma)
        hunk.old_file_range.number_of_lines = command == 'a' ? 0 : 1;

    // All of the normal commands must have an integer once we've reached this point.
    if (!parser.consume_line_number(hunk.new_file_range.start_line))
        return false;

    // Read the end line of the new file to work backwards to determine the number of lines.
    LineNumber new_range_end_line = 0;
    if (parser.consume_specific(',')) {
        // If this is the second comma that was found, this must be a change command.
        if (has_first_comma && command != 'c')
            return false;
        if (!parser.consume_line_number(new_range_end_line))
            return false;
    } else {
        new_range_end_line = hunk.new_file_range.start_line;
    }

    hunk.new_file_range.number_of_lines = new_range_end_line - hunk.new_file_range.start_line + 1;
    if (command == 'd')
        --hunk.new_file_range.number_of_lines;

    return parser.is_eof();
}

static uint16_t parse_mode(const std::string& mode_str)
{
    // Ignore any mode strings which are not in the format which we expect.

    if (mode_str.size() != 6)
        return 0;

    try {
        size_t pos;
        auto value = std::stoul(mode_str, &pos, 8);
        if (pos != mode_str.size())
            return 0;

        return static_cast<uint16_t>(value);
    } catch (const std::exception&) {
        // do nothing
    }

    return 0;
}

bool LineParser::parse_git_extended_info(Patch& patch, int strip)
{
    auto parse_filename = [&](std::string& output, const std::string& prefix) {
        // NOTE: we do 'strip - 1' here as the extended headers do not come with a leading
        // "a/" or "b/" prefix - strip the filename as if this part is already stripped.
        if (peek() == '"') {
            output = parse_quoted_string();
            output = strip_path(output, strip - 1);
        } else {
            output = strip_path(std::string(m_current, m_end), strip - 1);
        }

        // Special case - we're not stripping at all. So make sure to add on the "a/" or "b/" prefix.
        if (strip == 0)
            output = prefix + output;
    };

    if (consume_specific("rename from ")) {
        patch.operation = Operation::Rename;
        parse_filename(patch.old_file_path, "a/");
        return true;
    }

    if (consume_specific("rename to ")) {
        patch.operation = Operation::Rename;
        parse_filename(patch.new_file_path, "b/");
        return true;
    }

    if (consume_specific("copy to ")) {
        patch.operation = Operation::Copy;
        parse_filename(patch.new_file_path, "b/");
        return true;
    }

    if (consume_specific("copy from ")) {
        patch.operation = Operation::Copy;
        parse_filename(patch.old_file_path, "a/");
        return true;
    }

    if (consume_specific("deleted file mode ")) {
        patch.operation = Operation::Delete;
        patch.old_file_mode = parse_mode(std::string(m_current, m_end));
        return true;
    }

    if (consume_specific("new file mode ")) {
        patch.operation = Operation::Add;
        patch.new_file_mode = parse_mode(std::string(m_current, m_end));
        return true;
    }

    if (consume_specific("old mode ")) {
        patch.old_file_mode = parse_mode(std::string(m_current, m_end));
        return true;
    }

    if (consume_specific("new mode ")) {
        patch.new_file_mode = parse_mode(std::string(m_current, m_end));
        return true;
    }

    if (consume_specific("index "))
        return true;

    // NOTE: GIT binary patch line not included as part of header info.
    if (consume_specific("GIT binary patch")) {
        patch.operation = Operation::Binary;
        return false;
    }

    return false;
}

void LineParser::parse_git_header_name(Patch& patch, int strip)
{
    std::string name;
    if (peek() == '"') {
        name = parse_quoted_string();
    } else {
        while (!is_eof()) {
            if (consume_specific(" b/"))
                break;
            name += consume();
        }
    }

    name = strip_path(name, strip);
    patch.old_file_path = name;
    patch.new_file_path = name;
}

Parser::Parser(File& file)
    : m_file(file)
{
}

bool Parser::get_line(std::string& line, NewLine* newline)
{
    if (!m_file.get_line(line, newline))
        return false;
    ++m_line_number;
    return true;
}

void Parser::print_header_info(const PatchHeaderInfo& header_info, std::ostream& out)
{
    m_file.seekg(header_info.patch_start);

    if (header_info.lines_till_first_hunk > 1) {
        out << "The text leading up to this was:\n"
            << "--------------------------\n";
        size_t my_lines = header_info.lines_till_first_hunk;
        std::string line;
        while (my_lines > 1) {
            --my_lines;
            if (!m_file.get_line(line))
                throw std::runtime_error("Failure reading line from patch outputting header info");

            out << '|' << line << '\n';
        }
        out << "--------------------------\n";
    }
}

bool Parser::parse_patch_header(Patch& patch, PatchHeaderInfo& header_info, int strip)
{
    header_info.patch_start = m_file.tellg();

    auto this_line_looks_like = Format::Unknown;

    std::string line;

    size_t lines = 0;
    bool is_git_patch = false;
    bool should_parse_body = true;
    Hunk hunk;

    auto start_line_number = m_line_number;

    // Iterate through the input file looking for lines that look like a context, normal or unified diff.
    // If we do not know what the format is already, we use this information as a heuristic to determine
    // what the patch should be. Even if we already are told the format of the input patch, we still need
    // to parse this patch header to determine the path of the file to patch, and leave the parsing
    // of the hunks for later on.
    //
    // Once the format is determine, we continue parsing until the beginning of the first hunk is found.
    while (get_line(line)) {
        LineParser parser(line);

        ++lines;
        auto last_line_looks_like = this_line_looks_like;
        this_line_looks_like = Format::Unknown;

        // Look for any file headers in the patch header telling up what the old and new file names are.
        if ((last_line_looks_like != Format::Context && parser.consume_specific("*** "))
            || parser.consume_specific("+++ ")) {
            parser.parse_file_line(strip, patch.old_file_path, &patch.old_file_time);
            continue;
        }

        if (parser.consume_specific("--- ")) {
            parser.parse_file_line(strip, patch.new_file_path, &patch.new_file_time);
            continue;
        }

        if (parser.consume_specific("Index: ")) {
            parser.parse_file_line(strip, patch.index_file_path);
            continue;
        }

        if (parser.consume_specific("Prereq: ")) {
            parser.parse_file_line(strip, patch.prerequisite);
            continue;
        }

        // Git diffs sometimes have some extended information in them which can express some
        // operations in a more terse manner. If we recognise a git diff, try and look for
        // these extensions lines in the patch.
        if (parser.consume_specific("diff --git ")) {
            // We have already parsed the patch header but have found the next patch! This
            // must mean that we have not found any hunk to parse for the patch body.
            if (is_git_patch) {
                should_parse_body = false;
                break;
            }

            parser.parse_git_header_name(patch, strip);
            is_git_patch = true;
            patch.format = Format::Unified;
            continue;
        }

        // Consider any extended info line as part of the hunk as renames and copies may not
        // have any hunk - and we need to advance parsing past this informational section.
        if (is_git_patch && parser.parse_git_extended_info(patch, strip)) {
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

    if (is_git_patch)
        patch.format = Format::Git;

    m_file.clear();
    m_file.seekg(header_info.patch_start);

    // FIXME: This should get tidied up to something more sensible.
    header_info.format = patch.format;
    m_line_number = start_line_number;
    size_t my_lines = header_info.lines_till_first_hunk;
    while (my_lines > 1) {
        --my_lines;
        if (!get_line(line))
            throw std::runtime_error("Failure reading line from file parsing patch header");
    }

    if (patch.operation == Operation::Change) {
        if (hunk.new_file_range.start_line == 0)
            patch.operation = Operation::Delete;
        else if (hunk.old_file_range.start_line == 0)
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
    LineParser parser(context_string);

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

void Parser::parse_context_hunk(std::vector<PatchLine>& old_lines, LineNumber& old_start_line, std::vector<PatchLine>& new_lines, LineNumber& new_start_line)
{
    std::string line;

    LineNumber from_file_range_line_number = 0;

    LineNumber old_end_line = 0;
    LineNumber new_end_line = 0;

    auto append_line = [&](std::vector<PatchLine>& lines, const std::string& content, NewLine newline) {
        if (content.size() < 2)
            throw std::invalid_argument("Unexpected empty patch line");
        lines.emplace_back(content[0], Line(content.substr(2, content.size()), newline));

        if (content[1] == '-')
            throw parser_error("Premature '---' at line " + std::to_string(m_line_number - 1) + "; check line numbers at line " + std::to_string(from_file_range_line_number));

        const auto& line = lines.back();
        if (line.operation != ' ' && line.operation != '+' && line.operation != '-' && line.operation != '!') {
            std::ostringstream ss;
            ss << "malformed patch at line " << (m_line_number - 1) << ": " << content << '\n';
            throw parser_error(ss.str());
        }
    };

    auto append_content = [&](std::vector<PatchLine>& lines, LineNumber start_line, LineNumber end_line) {
        NewLine newline;
        for (LineNumber i = start_line + static_cast<LineNumber>(lines.size()); i <= end_line; ++i) {
            if (!get_line(line, &newline))
                throw parser_error("context mangled in hunk at line " + std::to_string(from_file_range_line_number));
            append_line(lines, line, newline);
        }
    };

    auto check_for_no_newline = [&](std::vector<PatchLine>& lines) {
        if (!lines.empty() && m_file.peek() == '\\') {
            get_line(line);
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
    while (get_line(line, &newline)) {
        if (starts_with(line, "*** ") && ends_with(line, " ****")) {
            parse_context_range(old_start_line, old_end_line, line.substr(4, line.size() - 9));
            from_file_range_line_number = m_line_number - 1;
            break;
        }
    }

    // We've now parse the range of the 'old-file', but do not know whether the
    // old file contents were omitted in the hunk. Peek ahead a line to check if
    // we can spot the 'to-file' range which looks something like:
    // --- 5,10 ----
    //
    // If we found this, we can skip looking for any old file lines, and just
    // parse that range instead.
    if (!get_line(line, &newline))
        throw std::runtime_error("Unable to retrieve line for context range");

    if (!parse_range(new_start_line, new_end_line)) {
        // Append in all of the expected lines that the range header told us to parse.
        append_line(old_lines, line, newline);
        append_content(old_lines, old_start_line, old_end_line);
        check_for_no_newline(old_lines);

        get_line(line, &newline);
        if (!parse_range(new_start_line, new_end_line))
            throw std::runtime_error("Could not parse expected range!");

        get_line(line, &newline);
        if (m_file.eof())
            return;

        // Check if we have a 'to-file' that has been omitted, and we have reached the next patch.
        if (starts_with(line, "**********"))
            return;
        append_line(new_lines, line, newline);
    }

    append_content(new_lines, new_start_line, new_end_line);
    check_for_no_newline(new_lines);
}

Patch Parser::parse_context_patch(Patch& patch)
{
    while (true) {
        std::vector<PatchLine> old_lines;
        std::vector<PatchLine> new_lines;
        LineNumber old_start_line = 0;
        LineNumber new_start_line = 0;

        parse_context_hunk(old_lines, old_start_line, new_lines, new_start_line);

        Hunk hunk = hunk_from_context_parts(old_start_line, old_lines, new_start_line, new_lines);
        patch.hunks.push_back(hunk);

        auto pos = m_file.tellg();
        std::string line;
        get_line(line);
        m_file.seekg(pos);

        if (!starts_with(line, "***"))
            return patch;
    }
}

void Parser::parse_patch_body(Patch& patch)
{
    if (patch.format == Format::Unified || patch.format == Format::Git)
        parse_unified_patch(patch);
    else if (patch.format == Format::Context)
        parse_context_patch(patch);
    else if (patch.format == Format::Normal)
        parse_normal_patch(patch);
    else
        throw std::runtime_error("Unable to determine patch format");
}

Patch Parser::parse_unified_patch(Patch& patch)
{
    Hunk hunk;
    std::string line;

    enum class State {
        InitialHunkContext,
        Content,
    };

    State state = State::InitialHunkContext;

    LineNumber old_lines_expected = -1;
    LineNumber new_lines_expected = -1;

    while (true) {
        NewLine newline;
        if (!get_line(line, &newline))
            break;

        switch (state) {
        case State::InitialHunkContext: {
            if (parse_unified_range(hunk, line)) {
                state = State::Content;
                old_lines_expected = hunk.old_file_range.number_of_lines;
                new_lines_expected = hunk.new_file_range.number_of_lines;
            }
            break;
        }
        case State::Content: {
            if (line.empty())
                line = " ";

            char what = line.at(0);
            if (what != ' ' && what != '-' && what != '+') {
                std::ostringstream ss;
                ss << "malformed patch at line " << (m_line_number - 1) << ": " << line << '\n';
                throw parser_error(ss.str());
            }

            hunk.lines.emplace_back(what, Line(line.substr(1), newline));

            if (what != '-') {
                --new_lines_expected;
                // At end of file for 'to', and found a '\ No newline at end of file'
                if (new_lines_expected == 0 && m_file.peek() == '\\') {
                    hunk.lines.back().line.newline = NewLine::None;
                    get_line(line);
                }
            }

            if (what != '+') {
                --old_lines_expected;
                // At end of file for 'old', and found a '\ No newline at end of file'
                if (old_lines_expected == 0 && m_file.peek() == '\\') {
                    hunk.lines.back().line.newline = NewLine::None;
                    get_line(line);
                }
            }

            // We've found everything for the current hunk that we expect.
            if (old_lines_expected == 0 && new_lines_expected == 0) {
                patch.hunks.push_back(hunk);
                hunk.lines.clear();

                // If we can spot another hunk on the next line, continue
                // to parse the next hunk, otherwise return the end of
                // this patch.
                auto pos = m_file.tellg();
                if (!get_line(line))
                    return patch;

                if (!parse_unified_range(hunk, line)) {
                    --m_line_number;
                    m_file.seekg(pos);
                    return patch;
                }

                state = State::Content;
                old_lines_expected = hunk.old_file_range.number_of_lines;
                new_lines_expected = hunk.new_file_range.number_of_lines;
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

    if (new_lines_expected != 0)
        throw std::invalid_argument("Expected 0 lines left in 'to', got " + std::to_string(new_lines_expected));
    if (old_lines_expected != 0)
        throw std::invalid_argument("Expected 0 lines left in 'old', got " + std::to_string(old_lines_expected));

    return patch;
}

Patch Parser::parse_normal_patch(Patch& patch)
{
    NewLine newline;
    std::string patch_line;

    while (get_line(patch_line)) {
        if (m_file.eof() || patch_line.empty())
            break;

        patch.hunks.emplace_back();
        auto& current_hunk = patch.hunks.back();
        if (!parse_normal_range(current_hunk, patch_line))
            throw std::invalid_argument("Unable to parse normal range command: " + patch_line);

        for (LineNumber i = 0; i < current_hunk.old_file_range.number_of_lines; ++i) {
            if (!get_line(patch_line, &newline))
                throw parser_error("unexpected end of file in patch at line " + std::to_string(m_line_number - 1));

            if (patch_line.size() < 2 || patch_line[0] != '<' || !is_whitespace(patch_line[1]))
                throw parser_error("'<' followed by space or tab expected at line " + std::to_string(m_line_number - 1) + " of patch");

            current_hunk.lines.emplace_back('-', Line(patch_line.substr(2, patch_line.size()), newline));
        }

        if (m_file.peek() == '\\') {
            get_line(patch_line, &newline);
            current_hunk.lines.back().line.newline = NewLine::None;
        }

        // Expect --- if 'c' command
        if (m_file.peek() == '-') {
            get_line(patch_line, &newline);
        }

        for (LineNumber i = 0; i < current_hunk.new_file_range.number_of_lines; ++i) {
            if (!get_line(patch_line, &newline))
                throw parser_error("unexpected end of file in patch at line " + std::to_string(m_line_number - 1));

            if (patch_line.size() < 2 || patch_line[0] != '>' || !is_whitespace(patch_line[1]))
                throw parser_error("'>' followed by space or tab expected at line " + std::to_string(m_line_number - 1) + " of patch");

            current_hunk.lines.emplace_back('+', Line(patch_line.substr(2, patch_line.size()), newline));
        }

        if (m_file.peek() == '\\') {
            get_line(patch_line, &newline);
            current_hunk.lines.back().line.newline = NewLine::None;
        }
    }
    return patch;
}

Patch parse_patch(File& file, Format format, int strip)
{
    Parser parser(file);
    Patch patch(format);
    PatchHeaderInfo info;
    bool should_parse_body = parser.parse_patch_header(patch, info, strip);
    if (should_parse_body)
        parser.parse_patch_body(patch);
    return patch;
}

std::string strip_path(const std::string& path, int amount)
{
    // A negative strip count (the default) indicates that we use the basename of the filepath.
    const bool use_basename = amount < 0;
    if (use_basename)
        return filesystem::basename(path);

    int remaining_to_strip = amount;
    auto stripped_begin = path.begin();
    for (auto c = path.begin(); c != path.end(); ++c) {
        if (filesystem::is_seperator(*c)) {
            // A double slash resolves as the same path as a single one does.
            ++c;
            if (c != path.end() && filesystem::is_seperator(*c))
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
