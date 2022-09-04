// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <filesystem>
#include <fstream>
#include <istream>
#include <string>
#include <vector>

namespace Patch {

enum class NewLine {
    LF,
    CRLF,
    None,
};

bool get_line(std::istream& is, std::string& line, NewLine* newline = nullptr);

std::string read_tty_until_enter();

void chdir(const std::string& path);

void remove_file_and_empty_parent_folders(const std::string& path);

void ensure_parent_directories(const std::string& path);

namespace filesystem {

bool exists(const std::string& path);

bool is_regular_file(const std::string& path);

void rename(const std::string& old_path, const std::string& new_path);

using std::filesystem::perms;

void permissions(const std::string& path, perms permissions);

uintmax_t file_size(const std::string& path);

} // namespace filesystem

#ifdef _WIN32

std::wstring to_wide(const std::string& str);

std::string to_narrow(const std::wstring& str);

inline std::wstring to_native(const std::string& str)
{
    return to_wide(str);
}

#else

inline std::string to_native(const std::string& str)
{
    return str;
}

#endif

} // namespace Patch
