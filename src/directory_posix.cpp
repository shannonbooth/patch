// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <cerrno>
#include <fcntl.h>
#include <patch/directory.h>
#include <patch/system.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace Patch {

static int open_directory_path(const std::string& path)
{
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open directory " + path);
    return fd;
}

PathResolver::PathResolver(const std::string& root)
    : m_root(open_directory_path(root))
{
}

OpenDirectory PathResolver::open_absolute_root(const std::string& path, std::string& path_from_root)
{
    path_from_root = path;
    const int fd = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open filesystem root");
    return OpenDirectory(fd);
}

OpenDirectory::~OpenDirectory()
{
    if (m_fd >= 0)
        ::close(m_fd);
}

OpenDirectory::OpenDirectory(OpenDirectory&& other) noexcept
    : m_fd(other.m_fd)
{
    other.m_fd = -1;
}

OpenDirectory& OpenDirectory::operator=(OpenDirectory&& other) noexcept
{
    if (&other != this) {
        if (m_fd >= 0)
            ::close(m_fd);
        m_fd = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}

OpenDirectory OpenDirectory::duplicate() const
{
    const int fd = ::fcntl(m_fd, F_DUPFD_CLOEXEC, 0);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to duplicate directory handle");
    return OpenDirectory(fd);
}

OpenDirectory OpenDirectory::open_child(const std::string& name, bool create_missing) const
{
    require_component(name);
    if (create_missing)
        create_directory(name);

    const int fd = ::openat(m_fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open directory " + name);
    return OpenDirectory(fd);
}

bool OpenDirectory::read_link(const std::string& name, std::string& target) const
{
    require_component(name);
    std::string buffer(256, '\0');

    while (true) {
        const auto length = ::readlinkat(m_fd, name.c_str(), &buffer[0], buffer.size());
        if (length < 0)
            return false;

        if (static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(static_cast<std::size_t>(length));
            target = std::move(buffer);
            return true;
        }

        buffer.resize(buffer.size() * 2);
    }
}

FileType OpenDirectory::type_of(const std::string& name) const
{
    require_component(name);

    struct stat buffer;
    if (::fstatat(m_fd, name.c_str(), &buffer, AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno == ENOENT)
            return FileType::None;
        throw std::system_error(errno, std::generic_category(), "Unable to inspect " + name);
    }

    if (S_ISREG(buffer.st_mode))
        return FileType::Regular;
    if (S_ISDIR(buffer.st_mode))
        return FileType::Directory;
    if (S_ISLNK(buffer.st_mode))
        return FileType::Symlink;
    return FileType::Other;
}

filesystem::perms OpenDirectory::permissions_of(const std::string& name) const
{
    require_component(name);

    struct stat buffer;
    if (::fstatat(m_fd, name.c_str(), &buffer, AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno == ENOENT)
            return filesystem::perms::unknown;
        throw std::system_error(errno, std::generic_category(), "Unable to inspect permissions of " + name);
    }

    return static_cast<filesystem::perms>(buffer.st_mode) & filesystem::perms::mask;
}

static FILE* stream_for(int fd, const char* mode)
{
    FILE* stream = ::fdopen(fd, mode);
    if (!stream) {
        const auto saved_errno = errno;
        ::close(fd);
        errno = saved_errno;
    }
    return stream;
}

FILE* OpenDirectory::open_read(const std::string& name, bool binary) const
{
    require_component(name);

    const int fd = ::openat(m_fd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open " + name + " for reading");

    auto* stream = stream_for(fd, binary ? "rb" : "r");
    if (!stream)
        throw std::system_error(errno, std::generic_category(), "Unable to create a stream for " + name);
    return stream;
}

FILE* OpenDirectory::open_write(const std::string& name, bool binary) const
{
    require_component(name);

    const int fd = ::openat(m_fd, name.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0666);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open " + name + " for writing");

    auto* stream = stream_for(fd, binary ? "wb" : "w");
    if (!stream)
        throw std::system_error(errno, std::generic_category(), "Unable to create a stream for " + name);
    return stream;
}

FILE* OpenDirectory::create_exclusive(const std::string& name, bool binary, filesystem::perms permissions) const
{
    require_component(name);

    const int fd = ::openat(m_fd, name.c_str(), O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, static_cast<mode_t>(permissions));
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Failed exclusively creating file " + name);

    FILE* stream = stream_for(fd, binary ? "wb+" : "w+");
    if (!stream) {
        const auto saved_errno = errno;
        ::unlinkat(m_fd, name.c_str(), 0);
        throw std::system_error(saved_errno, std::generic_category(), "Failed opening exclusively created file " + name);
    }

    return stream;
}

bool OpenDirectory::create_directory(const std::string& name) const
{
    require_component(name);
    if (::mkdirat(m_fd, name.c_str(), 0777) == 0)
        return true;
    if (errno == EEXIST)
        return false;
    throw std::system_error(errno, std::generic_category(), "Unable to create directory " + name);
}

void OpenDirectory::set_permissions(const std::string& name, filesystem::perms permissions) const
{
    require_component(name);
    if (permissions == filesystem::perms::unknown)
        return;

    const int fd = ::openat(m_fd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open " + name + " for changing permissions");
    if (::fchmod(fd, static_cast<mode_t>(permissions)) == 0) {
        ::close(fd);
        return;
    }

    const auto error = errno;
    ::close(fd);
    throw std::system_error(error, std::generic_category(), "Unable to change permissions of " + name);
}

void OpenDirectory::rename_within(const std::string& from, const std::string& to) const
{
    require_component(from);
    require_component(to);

    if (::renameat(m_fd, from.c_str(), m_fd, to.c_str()) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to rename " + from + " to " + to);
}

void OpenDirectory::remove(const std::string& name) const
{
    require_component(name);

    if (::unlinkat(m_fd, name.c_str(), 0) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to remove file " + name);
}

void OpenDirectory::create_symlink(const std::string& target, const std::string& name) const
{
    require_component(name);

    if (::symlinkat(target.c_str(), m_fd, name.c_str()) != 0)
        throw std::system_error(errno, std::generic_category(), "Can't create symbolic link " + target + " ");
}

bool OpenDirectory::remove_if_empty(const std::string& name) const
{
    require_component(name);

    return ::unlinkat(m_fd, name.c_str(), AT_REMOVEDIR) == 0;
}

OpenDirectory OpenDirectory::open_parent() const
{
    const int parent = ::openat(m_fd, "..", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parent < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open parent directory");
    return OpenDirectory(parent);
}

} // namespace Patch
