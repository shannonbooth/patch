// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <filesystem>
#include <patch/system.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#    include <direct.h>
#    include <io.h>
#    include <windows.h>
#    define close _close
#    define read _read
#    define open _open
#else
#    include <unistd.h>
#endif

namespace Patch {

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
        auto ret = ::read(fd, &buffer[0] + offset, buffer.size() - offset);
        if (ret < 0) {
            int saved_errno = errno;
            ::close(fd);
            throw std::system_error(saved_errno, std::generic_category(), "Reading from tty device failed");
        }

        // Finish if we didn't read up until the end of our buffer, indicating input has finished, or
        // if the last character given was an enter which means that the user has submitted their answer.
        if (buffer.size() - offset != static_cast<size_t>(ret) || buffer.back() == '\n') {
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

static bool remove_empty_directory(const std::filesystem::path& path)
{
#ifdef _WIN32
    int ret = ::_wrmdir(path.c_str());
#else
    int ret = ::rmdir(path.c_str());
#endif

    if (ret == 0)
        return true;

    // POSIX allows for either ENOTEMPTY or EEXIST.
    if (errno != ENOTEMPTY && errno != EEXIST)
        throw std::system_error(errno, std::generic_category(), "Unable to remove directory " + path.string());

    return false;
}

void remove_file_and_empty_parent_folders(const std::string& path)
{
    std::filesystem::path native_path(to_native(path));

    std::filesystem::remove(native_path);
    while (native_path.has_parent_path()) {
        native_path = native_path.parent_path();
        if (!remove_empty_directory(native_path))
            break;
    }
}

void ensure_parent_directories(const std::string& path)
{
    const std::filesystem::path native_path(to_native(path));

    if (native_path.has_parent_path())
        std::filesystem::create_directories(native_path.parent_path());
}

void chdir(const std::string& path)
{
#ifdef _WIN32
    const auto wpath = to_wide(path);
    int ret = ::_wchdir(wpath.c_str());
#else
    int ret = ::chdir(path.c_str());
#endif

    if (ret != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to change to directory " + path);
}

namespace filesystem {

bool exists(const std::string& path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(to_native(path).c_str(), GetFileExInfoStandard, &data))
        return true;

    switch (GetLastError()) {
    case ERROR_BAD_PATHNAME:
    case ERROR_FILE_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_INVALID_NAME:
    case ERROR_INVALID_PARAMETER:
    case ERROR_PATH_NOT_FOUND:
        return false;
    default:
        return true;
    }
#else
    struct stat buf;
    return ::stat(path.c_str(), &buf) == 0;
#endif
}

bool is_regular_file(const std::string& path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesW(to_native(path).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;

    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        return false;

    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return false;

    return true;
#else
    struct stat buf;
    return ::stat(path.c_str(), &buf) == 0 && S_ISREG(buf.st_mode);
#endif
}

void rename(const std::string& old_path, const std::string& new_path)
{
#ifdef _WIN32
    // rename on Windows does not follow Dr.POSIX and overwrite existing files, so we need to use MoveFileEx with MOVEFILE_REPLACE_EXISTING set.
    if (MoveFileExW(to_native(old_path).c_str(), to_native(new_path).c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Unable to rename " + old_path + " to " + new_path);
#else
    if (std::rename(old_path.c_str(), new_path.c_str()) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to rename " + old_path + " to " + new_path);
#endif
}

void permissions(const std::string& path, perms permissions)
{
    std::filesystem::permissions(to_native(path), permissions);
}

uintmax_t file_size(const std::string& path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;

    if (!GetFileAttributesExW(to_native(path).c_str(), GetFileExInfoStandard, &data))
        throw std::system_error(GetLastError(), std::system_category(), "Unable to determine file size for " + path);

    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        throw std::system_error(std::make_error_code(std::errc::is_a_directory), path + " is a directory, unable to determine file size");

    return static_cast<uintmax_t>(data.nFileSizeHigh) << 32 | data.nFileSizeLow;
#else
    struct stat buf;
    if (::stat(path.c_str(), &buf) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to determine file size for " + path);

    if (S_ISDIR(buf.st_mode))
        throw std::system_error(std::make_error_code(std::errc::is_a_directory), path + " is a directory, unable to determine file size");

    if (!S_ISREG(buf.st_mode))
        throw std::system_error(std::make_error_code(std::errc::not_supported), path + " is not a regular file, unable to determine file size");

    return buf.st_size;
#endif
}

} // namespace filesystem

#ifdef _WIN32

std::wstring to_wide(const std::string& str)
{
    if (str.empty())
        return {};

    int length = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (length == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Failed widening string");

    std::wstring wide_str;
    wide_str.resize(static_cast<size_t>(length));

    length = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wide_str[0], length);
    if (length == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Failed widening string");

    return wide_str;
}

std::string to_narrow(const std::wstring& str)
{
    if (str.empty())
        return {};

    int length = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    if (length == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Failed narrowing string");

    std::string narrow_str;
    narrow_str.resize(static_cast<size_t>(length));

    length = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &narrow_str[0], length, nullptr, nullptr);
    if (length == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Failed narrowing string");

    return narrow_str;
}

#endif
} // namespace Patch
