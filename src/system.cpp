// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <patch/directory.h>
#include <patch/system.h>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <utility>

#ifdef _WIN32
#    include "windows_error.h"
#    include <direct.h>
#    include <io.h>
#    define close _close
#    define read _read
#    define open _open
#    define fdopen _fdopen
#else
#    include <unistd.h>
#endif

namespace Patch {

static std::mt19937 random_generator()
{
    constexpr auto seed_bytes = sizeof(std::mt19937::result_type) * std::mt19937::state_size;
    constexpr auto seed_len = seed_bytes / sizeof(std::seed_seq::result_type);

    auto seed = std::array<std::seed_seq::result_type, seed_len>();
    std::random_device dev;
    std::generate_n(std::begin(seed), seed_len, std::ref(dev));

    std::seed_seq seed_seq(std::begin(seed), std::end(seed));

    return std::mt19937(seed_seq);
}

static std::string generate_random_alphanumeric_string(std::size_t len)
{
    static const char* chars = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937 rng = random_generator();
    auto dist = std::uniform_int_distribution<size_t> { {}, std::strlen(chars) - 1 };
    auto result = std::string(len, '\0');
    std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
    return result;
}

std::string read_tty_until_enter()
{
    // NOTE: we need to read from /dev/tty and not stdin. This is for two reasons:
    //   1. POSIX says so (which should be enough reason)
    //   2. Asking for user input when the patch is read from stdin would not work.
#ifdef _WIN32
    int fd = ::open("CON", O_RDONLY);
#else
    int fd = ::open("/dev/tty", O_RDONLY);
#endif

    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Opening tty device failed");

    std::string buffer;
    buffer.resize(32);

    size_t offset = 0;

    while (true) {
        const auto available_size = buffer.size() - offset;
#ifdef _WIN32
        const auto read_size = std::min(available_size, static_cast<size_t>(INT_MAX));
        auto ret = ::read(fd, &buffer[0] + offset, static_cast<unsigned int>(read_size));
#else
        const auto read_size = available_size;
        auto ret = ::read(fd, &buffer[0] + offset, read_size);
#endif
        if (ret < 0) {
            int saved_errno = errno;
            ::close(fd);
            throw std::system_error(saved_errno, std::generic_category(), "Reading from tty device failed");
        }

        // Finish if we didn't read up until the end of our buffer, indicating input has finished, or
        // if the last character given was an enter which means that the user has submitted their answer.
        if (read_size != static_cast<size_t>(ret) || buffer.back() == '\n') {
            // Trim to size, any pop any trailing '\n' since that is not part of their answer.
            buffer.resize(offset + static_cast<size_t>(ret));
            if (!buffer.empty() && buffer.back() == '\n')
                buffer.pop_back();
            ::close(fd);
            return buffer;
        }

        offset = buffer.size();
        buffer.resize(buffer.size() * 2);
    }
}

static FILE* open_exclusive_file(const std::string& path, bool binary, filesystem::perms permissions, bool delete_on_close)
{
#ifdef _WIN32
    (void)permissions;
    const auto native_path = to_native(path);
    const int flags = _O_CREAT | _O_EXCL | _O_RDWR | _O_NOINHERIT
        | (binary ? _O_BINARY : _O_TEXT)
        | (delete_on_close ? _O_TEMPORARY : 0);
    const int fd = ::_wopen(native_path.c_str(), flags, _S_IREAD | _S_IWRITE);
#else
    const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, static_cast<mode_t>(permissions));
#endif
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Failed exclusively creating file " + path);

#ifndef _WIN32
    if (delete_on_close && ::unlink(path.c_str()) != 0) {
        const auto saved_errno = errno;
        ::close(fd);
        throw std::system_error(saved_errno, std::generic_category(), "Failed unlinking temporary file " + path);
    }
#endif

    FILE* stream = ::fdopen(fd, binary ? "wb+" : "w+");
    if (!stream) {
        const auto saved_errno = errno;
        ::close(fd);
        if (!delete_on_close) {
#ifdef _WIN32
            ::_wremove(native_path.c_str());
#else
            ::unlink(path.c_str());
#endif
        }
        throw std::system_error(saved_errno, std::generic_category(), "Failed opening exclusively created file " + path);
    }

    return stream;
}

FILE* create_temporary_file()
{
    constexpr int max_attempts = 256;
    const auto permissions = filesystem::perms::owner_read | filesystem::perms::owner_write;

    for (int i = 0; i < max_attempts; ++i) {
        auto path = filesystem::temp_directory_path() + "/patch-" + generate_random_alphanumeric_string(6);
        try {
            return open_exclusive_file(path, true, permissions, true);
        } catch (const std::system_error& error) {
            if (error.code() == std::errc::file_exists)
                continue;
            throw;
        }
    }

    throw std::system_error(EEXIST, std::generic_category(), "Failed creating temporary file");
}

