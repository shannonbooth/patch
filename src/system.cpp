// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

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
#include <patch/system.h>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

#ifdef _WIN32
#    include <direct.h>
#    include <io.h>
#    include <windows.h>
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

static bool remove_empty_directory(const std::string& path)
{
#ifdef _WIN32
    int ret = ::_wrmdir(to_native(path).c_str());
#else
    int ret = ::rmdir(path.c_str());
#endif

    if (ret == 0)
        return true;

    // POSIX allows for either ENOTEMPTY or EEXIST.
    if (errno != ENOTEMPTY && errno != EEXIST)
        throw std::system_error(errno, std::generic_category(), "Unable to remove directory " + path);

    return false;
}

void remove_file_and_empty_parent_folders(std::string path)
{
#ifdef _WIN32
    int ret = _wremove(to_native(path).c_str());
#else
    int ret = std::remove(path.c_str());
#endif

    if (ret != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to remove file " + path);

    while (true) {

        std::size_t i = path.find_last_of('/');
        if (i == std::string::npos)
            break;

        path = path.substr(0, i);

        if (path.empty())
            break;

        if (!remove_empty_directory(path))
            break;
    }
}

void ensure_parent_directories(const std::string& file_path)
{
    if (file_path.empty())
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Invalid path to create directories");

    std::size_t i = file_path.find_last_of('/');
    std::string path = file_path.substr(0, i + 1);

    // Move forwards through the path given, creating each individual directory.
    // Note that the below loop assumes that the path given has been canonicalized,
    // i.e, does not contain any '..' or similar components.
    size_t pos = 0;
    while ((pos = path.find_first_of('/', pos)) != std::string::npos) {
        auto dir = path.substr(0, pos++);

        if (dir.empty())
            continue;

        filesystem::create_directory(dir);
    }
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
        throw std::system_error(errno, std::generic_category(), "Can't change to directory " + path + " ");
}

std::string current_path()
{
#ifdef _WIN32
    std::wstring result;
    result.resize(MAX_PATH);

    while (true) {
        const auto requested_size = static_cast<unsigned long>(result.size());

        const auto size = GetCurrentDirectoryW(requested_size, &result[0]);

        if (size == 0)
            throw std::system_error(GetLastError(), std::system_category(), "Failed getting current directory");

        result.resize(size);
        if (size <= requested_size)
            return to_narrow(result);
    }
#else
    std::array<char, PATH_MAX> buf;

    if (!getcwd(buf.data(), buf.size()))
        throw std::system_error(errno, std::generic_category(), "Failed getting current directory");

    return std::string(buf.data());
#endif
}

FILE* create_temporary_file()
{
    constexpr int max_attempts = 256; // something very wrong if this fails.

    for (int i = 0; i < max_attempts; i++) {
        std::string tmpname = filesystem::temp_directory_path() + "/patch-" + generate_random_alphanumeric_string(6);
#ifdef _WIN32
        int fd = ::_open(tmpname.c_str(), _O_BINARY | _O_CREAT | _O_EXCL | _O_RDWR | _O_TEMPORARY, _S_IREAD | _S_IWRITE);
#else
        int fd = ::open(tmpname.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
#endif
        if (fd == -1)
            continue;

#ifndef _WIN32
        if (::unlink(tmpname.c_str()) != 0)
            throw std::system_error(errno, std::generic_category(), "Failed unlinking temporary file " + tmpname);
#endif

        FILE* fp = ::fdopen(fd, "wb+");
        if (!fp)
            throw std::system_error(errno, std::generic_category(), "Failed running fdopen to create temporary file");

        return fp;
    }

    // Ran out of attempts creating the file :(
    throw std::system_error(errno, std::generic_category(), "Failed creating temporary file");
}

namespace filesystem {

std::string temp_directory_path()
{
#ifdef _WIN32
    std::wstring result;
    result.resize(MAX_PATH);

    while (true) {
        const auto requested_size = static_cast<unsigned long>(result.size());

        const auto size = GetTempPathW(requested_size, &result[0]);

        if (size == 0)
            throw std::system_error(GetLastError(), std::system_category(), "Failed getting current directory");

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

std::string make_temp_directory()
{
#ifdef _WIN32
    std::wstring wpath = L"patch-XXXXXX";

    if (_wmktemp_s(&wpath[0], wpath.size() + 1) < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to create temporary file name");

    if (_wmkdir(wpath.c_str()) < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to make temporary directory");

    return to_narrow(wpath);
#else
    std::string path = "patch-XXXXXX";

    if (!mkdtemp(&path[0]))
        throw std::system_error(errno, std::generic_category(), "Unable to make temporary directory");

    return path;
#endif
}

void symlink(const std::string& target, const std::string& linkpath)
{
#ifdef _WIN32
    // FIXME: How do we implement this properly on Windows??
    throw std::system_error(ENOSYS, std::generic_category(), "Can't create symbolic link " + target + " ");
#else
    int ret = ::symlink(target.c_str(), linkpath.c_str());
    if (ret != 0)
        throw std::system_error(errno, std::generic_category(), "Can't create symbolic link " + target + " ");
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

bool create_directory(const std::string& path)
{
#ifdef _WIN32
    int ret = _wmkdir(to_native(path).c_str());
#else
    int ret = mkdir(path.c_str(), 0777);
#endif

    if (ret != 0) {
        if (errno == EEXIST)
            return false;

        throw std::system_error(errno, std::generic_category(), "Unable to remove file " + path);
    }

    return true;
}

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

bool is_symlink(const std::string& path)
{
#ifdef _WIN32
    // FIXME: support this
    return false;
#else
    struct stat buf;
    if (::lstat(path.c_str(), &buf) != 0)
        return false;

    return S_ISLNK(buf.st_mode);
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
    if (permissions == perms::unknown)
        return;

#ifdef _WIN32
    const auto native = to_native(path);

    DWORD attributes = GetFileAttributesW(native.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        throw std::system_error(GetLastError(), std::system_category(), "Unable to set permissions to " + path);

    // No group/owner/all on Windows - if any are set treat as write permissions.
    const auto write_perms = perms::owner_write | perms::group_write | perms::others_write;

    bool should_be_read_only = (permissions & write_perms) == perms::none;

    // If the file is already read only - there is nothing to do.
    if ((attributes & FILE_ATTRIBUTE_READONLY) && should_be_read_only)
        return;

    if (should_be_read_only)
        attributes |= FILE_ATTRIBUTE_READONLY;
    else
        attributes &= ~FILE_ATTRIBUTE_READONLY;

    if (SetFileAttributesW(native.c_str(), attributes) == 0)
        throw std::system_error(GetLastError(), std::system_category(), "Unable to set permissions to " + path);

#else
    if (::chmod(path.c_str(), static_cast<mode_t>(permissions)) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to change permissions for " + path);
#endif
}

perms get_permissions(const std::string& path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesW(to_native(path).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return perms::unknown;

    perms permissions = perms::owner_read | perms::group_read | perms::others_read;

    if (!(attributes & FILE_ATTRIBUTE_READONLY))
        permissions |= perms::owner_write | perms::group_write | perms::others_write;

    return permissions;
#else

    struct stat buf;
    if (::stat(path.c_str(), &buf) != 0)
        return perms::unknown;

    return static_cast<perms>(buf.st_mode) & perms::mask;
#endif
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

    return static_cast<uintmax_t>(buf.st_size);
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
