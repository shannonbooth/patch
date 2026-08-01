// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <string>
#include <system_error>
#include <windows.h>

namespace Patch {

inline std::error_code win32_error_code(DWORD error)
{
    return { static_cast<int>(error), std::system_category() };
}

class win32_error : public std::system_error {
public:
    win32_error(DWORD error, const std::string& message)
        : std::system_error(win32_error_code(error), message)
    {
    }
};

class last_win32_error : public win32_error {
public:
    explicit last_win32_error(const char* message)
        : win32_error(GetLastError(), message)
    {
    }
};

} // namespace Patch