TemporaryFile create_temporary_file_in(const ResolvedPath& destination, bool binary, filesystem::perms permissions)
{
    constexpr int max_attempts = 256;

    for (int i = 0; i < max_attempts; ++i) {
        auto temporary_name = ".patch-" + generate_random_alphanumeric_string(8);
        try {
            return { destination.create_exclusive_sibling(temporary_name, binary, permissions), std::move(temporary_name) };
        } catch (const std::system_error& error) {
            if (error.code() == std::errc::file_exists)
                continue;
            throw;
        }
    }

    throw std::system_error(EEXIST, std::generic_category(), "Failed creating temporary file for " + destination.path());
}

namespace filesystem {

bool is_absolute(const std::string& path)
{
    if (path.empty())
        return false;

    if (is_seperator(path[0]))
        return true;

#ifdef _WIN32
    // A drive letter, with a following separator.
    return path.size() >= 3 && path[1] == ':' && is_seperator(path[2]);
#else
    return false;
#endif
}

std::string temp_directory_path()
{
#ifdef _WIN32
    std::wstring result;
    result.resize(MAX_PATH);

    while (true) {
        const auto requested_size = static_cast<unsigned long>(result.size());

        const auto size = GetTempPathW(requested_size, &result[0]);

        if (size == 0)
            throw last_win32_error("Failed getting current directory");

        result.resize(size);
        if (size <= requested_size)
            return to_narrow(result);
    }
#else
    std::array<const char*, 4> env_vars { "TMPDIR", "TMP", "TEMP", "TEMPDIR" };
    for (const char* env : env_vars) {
        const char* maybe_temp_dir = std::getenv(env);
        if (maybe_temp_dir)
            return maybe_temp_dir;
    }
    // Fallback to /tmp if we couldn't find anything else.
    return "/tmp";
#endif
}

std::string basename(const std::string& path)
{
#ifdef _WIN32
    constexpr const char* seperator = "/\\";
#else
    constexpr char seperator = '/';
#endif

    const auto pos = path.find_last_of(seperator);

    // No slash - path is already basename.
    if (pos == std::string::npos)
        return path;

    // basename is the component of the path after the seperator
    return path.substr(pos + 1);
}

void permissions(FILE* file, perms permissions)
{
    if (permissions == perms::unknown)
        return;

#ifdef _WIN32
    const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
    if (handle == INVALID_HANDLE_VALUE)
        throw std::system_error(errno, std::generic_category(), "Unable to change permissions");

    FILE_BASIC_INFO info;
    if (GetFileInformationByHandleEx(handle, FileBasicInfo, &info, sizeof(info)) == 0)
        throw last_win32_error("Unable to change permissions");

    const auto write_permissions = perms::owner_write | perms::group_write | perms::others_write;
    if ((permissions & write_permissions) == perms::none)
        info.FileAttributes |= FILE_ATTRIBUTE_READONLY;
    else
        info.FileAttributes &= ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);

    if (SetFileInformationByHandle(handle, FileBasicInfo, &info, sizeof(info)) == 0)
        throw last_win32_error("Unable to change permissions");
#else
    if (::fchmod(fileno(file), static_cast<mode_t>(permissions)) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to change permissions");
#endif
}

perms get_permissions(FILE* file)
{
#ifdef _WIN32
    const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
    if (handle == INVALID_HANDLE_VALUE)
        throw std::system_error(errno, std::generic_category(), "Unable to get permissions");

    FILE_BASIC_INFO info;
    if (GetFileInformationByHandleEx(handle, FileBasicInfo, &info, sizeof(info)) == 0)
        throw last_win32_error("Unable to get permissions");

    perms permissions = perms::owner_read | perms::group_read | perms::others_read;
    if (!(info.FileAttributes & FILE_ATTRIBUTE_READONLY))
        permissions |= perms::owner_write | perms::group_write | perms::others_write;

    return permissions;
#else
    struct stat buf;
    if (::fstat(fileno(file), &buf) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to get permissions");

    return static_cast<perms>(buf.st_mode) & perms::mask;
#endif
}

uintmax_t file_size(FILE* file)
{
    struct stat buf;
    if (fstat(fileno(file), &buf) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to fstat file");

    if (buf.st_size < 0)
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "File has a negative size");

    return static_cast<uintmax_t>(buf.st_size);
}

} // namespace filesystem

#ifdef _WIN32

std::wstring to_wide(const std::string& str)
{
    if (str.empty())
        return {};

    int length = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (length == 0)
        throw last_win32_error("Failed widening string");

    std::wstring wide_str;
    wide_str.resize(static_cast<size_t>(length));

    length = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wide_str[0], length);
    if (length == 0)
        throw last_win32_error("Failed widening string");

    return wide_str;
}

std::string to_narrow(const std::wstring& str)
{
    if (str.empty())
        return {};

    int length = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    if (length == 0)
        throw last_win32_error("Failed narrowing string");

    std::string narrow_str;
    narrow_str.resize(static_cast<size_t>(length));

    length = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &narrow_str[0], length, nullptr, nullptr);
    if (length == 0)
        throw last_win32_error("Failed narrowing string");

    return narrow_str;
}

#endif
} // namespace Patch
