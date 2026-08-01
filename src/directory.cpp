// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <cstddef>
#include <deque>
#include <patch/directory.h>
#include <patch/system.h>
#include <system_error>
#include <utility>
#include <vector>

namespace Patch {

static std::vector<std::string> path_components(const std::string& path)
{
    std::vector<std::string> components;
    std::size_t begin = 0;

    while (begin < path.size()) {
        std::size_t end = begin;
        while (end < path.size() && !filesystem::is_seperator(path[end]))
            ++end;

        // A repeated separator gives an empty component, and '.' names the
        // directory which is already being resolved from.
        if (end != begin && path.compare(begin, end - begin, ".") != 0)
            components.emplace_back(path, begin, end - begin);

        begin = end + 1;
    }

    return components;
}

#ifdef _WIN32
// A reserved DOS device name refers to the device rather than a file. The comparison
// ignores case, anything from the first period on, and spaces before it, so "NUL.txt"
// and "nul .txt" both still name the device.
static bool is_reserved_device_name(const std::string& name)
{
    auto base = name.substr(0, name.find('.'));
    while (!base.empty() && base.back() == ' ')
        base.pop_back();

    for (auto& character : base) {
        if (character >= 'a' && character <= 'z')
            character = static_cast<char>(character - 'a' + 'A');
    }

    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL")
        return true;

    if (base.size() < 4 || (base.compare(0, 3, "COM") != 0 && base.compare(0, 3, "LPT") != 0))
        return false;

    const auto device = base.substr(3);
    if (device.size() == 1 && device[0] >= '0' && device[0] <= '9')
        return true;

    // A superscript digit also names the device: "COM¹" is COM1.
    return device == "\xC2\xB9" || device == "\xC2\xB2" || device == "\xC2\xB3";
}
#endif

void OpenDirectory::require_component(const std::string& name)
{
    const auto refuse = [&name] {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Invalid path component " + name);
    };

    if (name.empty() || name == "." || name == "..")
        refuse();

    for (const auto character : name) {
        // A separator would undo the resolution done so far. A name is carried
        // in a std::string, which can hold a NUL that the system call it is
        // handed would silently truncate at, leaving a different name resolved
        // than the one checked.
        if (filesystem::is_seperator(character) || character == '\0')
            refuse();

#ifdef _WIN32
        // A colon separates a file from an alternate data stream, so "C:foo" would open
        // a stream of a file named C rather than a file named "C:foo". The rest are names
        // Win32 refuses but the native API hands to the filesystem, which can hold names
        // ordinary Windows tools cannot open, normalize away, or reserve.
        if (character == ':' || character == '<' || character == '>' || character == '"'
            || character == '|' || character == '?' || character == '*'
            || static_cast<unsigned char>(character) < 0x20)
            refuse();
#endif
    }

#ifdef _WIN32
    // Win32 strips a trailing space or period, so a file made with one is not
    // reachable by the name later tools spell; a DOS device name is never an
    // ordinary file, even written with an extension.
    if (name.back() == ' ' || name.back() == '.' || is_reserved_device_name(name))
        refuse();
#endif
}

ResolvedPath PathResolver::resolve(const std::string& path, PathOrigin origin, bool create_missing) const
{
    const bool contained = origin == PathOrigin::Patch;

    const bool absolute = filesystem::is_absolute(path);
    if (contained && absolute)
        throw UnsafePatchPath(path);

    std::string path_from_root = path;
    auto current = absolute ? open_absolute_root(path, path_from_root) : m_root.duplicate();

    auto components = path_components(path_from_root);
    if (components.empty())
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "No file name in " + path);

    auto name = std::move(components.back());
    components.pop_back();

    // An invalid component is a bad name rather than an escape, but for a patch
    // path it must still be reported as one so that the patch is skipped and
    // the rest of the input is applied.
    const auto require_component = [&](const std::string& component) {
        try {
            OpenDirectory::require_component(component);
        } catch (const std::system_error&) {
            if (contained)
                throw UnsafePatchPath(path);
            throw;
        }
    };

    require_component(name);

    std::vector<ResolvedPath::Ancestor> ancestors;
    std::deque<std::string> pending(components.begin(), components.end());
    int remaining_links = 40;
    bool can_prune = true;

