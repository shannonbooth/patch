// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cstring>
#include <patch/cmdline.h>
#include <patch/system.h>
#include <patch/utils.h>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace Patch {

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

void CmdLineParser::parse_short_option(Handler& handler, const std::string& option_string)
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

        for (const auto& option : handler.switches()) {
            if (c != option.short_name)
                continue;

            if (option.has_argument == HasArgument::Yes) {
                handler.process_option(option.short_name, consume_argument());
                return;
            }

            handler.process_option(option.short_name);
            should_continue = true;
        }

        if (should_continue)
            continue;

        throw cmdline_parse_error("invalid option -- '" + std::string(1, c) + "'");
    }
}

void CmdLineParser::parse_long_option(Handler& handler, const std::string& option_string)
{
    const auto equal_pos = option_string.find_first_of('=');
    const bool has_separator = equal_pos != std::string::npos;
    const auto& key = has_separator ? option_string.substr(0, equal_pos) : option_string;

    auto handle_option = [&](const Option& option) {
        // Found a matching option. If it doesn't have a boolean, just use that.
        if (option.has_argument == HasArgument::No) {
            if (has_separator)
                throw cmdline_parse_error("option '" + key + "' doesn't allow an argument");

            handler.process_option(option.short_name);
        } else {
            const auto value = has_separator ? option_string.substr(equal_pos + 1) : consume_next_argument();
            handler.process_option(option.short_name, value);
        }
    };

    for (const auto& option : handler.switches()) {
        if (option.long_name != key)
            continue;

        handle_option(option);
        return;
    }

    const Option* active_option = nullptr;

    // No exact match, try find a non ambiguous partial match with a long option.
    for (auto option_it = handler.switches().begin(); option_it != handler.switches().end(); ++option_it) {
        if (!starts_with(option_it->long_name, key))
            continue;

        // Found more than one option. Error out, and show all possible options which it could have been.
        if (active_option) {
            std::ostringstream ss;
            ss << "option '" << key << "' is ambiguous; possibilities: '" << active_option->long_name << '\'';
            for (; option_it != handler.switches().end(); ++option_it) {
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

void CmdLineParser::parse(Handler& handler)
{
    for (; i < m_argc; ++i) {

        // If the option does not start with '-' it must be a positional argument.
        // However, there is a special case for '-' which denotes reading from stdin.
        //
        // NOTE: this behaviour deviates from POSIX which expects all option arguments
        //       to be given before the operands.
        if (*m_argv[i] != '-' || std::strcmp(m_argv[i], "-") == 0) {
            handler.process_option('?', m_argv[i]);
            continue;
        }

        // If the arg given is "--" then all arguments afterwards are interpreted as operands.
        if (std::strcmp(m_argv[i], "--") == 0) {
            ++i;
            for (; i < m_argc; ++i)
                handler.process_option('?', m_argv[i]);
            break;
        }

        // By this stage, we know that this arg is some option.
        // Now we need to work out which one it is.
        std::string option_string = m_argv[i];

        bool is_long_option = option_string[1] == '-';

        if (is_long_option) {
            parse_long_option(handler, option_string);
        } else {
            parse_short_option(handler, option_string);
            if (m_short_option_offset != 1)
                --i;
        }
    }
}

} // namespace Patch
