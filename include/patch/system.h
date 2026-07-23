// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <system_error>
#include <vector>

namespace Patch {

FILE* create_temporary_file();

std::string generate_random_alphanumeric_string(std::size_t len);

// Exclusively create (O_CREAT | O_EXCL) a named file at path with the given
// creation permissions, returning an open read/write stream positioned at the
// start. Throws std::system_error with errc::file_exists if path already exists.
FILE* create_file_exclusively(const std::string& path, bool binary, unsigned permissions);

std::string read_tty_until_enter();

void chdir(const std::string& path);

std::string current_path();

void remove_file_and_empty_parent_folders(std::string path);

void ensure_parent_directories(const std::string& file_path);

namespace filesystem {

constexpr bool is_seperator(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

void symlink(const std::string& target, const std::string& linkpath);

// Non-throwing variant reporting failure through ec, for use in retry and
// cleanup paths.
void symlink(const std::string& target, const std::string& linkpath, std::error_code& ec) noexcept;

std::string basename(const std::string& path);

bool create_directory(const std::string& path);

std::string temp_directory_path();

std::string make_temp_directory();

bool exists(const std::string& path);

bool is_regular_file(const std::string& path);

bool is_symlink(const std::string& path);

void remove(const std::string& path);

// Best-effort removal that reports through an error code instead of throwing,
// for use on cleanup paths that must not mask a primary error.
void remove(const std::string& path, std::error_code& ec) noexcept;

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

inline bool is_symlink(uint32_t mode)
{
    constexpr uint32_t symlink_mode = 0120000;
    return (mode & symlink_mode) == symlink_mode;
}

inline perms operator&(perms left, perms right)
{
    return static_cast<perms>(static_cast<unsigned>(left) & static_cast<unsigned>(right));
}

inline perms operator|(perms left, perms right)
{
    return static_cast<perms>(static_cast<unsigned>(left) | static_cast<unsigned>(right));
}

inline perms operator^(perms left, perms right)
{
    return static_cast<perms>(static_cast<unsigned>(left) ^ static_cast<unsigned>(right));
}

inline perms& operator&=(perms& left, perms right)
{
    return left = left & right;
}

inline perms& operator|=(perms& left, perms right)
{
    return left = left | right;
}

inline perms& operator^=(perms& left, perms right)
{
    return left = left ^ right;
}

void permissions(const std::string& path, perms permissions);

perms get_permissions(const std::string& path);

// Descriptor-based variants that operate on an open stream, avoiding a second
// pathname lookup and the associated time-of-check/time-of-use race.
void permissions(FILE* file, perms permissions);

perms get_permissions(FILE* file);

struct file_metadata {
    perms permissions { perms::unknown };
    uint32_t owner { 0 };
    uint32_t group { 0 };
    // Ownership is only meaningful where the platform reports it (POSIX); it is
    // false on Windows and when the stat failed.
    bool has_ownership { false };
};

// Read mode and ownership from an open stream with a single fstat().
file_metadata get_metadata(FILE* file);

// Best-effort ownership preservation. Restoring owner/group generally requires
// privilege, so failures (typically EPERM) are reported through ec for the
// caller to ignore rather than thrown.
void set_ownership(FILE* file, const file_metadata& metadata, std::error_code& ec) noexcept;

uintmax_t file_size(FILE* file);

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
