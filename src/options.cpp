// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <climits>
#include <patch/options.h>

namespace Patch {

const std::vector<CmdLineParser::Option> s_switches { {
    { 'B', "--prefix", CmdLineParser::HasArgument::Yes },
    { 'D', "--ifdef", CmdLineParser::HasArgument::Yes },
    { 'E', "--remove-empty-files", CmdLineParser::HasArgument::No },
    { 'F', "--fuzz", CmdLineParser::HasArgument::Yes },
    { 'N', "--forward", CmdLineParser::HasArgument::No },
    { 'R', "--reverse", CmdLineParser::HasArgument::No },
    { 'b', "--backup", CmdLineParser::HasArgument::No },
    { 'c', "--context", CmdLineParser::HasArgument::No },
    { 'd', "--directory", CmdLineParser::HasArgument::Yes },
    { 'e', "--ed", CmdLineParser::HasArgument::No },
    { 'f', "--force", CmdLineParser::HasArgument::No },
    { 'h', "--help", CmdLineParser::HasArgument::No },
    { 'i', "--input", CmdLineParser::HasArgument::Yes },
    { 'l', "--ignore-whitespace", CmdLineParser::HasArgument::No },
    { 'n', "--normal", CmdLineParser::HasArgument::No },
    { 'o', "--output", CmdLineParser::HasArgument::Yes },
    { 'p', "--strip", CmdLineParser::HasArgument::Yes },
    { 'r', "--reject-file", CmdLineParser::HasArgument::Yes },
    { 't', "--batch", CmdLineParser::HasArgument::No },
    { 'u', "--unified", CmdLineParser::HasArgument::No },
    { 'v', "--version", CmdLineParser::HasArgument::No },
    { 'z', "--suffix", CmdLineParser::HasArgument::Yes },
    { CHAR_MAX + 1, "--newline-output", CmdLineParser::HasArgument::Yes },
    { CHAR_MAX + 2, "--read-only", CmdLineParser::HasArgument::Yes },
    { CHAR_MAX + 3, "--reject-format", CmdLineParser::HasArgument::Yes },
    { CHAR_MAX + 4, "--verbose", CmdLineParser::HasArgument::No },
    { CHAR_MAX + 5, "--dry-run", CmdLineParser::HasArgument::No },
    { CHAR_MAX + 6, "--backup-if-mismatch", CmdLineParser::HasArgument::No },
    { CHAR_MAX + 7, "--no-backup-if-mismatch", CmdLineParser::HasArgument::No },
    { CHAR_MAX + 8, "--posix", CmdLineParser::HasArgument::No },
    { CHAR_MAX + 9, "--quoting-style", CmdLineParser::HasArgument::Yes },
} };

OptionHandler::OptionHandler()
    : Handler(s_switches)
{
}

void OptionHandler::process_option(int short_name, const std::string& option)
{
    switch (short_name) {
    case 'B':
        m_options.backup_prefix = option;
        break;
    case 'D':
        m_options.define_macro = option;
        break;
    case 'E':
        m_options.remove_empty_files = Options::OptionalBool::Yes;
        break;
    case 'F':
        m_options.max_fuzz = stoi(option, "fuzz factor");
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
        m_options.patch_directory_path = option;
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
        m_options.patch_file_path = option;
        break;
    case 'l':
        m_options.ignore_whitespace = true;
        break;
    case 'n':
        m_options.interpret_as_normal = true;
        break;
    case 'o':
        m_options.out_file_path = option;
        break;
    case 'p':
        m_options.strip_size = stoi(option, "strip count");
        break;
    case 'r':
        m_options.reject_file_path = option;
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
        m_options.backup_suffix = option;
        break;
    case CHAR_MAX + 1:
        handle_newline_strategy(option);
        break;
    case CHAR_MAX + 2:
        handle_read_only(option);
        break;
    case CHAR_MAX + 3:
        handle_reject_format(option);
        break;
    case CHAR_MAX + 4:
        m_options.verbose = true;
        break;
    case CHAR_MAX + 5:
        m_options.dry_run = true;
        break;
    case CHAR_MAX + 6:
        m_options.backup_if_mismatch = Options::OptionalBool::Yes;
        break;
    case CHAR_MAX + 7:
        m_options.backup_if_mismatch = Options::OptionalBool::No;
        break;
    case CHAR_MAX + 8:
        m_options.posix = true;
        break;
    case CHAR_MAX + 9:
        handle_quoting_style(option);
        break;
    default:
        process_operand(option);
        break;
    }
}

int OptionHandler::stoi(const std::string& str, const std::string& description)
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

void OptionHandler::process_operand(const std::string& value)
{
    // We only support two positional arguments.
    if (m_positional_arguments_found == 2)
        throw cmdline_parse_error(value + ": extra operand");

    if (m_positional_arguments_found == 0)
        m_options.file_to_patch = value;
    else
        m_options.patch_file_path = value;

    ++m_positional_arguments_found;
}

void OptionHandler::handle_newline_strategy(const std::string& strategy)
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

void OptionHandler::handle_read_only(const std::string& handling)
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

void OptionHandler::handle_reject_format(const std::string& format)
{
    if (format == "context")
        m_options.reject_format = Options::RejectFormat::Context;
    else if (format == "unified")
        m_options.reject_format = Options::RejectFormat::Unified;
    else
        throw cmdline_parse_error("unrecognized reject format " + format);
}

void OptionHandler::handle_quoting_style(const std::string& style, const Options::QuotingStyle* default_quote_style)
{
    if (style == "literal")
        m_options.quoting_style = Options::QuotingStyle::Literal;
    else if (style == "shell")
        m_options.quoting_style = Options::QuotingStyle::Shell;
    else if (style == "shell-always")
        m_options.quoting_style = Options::QuotingStyle::ShellAlways;
    else if (style == "c")
        m_options.quoting_style = Options::QuotingStyle::C;
    else if (default_quote_style)
        m_options.quoting_style = *default_quote_style;
    else
        throw cmdline_parse_error("unrecognized quoting style " + style);
}

void OptionHandler::apply_posix_defaults()
{
    auto false_if_posix = [this](Options::OptionalBool& option) {
        if (option == Options::OptionalBool::Unset) {
            if (m_options.posix)
                option = Options::OptionalBool::No;
            else
                option = Options::OptionalBool::Yes;
        }
    };

    // POSIX does not backup files on mismatch or remove empty files.
    false_if_posix(m_options.backup_if_mismatch);
    false_if_posix(m_options.remove_empty_files);
}

void OptionHandler::apply_environment_defaults()
{
    if (!m_options.posix)
        m_options.posix = std::getenv("POSIXLY_CORRECT") != nullptr;

    if (m_options.quoting_style == Options::QuotingStyle::Unset) {
        const auto default_quote_style = Options::QuotingStyle::Shell;
        const char* style = std::getenv("QUOTING_STYLE");
        if (style)
            handle_quoting_style(style, &default_quote_style);
        else
            m_options.quoting_style = default_quote_style;
    }
}

void show_version(std::ostream& out)
{
    out << "patch 0.0.1\n"
           "Copyright (C) 2022 Shannon Booth\n";
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
           "                to 'true', unless the '--posix' option is set.\n"
           "\n"
           "    -E, --remove-empty-files\n"
           "                Empty files after patching are removed. Defaults to true unless '--posix' is set.\n"
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
           "    --posix\n"
           "                Change behavior to align with the POSIX standard.\n"
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
           "    --quoting-style <style>\n"
           "                Change how output file names are quoted. The default style is shell. The possible values for\n"
           "                this flag are:\n"
           "\n"
           "                    literal       Do not quote file names, display as is.\n"
           "                    shell         Quote the file name if it contains special shell characters, and escape them.\n"
           "                    shell-always  As 'shell' above, but always quote file names.\n"
           "                    c             Quote the string following the rules of the C programming langnuage.\n"
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
