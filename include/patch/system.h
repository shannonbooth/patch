// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <string>
#include <vector>

namespace Patch {

std::string read_tty_until_enter();

void chdir(const std::string& path);

void remove_file_and_empty_parent_folders(std::string path);

void ensure_parent_directories(const std::string& file_path);

namespace filesystem {

bool exists(const std::string& path);

bool is_regular_file(const std::string& path);

void rename(const std::string& old_path, const std::string& new_path);

enum class perms : unsigned {
    none = 0,
    owner_read = 0400,
    owner_write = 0200,
    owner_exec = 0100,
    owner_all = 0700,
    group_read = 040,
    group_write = 020,
    group_exec = 010,
    group_all = 070,
    others_read = 04,
    others_write = 02,
    others_exec = 01,
    others_all = 07,
    all = 0777,
    set_uid = 04000,
    set_gid = 02000,
    sticky_bit = 01000,
    mask = 07777,
    unknown = 0xFFFF,
};

inline perms operator&(perms left, perms right)
{
    return static_cast<perms>(static_cast<unsigned>(left) & static_cast<unsigned>(right));
}

inline perms operator|(perms left, perms right)
{
    return static_cast<perms>(static_cast<unsigned>(left) | static_cast<unsigned>(right));
}

inline perms& operator&=(perms& left, perms right)
{
    return left = left & right;
}

inline perms& operator|=(perms& left, perms right)
{
    return left = left | right;
}

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
