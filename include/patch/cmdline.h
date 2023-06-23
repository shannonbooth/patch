// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <ostream>
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

    enum class HasArgument {
        Yes,
        No,
    };

    struct Option {
        int short_name;
        const char* long_name;
        HasArgument has_argument;
    };

    class Handler {
    public:
        explicit Handler(const std::vector<Option>& switches)
            : m_switches(switches)
        {
        }

        virtual void process_option(int short_name, const std::string& option = "") = 0;

        const std::vector<Option>& switches() const { return m_switches; }

    protected:
        const std::vector<Option>& m_switches;
    };

    void parse(Handler& handler);

private:
    void parse_long_option(Handler& handler, const std::string& option);
    void parse_short_option(Handler& handler, const std::string& option);

    std::string consume_next_argument();

    int i { 1 };
    int m_argc { 0 };
    int m_short_option_offset { 1 };
    const char* const* m_argv { nullptr };
};

} // namespace Patch
