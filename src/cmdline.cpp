// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <patch/cmdline.h>
#include <patch/options.h>
#include <patch/system.h>
#include <patch/utils.h>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace Patch {

namespace {

enum class HasArgument {
    Yes,
    No,
};

struct Option {
    int short_name;
    const char* long_name;
    HasArgument has_argument;
};

const std::array<Option, 28> s_switches { {
    { 'B', "--prefix", HasArgument::Yes },
    { 'D', "--ifdef", HasArgument::Yes },
    { 'F', "--fuzz", HasArgument::Yes },
    { 'N', "--forward", HasArgument::No },
    { 'R', "--reverse", HasArgument::No },
    { 'b', "--backup", HasArgument::No },
    { 'c', "--context", HasArgument::No },
    { 'd', "--directory", HasArgument::Yes },
    { 'e', "--ed", HasArgument::No },
    { 'f', "--force", HasArgument::No },
    { 'h', "--help", HasArgument::No },
    { 'i', "--input", HasArgument::Yes },
    { 'l', "--ignore-whitespace", HasArgument::No },
    { 'n', "--normal", HasArgument::No },
    { 'o', "--output", HasArgument::Yes },
    { 'p', "--strip", HasArgument::Yes },
    { 'r', "--reject-file", HasArgument::Yes },
    { 't', "--batch", HasArgument::No },
    { 'u', "--unified", HasArgument::No },
    { 'v', "--version", HasArgument::No },
    { 'z', "--suffix", HasArgument::Yes },
    { CHAR_MAX + 1, "--newline-output", HasArgument::Yes },
    { CHAR_MAX + 2, "--read-only", HasArgument::Yes },
    { CHAR_MAX + 3, "--reject-format", HasArgument::Yes },
    { CHAR_MAX + 4, "--verbose", HasArgument::No },
    { CHAR_MAX + 5, "--dry-run", HasArgument::No },
    { CHAR_MAX + 6, "--backup-if-mismatch", HasArgument::No },
    { CHAR_MAX + 7, "--no-backup-if-mismatch", HasArgument::No },
} };

} // namespace

CmdLine::CmdLine(int argc, const char* const* argv)
#ifndef _WIN32
    : m_argc(argc)
    , m_argv(argv)
#endif
{
#ifdef _WIN32
    (void)argc;
    (void)argv;

    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &m_argc);
    if (!wide_argv)
        throw std::bad_alloc();

    narrowed_argv_str.reserve(m_argc);
    narrowed_argv.reserve(m_argc);

    for (int i = 0; i < m_argc; ++i) {
        narrowed_argv_str.emplace_back(to_narrow(wide_argv[i]));
        narrowed_argv.emplace_back(narrowed_argv_str.back().c_str());
    }
    LocalFree(wide_argv);

    m_argv = narrowed_argv.data();
#endif
}

std::string CmdLineParser::consume_next_argument()
{
    const char* c = m_argv[i + 1];
    if (!c)
        throw cmdline_parse_error("option missing operand for " + std::string(m_argv[i]));
    ++i;
    return c;
}

int CmdLineParser::stoi(const std::string& str, const std::string& description)
{
    int value;
    size_t pos;

    try {
        value = std::stoi(str, &pos);
    } catch (const std::invalid_argument&) {
        throw cmdline_parse_error(description + " " + str + " is not a number");
    }

    if (pos != str.size())
        throw cmdline_parse_error(description + " " + str + " is not a number");

    return value;
}

void CmdLineParser::parse_operand()
{
    // We only support two positional arguments.
    if (m_positional_arguments_found == 2)
        throw cmdline_parse_error(std::string(m_argv[i]) + ": extra operand");

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

void CmdLineParser::handle_read_only(const std::string& handling)
{
    if (handling == "warn")
        m_options.read_only_handling = Options::ReadOnlyHandling::Warn;
    else if (handling == "ignore")
        m_options.read_only_handling = Options::ReadOnlyHandling::Ignore;
    else if (handling == "fail")
        m_options.read_only_handling = Options::ReadOnlyHandling::Fail;
    else
        throw cmdline_parse_error("unrecognized read-only handling " + handling);
}

void CmdLineParser::handle_reject_format(const std::string& format)
{
    if (format == "context")
        m_options.reject_format = Options::RejectFormat::Context;
    else if (format == "unified")
        m_options.reject_format = Options::RejectFormat::Unified;
    else
        throw cmdline_parse_error("unrecognized reject format " + format);
}

void CmdLineParser::parse_short_option(const std::string& option_string)
{
    for (auto it = option_string.begin() + 1; it != option_string.end(); ++it) {
        char c = *it;

        auto consume_argument = [&] {
            // At the end of the option, must be in next argv
            if (it + 1 == option_string.end())
                return consume_next_argument();

            // Still more in this option, must be until the end of this string.
            return std::string(it + 1, option_string.end());
        };

        bool should_continue = false;

        for (const auto& option : s_switches) {
            if (c != option.short_name)
                continue;

            if (option.has_argument == HasArgument::Yes) {
                process_option(option.short_name, consume_argument());
                return;
            }

            process_option(option.short_name, "");
            should_continue = true;
        }

        if (should_continue)
            continue;

        throw cmdline_parse_error("invalid option -- '" + std::string(1, c) + "'");
    }
}

void CmdLineParser::parse_long_option(const std::string& option_string)
{
    const auto equal_pos = option_string.find_first_of('=');
    const bool has_separator = equal_pos != std::string::npos;
    const auto& key = has_separator ? option_string.substr(0, equal_pos) : option_string;

    auto handle_option = [&](const Option& option) {
        // Found a matching option. If it doesn't have a boolean, just use that.
        if (option.has_argument == HasArgument::No) {
            if (has_separator)
                throw cmdline_parse_error("option '" + key + "' doesn't allow an argument");

            process_option(option.short_name, "");
        } else {
            const auto value = has_separator ? option_string.substr(equal_pos + 1) : consume_next_argument();
            process_option(option.short_name, value);
        }
    };

    for (const auto& option : s_switches) {
        if (option.long_name != key)
            continue;

        handle_option(option);
        return;
    }

    const Option* active_option = nullptr;

    // No exact match, try find a non ambiguous partial match with a long option.
    for (auto option_it = s_switches.begin(); option_it != s_switches.end(); ++option_it) {
        if (!starts_with(option_it->long_name, key))
            continue;

        // Found more than one option. Error out, and show all possible options which it could have been.
        if (active_option) {
            std::ostringstream ss;
            ss << "option '" << key << "' is ambiguous; possibilities: '" << active_option->long_name << '\'';
            for (; option_it != s_switches.end(); ++option_it) {
                if (starts_with(option_it->long_name, key))
                    ss << " '" << option_it->long_name << '\'';
            }

            throw cmdline_parse_error(ss.str());
        }

        active_option = &(*option_it);
    }

    // Still haven't found any partial match - error out this time.
    if (!active_option)
        throw cmdline_parse_error("unrecognized option '" + option_string + "'");

    handle_option(*active_option);
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
        std::string option_string = m_argv[i];

        bool is_long_option = option_string[1] == '-';

        if (is_long_option)
            parse_long_option(option_string);
        else
            parse_short_option(option_string);
    }

    return m_options;
}

