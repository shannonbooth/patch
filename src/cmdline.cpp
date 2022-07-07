// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cstring>
#include <patch/cmdline.h>
#include <patch/options.h>
#include <stdexcept>

namespace Patch {

bool CmdLineParser::parse_string(char short_opt, const char* long_opt, std::string& option)
{
    auto parse_value_as_next_arg = [&]() {
        const char* c = m_argv[i + 1];
        if (!c)
            throw cmdline_parse_error("option missing operand for " + std::string(m_argv[i]));
        option = c;
        ++i;
        return true;
    };

    // Check if we've got the short option
    if (m_argv[i][0] == '-' && m_argv[i][1] == short_opt) {
        if (m_argv[i][2] == '\0')
            return parse_value_as_next_arg();

        const char* c = m_argv[i] + 3;
        if (!c)
            throw cmdline_parse_error("option missing operand for " + std::string(m_argv[i]));
        option = m_argv[i] + 2;
        return true;
    }

    if (!long_opt)
        return false;

    // Check if the token is equal to the long option.
    const char* argument = m_argv[i];
    while (*long_opt && *argument) {
        if (*long_opt != *argument)
            break;
        ++long_opt;
        ++argument;
    }

    // If we're at the end of the long option - the operand must be in the next position.
    if (*argument == '\0')
        return parse_value_as_next_arg();

    // Otherwise, if we're at the end of the argument and the next character in the argument is a '='
    // then the operand must be whatever is after the = in the argument.
    if (!*long_opt && *argument == '=') {
        const char* c = argument + 1;
        if (!c)
            throw cmdline_parse_error("option missing operand for " + std::string(m_argv[i]));
        option = c;
        ++i;
        return true;
    }

    return false;
}

bool CmdLineParser::parse_int(char short_opt, const char* long_opt, int& option)
{
    std::string option_as_str;
    if (!parse_string(short_opt, long_opt, option_as_str))
        return false;

    try {
        option = std::stoi(option_as_str);
    } catch (const std::invalid_argument&) {
        throw cmdline_parse_error("unable to parse cmdline option '" + option_as_str + "' as integer for: " + std::string(long_opt));
    }

    return true;
}

void CmdLineParser::parse_operand()
{
    // We only support two positional arguments.
    if (m_positional_arguments_found == 2)
        throw cmdline_parse_error("extra operand " + std::string(m_argv[i]));

    if (m_positional_arguments_found == 0)
        m_options.file_to_patch = m_argv[i];
    else
        m_options.patch_file_path = m_argv[i];

    ++m_positional_arguments_found;
}

void CmdLineParser::handle_newline_strategy(const std::string& strategy)
{
    if (strategy == "native")
        m_options.newline_output = Options::NewlineOutput::Native;
    else if (strategy == "lf")
        m_options.newline_output = Options::NewlineOutput::LF;
    else if (strategy == "crlf")
        m_options.newline_output = Options::NewlineOutput::CRLF;
    else if (strategy == "preserve")
        m_options.newline_output = Options::NewlineOutput::Keep;
    else
        throw cmdline_parse_error("unrecognized newline strategy " + strategy);
}

void CmdLineParser::handle_reject_format(const std::string& strategy)
{
    if (strategy == "context")
        m_options.reject_format = Options::RejectFormat::Context;
    else if (strategy == "unified")
        m_options.reject_format = Options::RejectFormat::Unified;
    else
        throw cmdline_parse_error("unrecognized reject format " + strategy);
}

void CmdLineParser::parse_short_bool(const std::string& option_string)
{
    for (auto it = option_string.begin() + 1; it != option_string.end(); ++it) {
        char c = *it;
        auto matched_option_it = std::find_if(m_bool_options.begin(), m_bool_options.end(), [&](BoolOption& option) {
            if (c != option.short_name)
                return false;

            option.enabled = true;
            return true;
        });

        if (matched_option_it == m_bool_options.end())
            throw cmdline_parse_error("unknown commandline argument " + std::string(1, c));
    }
}

void CmdLineParser::parse_long_bool(const std::string& option_string)
{
    auto it = std::find_if(m_bool_options.begin(), m_bool_options.end(), [&](BoolOption& option) {
        if (option_string != option.long_name)
            return false;

        option.enabled = true;
        return true;
    });

    if (it == m_bool_options.end())
        throw cmdline_parse_error("unknown commandline argument " + option_string);
}

const Options& CmdLineParser::parse()
{
    for (; i < m_argc; ++i) {

        // If the option does not start with '-' it must be a positional argument.
        // However, there is a special case for '-' which denotes reading from stdin.
        //
        // NOTE: this behaviour deviates from POSIX which expects all option arguments
        //       to be given before the operands.
        if (*m_argv[i] != '-' || std::strcmp(m_argv[i], "-") == 0) {
            parse_operand();
            continue;
        }

        // If the arg given is "--" then all arguments afterwards are interpreted as operands.
        if (std::strcmp(m_argv[i], "--") == 0) {
            ++i;
            for (; i < m_argc; ++i)
                parse_operand();
            break;
        }

        // By this stage, we know that this arg is some option.
        // Now we need to work out which one it is.
        if (parse_string('i', "--input", m_options.patch_file_path))
            continue;

        if (parse_string('d', "--directory", m_options.patch_directory_path))
            continue;

        if (parse_string('D', "--ifdef", m_options.define_macro))
            continue;

        if (parse_string('o', "--output", m_options.out_file_path))
            continue;

        if (parse_string('r', "--reject-file", m_options.reject_file_path))
            continue;

        if (parse_int('p', "--strip", m_options.strip_size))
            continue;

        if (parse_int('F', "--fuzz", m_options.max_fuzz))
            continue;

        if (parse_string('\0', "--newline-output", m_buffer)) {
            handle_newline_strategy(m_buffer);
            continue;
        }

        if (parse_string('\0', "--reject-format", m_buffer)) {
            handle_reject_format(m_buffer);
            continue;
        }

        std::string option_string = m_argv[i];

        bool is_long_option = option_string[1] == '-';

        if (is_long_option)
            parse_long_bool(option_string);
        else
            parse_short_bool(option_string);
    }

    return m_options;
}

void show_version(std::ostream& out)
{
    out << "patch 0.0.1\n"
        << "Copyright (C) 2022 Shannon Booth\n";
}

void show_usage(std::ostream& out)
{
    out << "patch - (C) 2022 Shannon Booth\n"
           "\n"
           "patch reads a patch file containing a difference (diff) and applies it to files.\n"
           "\n"
           "By default, patch will read the patch from stdin. Unified, context and normal format diffs are\n"
           "supported. Unless told otherwise, patch will try to determine the format of the diff listing\n"
           "automatically.\n"
           "\n"
           "USAGE:\n"
           "    patch [OPTIONS]... [FILE [PATCH]]\n"
           "\n"
           "OPTIONS:\n"
           "    -p, --strip <number>\n"
           "                Strip <number> of leading path components from file names. By default, without this\n"
           "                option set, patch will strip all components from the path, leaving the basename.\n"
           "\n"
           "    -l, --ignore-whitespace\n"
           "                When searching through the file to patch, try to ignore whitespace differences\n"
           "                between the patch and input file. Patch will ignore different line endings\n"
           "                between lines, and will try to also ignore any differences in indentation.\n"
           "\n"
           "    -c, --context\n"
           "                Interpret the patch as a context format patch.\n"
           "\n"
           "    -n, --normal\n"
           "                Interpret the patch file in the normal format.\n"
           "\n"
           "    -u, --unified\n"
           "                Interpret the patch file in the unified format.\n"
           "\n"
           "    -F, --fuzz <fuzz>\n"
           "                Set the maximum amount of 'fuzz' (default 2). When searching where to apply a\n"
           "                hunk, if lines matching the context are not able to be matched, patch will try to\n"
           "                re-apply the hunk ignoring up to <fuzz> lines of surrounding context.\n"
           "\n"
           "    -N, --forward\n"
           "                Ignore patches where it looks like the diff has already been applied to the input\n"
           "                file.\n"
           "\n"
           "    -R, --reverse\n"
           "                Reverse the given patch script. Assume that the old file contents are the new file\n"
           "                contents, and vice-versa.\n"
           "\n"
           "    -i, --input <path>\n"
           "                Read the patch file from <path> instead of using stdin.\n"
           "\n"
           "    -o, --output <file>\n"
           "                Output what would be the result of patched files to <file>.\n"
           "\n"
	   "    -r, --reject-file <file>\n"
           "                Write the reject file to <file> instead of the default location '<output-file>.rej'.\n"
           "\n"
           "    -D, --ifdef <define>\n"
           "                When applying patch to a file all differences will be marked with a C preprocessor\n"
           "                construct. The given <define> is used as the symbol in the generated '#ifdef'.\n"
           "\n"
           "    -f  --force\n"
           "                Do not prompt for input, try to apply patch as given.\n"
           "\n"
           "    -b  --backup\n"
           "                Before writing to the patched file, make a backup of the file that will be written\n"
           "                to. The output file with be given the filename suffix '.orig'.\n"
           "\n"
           "    --reject-format <format>\n"
           "                Write reject files in either 'unified' or 'context' format. By default, patch will\n"
           "                write unified patch rejects in unified format and use the context format otherwise.\n"
           "\n"
           "    --verbose\n"
           "                Make the patch application more talkative about what is being done.\n"
           "\n"
           "    --dry-run\n"
           "                Do not actually patch any files, perform a trial run to see what would happen.\n"
           "\n"
           "    -d, --directory <directory>\n"
           "                Change the working directory to <directory> before applying the patch file.\n"
           "\n"
           "    --newline-handling <handling>\n"
           "                Change how newlines are output to the patched file. The default newline behavior\n"
           "                is 'native'. The possible values for this flag are:\n"
           "\n"
           "                    native    Newlines for the patched file will be written in the platforms native format.\n"
           "                    lf        All newlines in the output file will be written as LF.\n"
           "                    crlf      All newlines in the output file will be written as CRLF.\n"
           "                    preserve  Patch will attempt to preserve newlines of the patched file.\n"
           "\n"
           "    -v  --version\n"
           "                Prints version information.\n"
           "\n"
           "    --help\n"
           "                Output this help.\n"
           "\n"
           "ARGS:\n"
           "    <FILE>\n"
           "                Path to the file to patch.\n"
           "\n"
           "    <PATCH>\n"
           "                Path to the patch file for patch to read from.\n";
}

} // namespace Patch
