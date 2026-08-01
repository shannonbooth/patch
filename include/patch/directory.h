// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <cstdio>
#include <patch/system.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace Patch {

class PathResolver;
class ResolvedPath;

enum class FileType {
    None,
    Regular,
    Directory,
    Symlink,
    Other,
};

// Where a path came from. A name taken from the patch may not leave the
// directory being patched; a name the user gave is theirs to choose.
enum class PathOrigin {
    Patch,
    User,
};

// A patch-derived path could not be resolved without traversing outside the
// directory being patched, or without following an unsafe link.
class UnsafePatchPath : public std::system_error {
public:
    explicit UnsafePatchPath(std::string path)
        : std::system_error(std::make_error_code(std::errc::too_many_symbolic_link_levels), "Invalid file name " + path)
        , m_path(std::move(path))
    {
    }

    const std::string& path() const { return m_path; }

private:
    std::string m_path;
};

// A directory capability used for component-relative operations.
class OpenDirectory {
public:
    ~OpenDirectory();

    OpenDirectory(const OpenDirectory&) = delete;
    OpenDirectory& operator=(const OpenDirectory&) = delete;

    OpenDirectory(OpenDirectory&& other) noexcept;
    OpenDirectory& operator=(OpenDirectory&& other) noexcept;

private:
    friend class PathResolver;
    friend class ResolvedPath;

    OpenDirectory duplicate() const;
    OpenDirectory open_child(const std::string& name, bool create_missing) const;
    bool read_link(const std::string& name, std::string& target) const;

    // Reject anything other than one path component.
    static void require_component(const std::string& name);

    FileType type_of(const std::string& name) const;

    filesystem::perms permissions_of(const std::string& name) const;

    FILE* open_read(const std::string& name, bool binary) const;
    FILE* open_write(const std::string& name, bool binary) const;
    FILE* create_exclusive(const std::string& name, bool binary, filesystem::perms permissions) const;
    bool create_directory(const std::string& name) const;
    void set_permissions(const std::string& name, filesystem::perms permissions) const;

    void rename_within(const std::string& from, const std::string& to) const;

    void remove(const std::string& name) const;

    void create_symlink(const std::string& target, const std::string& name) const;

    // Remove an empty child directory, returning false when it is not empty or cannot be removed.
    bool remove_if_empty(const std::string& name) const;

    OpenDirectory open_parent() const;

#ifdef _WIN32
    explicit OpenDirectory(void* handle);

    // Windows records whether a link names a directory when the link is
    // created, so the target has to be inspected before creating one.
    bool names_directory(const std::string& target) const;

    void* m_handle { nullptr };
#else
    explicit OpenDirectory(int fd)
        : m_fd(fd)
    {
    }

    int m_fd { -1 };
#endif
};

// A path pinned to the directory which holds its final component. Operations on
// the path do not resolve its original spelling again.
class ResolvedPath {
public:
    ResolvedPath(const ResolvedPath&) = delete;
    ResolvedPath& operator=(const ResolvedPath&) = delete;
    ResolvedPath(ResolvedPath&&) noexcept = default;
    ResolvedPath& operator=(ResolvedPath&&) noexcept = default;

    const std::string& name() const { return m_name; }
    const std::string& path() const { return m_path; }

    FileType type() const;
    filesystem::perms permissions() const;

    FILE* open_read(bool binary) const;
    FILE* open_write(bool binary) const;

    void create_symlink(const std::string& target) const;
    bool create_directory() const;
    void set_permissions(filesystem::perms permissions) const;
    void remove();
    void remove_and_empty_parents();

    // 'sibling' must be exactly one path component.
    FILE* create_exclusive_sibling(const std::string& sibling, bool binary, filesystem::perms permissions) const;
    void remove_sibling(const std::string& sibling) const;
    void replace_with_sibling(const std::string& sibling) const;

    ResolvedPath with_suffix(const std::string& suffix) const;

private:
    friend class PathResolver;

    struct Ancestor {
        Ancestor(OpenDirectory directory, std::string name)
            : directory(std::move(directory))
            , name(std::move(name))
        {
        }

        Ancestor(const Ancestor&) = delete;
        Ancestor& operator=(const Ancestor&) = delete;
        Ancestor(Ancestor&&) noexcept = default;
        Ancestor& operator=(Ancestor&&) noexcept = default;

        OpenDirectory directory;
        std::string name;
    };

    ResolvedPath(OpenDirectory directory, std::string name, std::string path, std::vector<Ancestor> ancestors, bool can_prune)
        : m_directory(std::move(directory))
        , m_name(std::move(name))
        , m_path(std::move(path))
        , m_ancestors(std::move(ancestors))
        , m_can_prune(can_prune)
    {
    }

    OpenDirectory m_directory;
    std::string m_name;
    std::string m_path;
    std::vector<Ancestor> m_ancestors;
    bool m_can_prune { false };
};

// Captures the directory being patched and resolves paths relative to that
// pinned directory. Patch paths may not leave it; user paths use normal path
// semantics and may be absolute or point outside it.
class PathResolver {
public:
    explicit PathResolver(const std::string& root);

    ResolvedPath resolve(const std::string& path, PathOrigin origin, bool create_missing = false) const;

private:
    static OpenDirectory open_absolute_root(const std::string& path, std::string& path_from_root);

    OpenDirectory m_root;
};

} // namespace Patch
