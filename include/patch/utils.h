// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <string>

namespace Patch {

inline bool ends_with(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool starts_with(const std::string& str, const std::string& prefix)
{
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

constexpr bool is_octal(char c)
{
    return c >= '0' && c <= '7';
}

constexpr bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

constexpr bool is_not_digit(char c)
{
    return !is_digit(c);
}

constexpr bool is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

} // namespace Patch