    while (!pending.empty()) {
        auto component = std::move(pending.front());
        pending.pop_front();

        if (component == "..") {
            can_prune = false;
            if (!ancestors.empty()) {
                current = std::move(ancestors.back().directory);
                ancestors.pop_back();
                continue;
            }

            if (contained)
                throw UnsafePatchPath(path);

            current = current.open_parent();
            continue;
        }

        require_component(component);

        try {
            auto next = current.open_child(component, create_missing);
            ancestors.emplace_back(std::move(current), component);
            current = std::move(next);
            continue;
        } catch (const std::system_error&) {
            std::string target;
            if (!current.read_link(component, target))
                throw;

            can_prune = false;
            if (remaining_links == 0) {
                if (contained)
                    throw UnsafePatchPath(path);
                throw std::system_error(std::make_error_code(std::errc::too_many_symbolic_link_levels), "Too many symbolic links in " + path);
            }
            --remaining_links;

            std::string target_from_root = target;
            if (filesystem::is_absolute(target)) {
                if (contained)
                    throw UnsafePatchPath(path);

                current = open_absolute_root(target, target_from_root);
                ancestors.clear();
            }

            const auto target_components = path_components(target_from_root);
            pending.insert(pending.begin(), target_components.begin(), target_components.end());
        }
    }

    if (!can_prune)
        ancestors.clear();
    return ResolvedPath(std::move(current), std::move(name), path, std::move(ancestors), can_prune);
}

FileType ResolvedPath::type() const
{
    return m_directory.type_of(m_name);
}

filesystem::perms ResolvedPath::permissions() const
{
    return m_directory.permissions_of(m_name);
}

FILE* ResolvedPath::open_read(bool binary) const
{
    return m_directory.open_read(m_name, binary);
}

FILE* ResolvedPath::open_write(bool binary) const
{
    return m_directory.open_write(m_name, binary);
}

void ResolvedPath::create_symlink(const std::string& target) const
{
    m_directory.create_symlink(target, m_name);
}

#ifdef _WIN32
// Windows records whether a link names a directory when the link is created and
// cannot change it afterwards, so the target has to be inspected first. Only a
// relative target which stays inside the directory holding the link is
// followed; anything else is taken to name a file, which is what a symbolic
// link in a patch almost always does.
bool OpenDirectory::names_directory(const std::string& target) const
{
    if (filesystem::is_absolute(target))
        return false;

    auto components = path_components(target);
    if (components.empty())
        return false;

    const auto name = std::move(components.back());
    components.pop_back();

    try {
        auto current = duplicate();
        for (const auto& component : components) {
            if (component == "..")
                return false;
            current = current.open_child(component, false);
        }

        require_component(name);
        return current.type_of(name) == FileType::Directory;
    } catch (const std::system_error&) {
        return false;
    }
}
#endif

bool ResolvedPath::create_directory() const
{
    return m_directory.create_directory(m_name);
}

void ResolvedPath::set_permissions(filesystem::perms permissions) const
{
    m_directory.set_permissions(m_name, permissions);
}

void ResolvedPath::remove()
{
    m_directory.remove(m_name);
}

void ResolvedPath::remove_and_empty_parents()
{
    remove();
    if (!m_can_prune)
        return;

    while (!m_ancestors.empty()) {
        auto& ancestor = m_ancestors.back();
        if (!ancestor.directory.remove_if_empty(ancestor.name))
            break;
        m_directory = std::move(ancestor.directory);
        m_ancestors.pop_back();
    }
}

FILE* ResolvedPath::create_exclusive_sibling(const std::string& sibling, bool binary, filesystem::perms permissions) const
{
    return m_directory.create_exclusive(sibling, binary, permissions);
}

void ResolvedPath::remove_sibling(const std::string& sibling) const
{
    m_directory.remove(sibling);
}

void ResolvedPath::replace_with_sibling(const std::string& sibling) const
{
    m_directory.rename_within(sibling, m_name);
}

ResolvedPath ResolvedPath::with_suffix(const std::string& suffix) const
{
    if (suffix.empty())
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "A path suffix may not be empty");

    auto name = m_name + suffix;
    OpenDirectory::require_component(name);
    return ResolvedPath(m_directory.duplicate(), std::move(name), m_path + suffix, { }, false);
}

} // namespace Patch
