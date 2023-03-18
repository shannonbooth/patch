// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

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
    void apply_posix_defaults();
    void apply_environment_defaults();

    static int stoi(const std::string& option, const std::string& description);

    void parse_long_option(const std::string& option);
    void parse_short_option(const std::string& option);
    void parse_operand();
    void handle_newline_strategy(const std::string& strategy);
    void handle_read_only(const std::string& handling);
    void handle_reject_format(const std::string& format);
    void handle_quoting_style(const std::string& style, const Options::QuotingStyle* default_quote_style = nullptr);

    void process_option(int short_name, const std::string& value);

    std::string consume_next_argument();

    int i { 1 };
    int m_argc { 0 };
    int m_positional_arguments_found { 0 };
    std::string m_buffer;
    const char* const* m_argv { nullptr };
    Options m_options;
};

void show_usage(std::ostream& out);
void show_version(std::ostream& out);

} // namespace Patch
