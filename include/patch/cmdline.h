// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <array>
#include <ostream>
#include <patch/options.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace Patch {

class cmdline_parse_error : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class CmdLine {
public:
    CmdLine(int argc, const char* const* argv);

private:
    friend class CmdLineParser;

    int m_argc;
    const char* const* m_argv;

#ifdef _WIN32
    std::vector<std::string> narrowed_argv_str;
    std::vector<const char*> narrowed_argv;
#endif
};

class CmdLineParser {
public:
    explicit CmdLineParser(const CmdLine& cmdline)
        : m_argc(cmdline.m_argc)
        , m_argv(cmdline.m_argv)
    {
    }

    CmdLineParser(int argc, const char* const* argv)
        : m_argc(argc)
        , m_argv(argv)
    {
    }

    const Options& parse();

private:
    bool parse_string(char short_opt, const char* long_opt, std::string& option);
    bool parse_int(char short_opt, const char* long_opt, int& option);
    static int stoi(const std::string& option, const char* long_name);

    void parse_long_bool(const std::string& option);
    void parse_short_bool(const std::string& option);
    void parse_operand();
    void handle_newline_strategy(const std::string& strategy);
    void handle_read_only(const std::string& handling);
    void handle_reject_format(const std::string& format);

    int i { 1 };
    int m_argc { 0 };
    int m_positional_arguments_found { 0 };
    std::string m_buffer;
    const char* const* m_argv { nullptr };
    Options m_options;

    struct BoolOption {
        char short_name;
        const char* long_name;
        bool& enabled;
    };

    std::array<BoolOption, 12> m_bool_options { {
        { 'b', "--backup", m_options.save_backup },
        { 'c', "--context", m_options.interpret_as_context },
        { 'e', "--ed", m_options.interpret_as_ed },
        { 'l', "--ignore-whitespace", m_options.ignore_whitespace },
        { 'n', "--normal", m_options.interpret_as_normal },
        { 'N', "--forward", m_options.ignore_reversed },
        { 'R', "--reverse", m_options.reverse_patch },
        { 'f', "--force", m_options.force },
        { 'h', "--help", m_options.show_help },
        { 'v', "--version", m_options.show_version },
        { '\0', "--verbose", m_options.verbose },
        { '\0', "--dry-run", m_options.dry_run },
    } };
};

void show_usage(std::ostream& out);
void show_version(std::ostream& out);

} // namespace Patch
