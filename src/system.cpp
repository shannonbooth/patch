// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

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

bool get_line(std::istream& is, std::string& line, NewLine* newline)
{
    line.clear();

    if (!is) {
        if (newline)
            *newline = NewLine::None;
        return false;
    }

    std::getline(is, line);
    bool have_data = !line.empty() || !is.eof();
    if (!line.empty()) {
        if (line.back() == '\r') {
            line.pop_back();
            if (newline)
                *newline = NewLine::CRLF;
        } else {
            if (newline)
                *newline = is.eof() ? NewLine::None : NewLine::LF;
        }
    } else {
        if (newline)
            *newline = is.eof() ? NewLine::None : NewLine::LF;
    }

    return have_data;
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

bool remove_empty_directory(const std::filesystem::path& path)
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
        throw std::system_error(errno, std::generic_category(), "Unable to remove directory " + path.u8string());

    return false;
}

void chdir(const std::filesystem::path& path)
{
#ifdef _WIN32
    int ret = ::_wchdir(path.c_str());
#else
    int ret = ::chdir(path.c_str());
#endif

    if (ret != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to change to directory " + path.u8string());
}

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