void CmdLineParser::process_option(int short_name, const std::string& value)
{
    switch (short_name) {
    case 'B':
        m_options.backup_prefix = value;
        break;
    case 'D':
        m_options.define_macro = value;
        break;
    case 'F':
        m_options.max_fuzz = stoi(value, "fuzz factor");
        break;
    case 'N':
        m_options.ignore_reversed = true;
        break;
    case 'R':
        m_options.reverse_patch = true;
        break;
    case 'b':
        m_options.save_backup = true;
        break;
    case 'c':
        m_options.interpret_as_context = true;
        break;
    case 'd':
        m_options.patch_directory_path = value;
        break;
    case 'e':
        m_options.interpret_as_ed = true;
        break;
    case 'f':
        m_options.force = true;
        break;
    case 'h':
        m_options.show_help = true;
        break;
    case 'i':
        m_options.patch_file_path = value;
        break;
    case 'l':
        m_options.ignore_whitespace = true;
        break;
    case 'n':
        m_options.interpret_as_normal = true;
        break;
    case 'o':
        m_options.out_file_path = value;
        break;
    case 'p':
        m_options.strip_size = stoi(value, "strip count");
        break;
    case 'r':
        m_options.reject_file_path = value;
        break;
    case 't':
        m_options.batch = true;
        break;
    case 'u':
        m_options.interpret_as_unified = true;
        break;
    case 'v':
        m_options.show_version = true;
        break;
    case 'z':
        m_options.backup_suffix = value;
        break;
    case CHAR_MAX + 1:
        handle_newline_strategy(value);
        break;
    case CHAR_MAX + 2:
        handle_read_only(value);
        break;
    case CHAR_MAX + 3:
        handle_reject_format(value);
        break;
    case CHAR_MAX + 4:
        m_options.verbose = true;
        break;
    case CHAR_MAX + 5:
        m_options.dry_run = true;
        break;
    case CHAR_MAX + 6:
        m_options.backup_if_mismatch = true;
        break;
    case CHAR_MAX + 7:
        m_options.backup_if_mismatch = false;
        break;
    default:
        break;
    }
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
           "    -f, --force\n"
           "                Do not prompt for input, try to apply patch as given.\n"
           "\n"
           "    -t, --batch\n"
           "                Assume patches are reversed if a reversed patch is detected. Do not apply patch file\n"
           "                if content given by 'Prereq' is missing in the original file.\n"
           "\n"
           "    -b, --backup\n"
           "                Before writing to the patched file, make a backup of the file that will be written\n"
           "                to. The output file with be given the filename suffix '.orig'.\n"
           "\n"
           "    --backup-if-mismatch\n"
           "                Automatically make a backup of the file to be written to (as if given '--backup') if\n"
           "                it is determined that the patch will apply with an offset or fuzz factor. Defaults\n"
           "                to 'true'.\n"
           "\n"
           "    --no-backup-if-mismatch\n"
           "                Only apply a backup of the file to be written to if told to do so by the '--backup'\n"
           "                option even if the patch is determined to not apply perfectly.\n"
           "\n"
           "    -B, --prefix <prefix>\n"
           "                Add <prefix> to the beginning of backup file names.\n"
           "\n"
           "    -z, --suffix <suffix>\n"
           "                Add <suffix> to the end of backup file names.\n"
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
           "    --read-only <handling>\n"
           "                Change how to handle when the file being patched is read only. The default read-only behaviour\n"
           "                is to 'warn'. The possible values for this flag are:\n"
           "\n"
           "                    warn    Warn that the file is read-only, but proceed patching it anyway.\n"
           "                    ignore  Proceed patching without any warning issued.\n"
           "                    fail    Fail, and refuse patching the file.\n"
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
           "    -v, --version\n"
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
