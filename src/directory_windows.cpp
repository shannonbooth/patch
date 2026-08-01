// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <io.h>
#include <limits>
#include <patch/directory.h>
#include <patch/system.h>
#include <system_error>
#include <utility>
#include <vector>
#include <windows.h>
#include <winioctl.h>
#include <winternl.h>

namespace Patch {

namespace {

constexpr ULONG nt_file_open = 0x00000001;
constexpr ULONG nt_file_create = 0x00000002;
constexpr ULONG nt_file_open_if = 0x00000003;
constexpr ULONG nt_file_directory = 0x00000001;
constexpr ULONG nt_file_synchronous = 0x00000020;
constexpr ULONG nt_file_non_directory = 0x00000040;
constexpr ULONG nt_file_open_reparse_point = 0x00200000;
constexpr ULONG symlink_flag_relative = 1;

constexpr ULONG nt_file_rename_information = 10;

using NtCreateFileFunction = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
using NtSetInformationFileFunction = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
using RtlNtStatusToDosErrorFunction = ULONG(WINAPI*)(NTSTATUS);

struct NativeApi {
    NtCreateFileFunction create_file { nullptr };
    NtSetInformationFileFunction set_information { nullptr };
    RtlNtStatusToDosErrorFunction status_to_error { nullptr };
};

static const NativeApi& native_api()
{
    // Avoid toolchain-specific ntdll import libraries.
    static const NativeApi api = [] {
        const auto module = GetModuleHandleW(L"ntdll.dll");
        if (!module)
            throw std::system_error(GetLastError(), std::system_category(), "Unable to load ntdll");

        NativeApi result;
        result.create_file = reinterpret_cast<NtCreateFileFunction>(GetProcAddress(module, "NtCreateFile"));
        result.set_information = reinterpret_cast<NtSetInformationFileFunction>(GetProcAddress(module, "NtSetInformationFile"));
        result.status_to_error = reinterpret_cast<RtlNtStatusToDosErrorFunction>(GetProcAddress(module, "RtlNtStatusToDosError"));
        if (!result.create_file || !result.set_information || !result.status_to_error)
            throw std::system_error(GetLastError(), std::system_category(), "Unable to load native file APIs");
        return result;
    }();
    return api;
}

static std::system_error windows_error(DWORD error, const std::string& message)
{
    // Call sites test for these conditions by their generic codes; whether the
    // system category maps to them varies between standard libraries.
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), message);
    case ERROR_DIRECTORY:
        return std::system_error(std::make_error_code(std::errc::not_a_directory), message);
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return std::system_error(std::make_error_code(std::errc::file_exists), message);
    default:
        return std::system_error(static_cast<int>(error), std::system_category(), message);
    }
}

static HANDLE create_file_relative(HANDLE parent, const std::wstring& name, ACCESS_MASK access, ULONG disposition, ULONG options, ULONG attributes = FILE_ATTRIBUTE_NORMAL)
{
    const auto bytes = name.size() * sizeof(wchar_t);
    if (bytes > std::numeric_limits<USHORT>::max()) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return INVALID_HANDLE_VALUE;
    }

    UNICODE_STRING object_name;
    object_name.Length = static_cast<USHORT>(bytes);
    object_name.MaximumLength = object_name.Length;
    object_name.Buffer = const_cast<PWSTR>(name.data());

    OBJECT_ATTRIBUTES object_attributes;
    object_attributes.Length = sizeof(object_attributes);
    object_attributes.RootDirectory = parent;
    object_attributes.ObjectName = &object_name;
    object_attributes.Attributes = OBJ_CASE_INSENSITIVE;
    object_attributes.SecurityDescriptor = nullptr;
    object_attributes.SecurityQualityOfService = nullptr;

    IO_STATUS_BLOCK status_block;
    HANDLE handle = INVALID_HANDLE_VALUE;
    const auto status = native_api().create_file(&handle, access, &object_attributes, &status_block, nullptr, attributes, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, disposition, options, nullptr, 0);
    if (status < 0) {
        const auto error = native_api().status_to_error(status);
        SetLastError(error);
        return INVALID_HANDLE_VALUE;
    }

    return handle;
}

static bool is_reparse_point(HANDLE handle)
{
    FILE_ATTRIBUTE_TAG_INFO information;
    if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &information, sizeof(information)) == 0) {
        throw windows_error(GetLastError(), "Unable to inspect file");
    }
    return (information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

static std::wstring absolute_path(const std::string& path)
{
    const auto native = to_native(path);
    std::wstring result(MAX_PATH, L'\0');

    while (true) {
        const auto size = GetFullPathNameW(native.c_str(), static_cast<DWORD>(result.size()), &result[0], nullptr);
        if (size == 0)
            throw windows_error(GetLastError(), "Unable to resolve path " + path);
        if (size < result.size()) {
            result.resize(size);
            return result;
        }
        result.resize(static_cast<std::size_t>(size) + 1);
    }
}

static std::wstring windows_root(const std::wstring& path)
{
    const auto is_separator = [](wchar_t character) { return character == L'\\' || character == L'/'; };

    if (path.size() >= 3 && path[1] == L':' && is_separator(path[2]))
        return path.substr(0, 3);

    std::size_t component = 2;
    if (path.compare(0, 4, L"\\\\?\\") == 0) {
        if (path.size() >= 7 && path[5] == L':' && is_separator(path[6]))
            return path.substr(0, 7);
        const bool unc = path.size() >= 8
            && (path[4] == L'U' || path[4] == L'u')
            && (path[5] == L'N' || path[5] == L'n')
            && (path[6] == L'C' || path[6] == L'c')
            && is_separator(path[7]);
        if (!unc) {
            const auto end = path.find_first_of(L"\\/", 4);
            return path.substr(0, end == std::wstring::npos ? path.size() : end + 1);
        }
        component = 8;
    } else if (path.size() < 2 || !is_separator(path[0]) || !is_separator(path[1])) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Path has no Windows root");
    }

    for (int i = 0; i < 2; ++i) {
        const auto end = path.find_first_of(L"\\/", component);
        if (end == std::wstring::npos)
            return path;
        component = end + 1;
    }
    return path.substr(0, component);
}

static HANDLE open_directory_path(const std::wstring& path)
{
    const auto handle = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open directory");
    return handle;
}

static std::wstring path_of_handle(HANDLE handle)
{
    std::wstring result(MAX_PATH, L'\0');

    while (true) {
        const auto size = GetFinalPathNameByHandleW(handle, &result[0], static_cast<DWORD>(result.size()), FILE_NAME_NORMALIZED);
        if (size == 0)
            throw windows_error(GetLastError(), "Unable to name an open directory");
        if (size < result.size()) {
            result.resize(size);
            return result;
        }
        result.resize(static_cast<std::size_t>(size) + 1);
    }
}

static FILE* stream_for_handle(HANDLE handle, bool binary, int access, const char* mode)
{
    const int flags = _O_NOINHERIT | (binary ? _O_BINARY : _O_TEXT) | access;

    const int fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(handle), flags);
    if (fd < 0) {
        const auto saved_errno = errno;
        CloseHandle(handle);
        errno = saved_errno;
        return nullptr;
    }

    FILE* stream = ::_fdopen(fd, mode);
    if (!stream) {
        const auto saved_errno = errno;
        ::_close(fd);
        errno = saved_errno;
    }
    return stream;
}

static HANDLE open_entry(HANDLE directory, const std::string& name, ACCESS_MASK access, ULONG disposition, bool directory_entry = false)
{
    auto options = nt_file_synchronous | nt_file_open_reparse_point;
    options |= directory_entry ? nt_file_directory : nt_file_non_directory;
    return create_file_relative(directory, to_native(name), access | SYNCHRONIZE, disposition, options);
}

static filesystem::perms permissions_from_attributes(DWORD attributes)
{
    auto permissions = filesystem::perms::owner_read
        | filesystem::perms::group_read | filesystem::perms::others_read;
    if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) {
        permissions |= filesystem::perms::owner_write
            | filesystem::perms::group_write | filesystem::perms::others_write;
    }
    return permissions;
}

static std::wstring normalized_symlink_target(const std::string& target, bool& relative)
{
    auto native = to_native(target);
    for (auto& character : native) {
        if (character == L'/')
            character = L'\\';
    }

    relative = !filesystem::is_absolute(target);
    if (relative)
        return native;

    if (native.size() >= 2 && native[0] == L'\\' && native[1] == L'\\')
        return L"\\??\\UNC\\" + native.substr(2);
    return L"\\??\\" + native;
}

struct SymbolicLinkReparseData {
    ULONG tag;
    USHORT data_length;
    USHORT reserved;
    USHORT substitute_name_offset;
    USHORT substitute_name_length;
    USHORT print_name_offset;
    USHORT print_name_length;
    ULONG flags;
    WCHAR path_buffer[1];
};

// FILE_RENAME_INFORMATION, which winternl.h does not declare.
struct NtRenameInformation {
    BOOLEAN replace_if_exists;
    HANDLE root_directory;
    ULONG file_name_length;
    WCHAR file_name[1];
};

} // namespace

OpenDirectory::OpenDirectory(void* handle)
    : m_handle(handle)
{
}

PathResolver::PathResolver(const std::string& root)
    : m_root(open_directory_path(to_native(root)))
{
}

OpenDirectory PathResolver::open_absolute_root(const std::string& path, std::string& path_from_root)
{
    const auto absolute = absolute_path(path);
    const auto root = windows_root(absolute);
    path_from_root = to_narrow(absolute.substr(root.size()));
    return OpenDirectory(open_directory_path(root));
}

OpenDirectory::~OpenDirectory()
{
    if (m_handle)
        CloseHandle(static_cast<HANDLE>(m_handle));
}

OpenDirectory::OpenDirectory(OpenDirectory&& other) noexcept
    : m_handle(other.m_handle)
{
    other.m_handle = nullptr;
}

OpenDirectory& OpenDirectory::operator=(OpenDirectory&& other) noexcept
{
    if (&other != this) {
        if (m_handle)
            CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

OpenDirectory OpenDirectory::duplicate() const
{
    HANDLE duplicate = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), static_cast<HANDLE>(m_handle), GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS) == 0) {
        throw windows_error(GetLastError(), "Unable to duplicate directory handle");
    }
    return OpenDirectory(duplicate);
}

OpenDirectory OpenDirectory::open_child(const std::string& name, bool create_missing) const
{
    require_component(name);
    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, create_missing ? nt_file_open_if : nt_file_open, nt_file_directory | nt_file_synchronous | nt_file_open_reparse_point);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open directory " + name);

    try {
        if (is_reparse_point(handle))
            throw windows_error(ERROR_CANT_ACCESS_FILE, "Unable to open directory " + name);
    } catch (...) {
        CloseHandle(handle);
        throw;
    }

    return OpenDirectory(handle);
}

bool OpenDirectory::read_link(const std::string& name, std::string& target) const
{
    require_component(name);
    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), FILE_READ_ATTRIBUTES | SYNCHRONIZE, nt_file_open, nt_file_synchronous | nt_file_open_reparse_point);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    std::vector<unsigned char> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    DWORD returned = 0;
    if (DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr) == 0) {
        const auto error = GetLastError();
        CloseHandle(handle);
        if (error == ERROR_NOT_A_REPARSE_POINT)
            return false;
        throw windows_error(error, "Unable to read symbolic link " + name);
    }
    CloseHandle(handle);

    const auto fixed_size = offsetof(SymbolicLinkReparseData, path_buffer);
    const auto* reparse = reinterpret_cast<const SymbolicLinkReparseData*>(buffer.data());
    if (returned < fixed_size || reparse->tag != IO_REPARSE_TAG_SYMLINK)
        return false;

    if (reparse->data_length < fixed_size - 8 || static_cast<std::size_t>(reparse->data_length) + 8 > returned) {
        throw windows_error(ERROR_INVALID_REPARSE_DATA, "Invalid symbolic link " + name);
    }

    const auto path_bytes = reparse->data_length - (fixed_size - 8);
    const auto offset = reparse->substitute_name_offset;
    const auto length = reparse->substitute_name_length;
    if (offset % sizeof(wchar_t) != 0 || length % sizeof(wchar_t) != 0 || offset > path_bytes || length > path_bytes - offset) {
        throw windows_error(ERROR_INVALID_REPARSE_DATA, "Invalid symbolic link " + name);
    }

    const auto* first = reinterpret_cast<const wchar_t*>(reinterpret_cast<const unsigned char*>(reparse->path_buffer) + offset);
    std::wstring native_target(first, length / sizeof(wchar_t));

    if ((reparse->flags & symlink_flag_relative) == 0) {
        if (native_target.compare(0, 8, L"\\??\\UNC\\") == 0)
            native_target = L"\\\\" + native_target.substr(8);
        else if (native_target.compare(0, 4, L"\\??\\") == 0)
            native_target.erase(0, 4);
    }

    target = to_narrow(native_target);
    return true;
}

FileType OpenDirectory::type_of(const std::string& name) const
{
    require_component(name);

    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), FILE_READ_ATTRIBUTES | SYNCHRONIZE, nt_file_open, nt_file_synchronous | nt_file_open_reparse_point);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return FileType::None;
        throw windows_error(error, "Unable to inspect " + name);
    }

    FILE_ATTRIBUTE_TAG_INFO information;
    if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &information, sizeof(information)) == 0) {
        const auto error = GetLastError();
        CloseHandle(handle);
        throw windows_error(error, "Unable to inspect " + name);
    }
    CloseHandle(handle);

    if ((information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && information.ReparseTag == IO_REPARSE_TAG_SYMLINK) {
        return FileType::Symlink;
    }
    if (information.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return FileType::Directory;
    if (information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return FileType::Other;
    return FileType::Regular;
}

filesystem::perms OpenDirectory::permissions_of(const std::string& name) const
{
    require_component(name);

    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), FILE_READ_ATTRIBUTES | SYNCHRONIZE, nt_file_open, nt_file_synchronous | nt_file_open_reparse_point);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return filesystem::perms::unknown;
        throw windows_error(error, "Unable to inspect permissions of " + name);
    }

    FILE_BASIC_INFO information;
    if (GetFileInformationByHandleEx(handle, FileBasicInfo, &information, sizeof(information)) == 0) {
        const auto error = GetLastError();
        CloseHandle(handle);
        throw windows_error(error, "Unable to inspect permissions of " + name);
    }
    CloseHandle(handle);
    return permissions_from_attributes(information.FileAttributes);
}

FILE* OpenDirectory::open_read(const std::string& name, bool binary) const
{
    require_component(name);

    const auto handle = open_entry(static_cast<HANDLE>(m_handle), name, GENERIC_READ | FILE_READ_ATTRIBUTES, nt_file_open);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open " + name + " for reading");

    try {
        if (is_reparse_point(handle)) {
            throw std::system_error(std::make_error_code(std::errc::too_many_symbolic_link_levels), "Refusing to open symbolic link " + name);
        }
    } catch (...) {
        CloseHandle(handle);
        throw;
    }

    auto* stream = stream_for_handle(handle, binary, _O_RDONLY, binary ? "rb" : "r");
    if (!stream)
        throw std::system_error(errno, std::generic_category(), "Unable to create a stream for " + name);
    return stream;
}

FILE* OpenDirectory::open_write(const std::string& name, bool binary) const
{
    require_component(name);

    const auto handle = open_entry(static_cast<HANDLE>(m_handle), name, GENERIC_WRITE | FILE_READ_ATTRIBUTES, nt_file_open_if);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open " + name + " for writing");

    try {
        if (is_reparse_point(handle)) {
            throw std::system_error(std::make_error_code(std::errc::too_many_symbolic_link_levels), "Refusing to open symbolic link " + name);
        }
    } catch (...) {
        CloseHandle(handle);
        throw;
    }

    LARGE_INTEGER beginning;
    beginning.QuadPart = 0;
    if (SetFilePointerEx(handle, beginning, nullptr, FILE_BEGIN) == 0 || SetEndOfFile(handle) == 0) {
        const auto error = GetLastError();
        CloseHandle(handle);
        throw windows_error(error, "Unable to truncate " + name);
    }

    auto* stream = stream_for_handle(handle, binary, _O_WRONLY, binary ? "wb" : "w");
    if (!stream)
        throw std::system_error(errno, std::generic_category(), "Unable to create a stream for " + name);
    return stream;
}

// The requested permissions carry nothing Windows records at creation. The files
// created here are staged temporaries, which are always writable, and the read-only
// attribute is applied when a result is installed.
FILE* OpenDirectory::create_exclusive(const std::string& name, bool binary, filesystem::perms) const
{
    require_component(name);

    const auto handle = open_entry(static_cast<HANDLE>(m_handle), name, GENERIC_READ | GENERIC_WRITE | FILE_READ_ATTRIBUTES, nt_file_create);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Failed exclusively creating file " + name);

    auto* stream = stream_for_handle(handle, binary, _O_RDWR, binary ? "wb+" : "w+");
    if (!stream) {
        const auto saved_errno = errno;
        try {
            remove(name);
        } catch (...) {
        }
        throw std::system_error(saved_errno, std::generic_category(), "Failed opening exclusively created file " + name);
    }
    return stream;
}

bool OpenDirectory::create_directory(const std::string& name) const
{
    require_component(name);
    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), FILE_LIST_DIRECTORY | SYNCHRONIZE, nt_file_create, nt_file_directory | nt_file_synchronous);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return true;
    }
    const auto error = GetLastError();
    if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
        return false;
    throw windows_error(error, "Unable to create directory " + name);
}

void OpenDirectory::set_permissions(const std::string& name, filesystem::perms permissions) const
{
    require_component(name);
    if (permissions == filesystem::perms::unknown)
        return;

    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, nt_file_open, nt_file_synchronous | nt_file_open_reparse_point);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open " + name + " for changing permissions");

    try {
        if (is_reparse_point(handle))
            throw std::system_error(std::make_error_code(std::errc::too_many_symbolic_link_levels), "Refusing to change permissions of symbolic link " + name);

        FILE_BASIC_INFO information { };
        if (GetFileInformationByHandleEx(handle, FileBasicInfo, &information, sizeof(information)) == 0)
            throw windows_error(GetLastError(), "Unable to inspect permissions of " + name);

        const auto write_permissions = filesystem::perms::owner_write | filesystem::perms::group_write | filesystem::perms::others_write;
        if ((permissions & write_permissions) == filesystem::perms::none)
            information.FileAttributes |= FILE_ATTRIBUTE_READONLY;
        else
            information.FileAttributes &= ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
        if (SetFileInformationByHandle(handle, FileBasicInfo, &information, sizeof(information)) == 0)
            throw windows_error(GetLastError(), "Unable to change permissions of " + name);
    } catch (...) {
        CloseHandle(handle);
        throw;
    }
    CloseHandle(handle);
}

void OpenDirectory::rename_within(const std::string& from, const std::string& to) const
{
    require_component(from);
    require_component(to);

    const auto directory = static_cast<HANDLE>(m_handle);
    const auto source = open_entry(directory, from, DELETE | FILE_READ_ATTRIBUTES, nt_file_open);
    if (source == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open " + from + " for rename");

    // Renamed through NtSetInformationFile since the Win32 wrapper documents
    // its RootDirectory as having to be null, and the destination must be a
    // name relative to this directory.
    const auto native_to = to_native(to);
    const auto name_bytes = native_to.size() * sizeof(wchar_t);
    std::vector<unsigned char> buffer(offsetof(NtRenameInformation, file_name) + name_bytes);
    auto* information = reinterpret_cast<NtRenameInformation*>(buffer.data());
    information->replace_if_exists = TRUE;
    information->root_directory = directory;
    information->file_name_length = static_cast<ULONG>(name_bytes);
    std::memcpy(information->file_name, native_to.data(), name_bytes);

    const auto rename = [&] {
        IO_STATUS_BLOCK status_block;
        const auto status = native_api().set_information(source, &status_block, information, static_cast<ULONG>(buffer.size()), nt_file_rename_information);
        if (status < 0) {
            SetLastError(native_api().status_to_error(status));
            return false;
        }
        return true;
    };

    if (rename()) {
        CloseHandle(source);
        return;
    }

    const auto first_error = GetLastError();
    if (first_error != ERROR_ACCESS_DENIED) {
        CloseHandle(source);
        throw windows_error(first_error, "Unable to rename " + from + " to " + to);
    }

    // Windows will not replace a read-only destination. Hold the destination
    // open while changing its attributes so that a concurrent rename cannot
    // make us modify or restore a different file.
    const auto destination = open_entry(directory, to, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, nt_file_open);
    if (destination == INVALID_HANDLE_VALUE) {
        CloseHandle(source);
        throw windows_error(first_error, "Unable to rename " + from + " to " + to);
    }

    FILE_BASIC_INFO original { };
    if (GetFileInformationByHandleEx(destination, FileBasicInfo, &original, sizeof(original)) == 0 || (original.FileAttributes & FILE_ATTRIBUTE_READONLY) == 0) {
        CloseHandle(destination);
        CloseHandle(source);
        throw windows_error(first_error, "Unable to rename " + from + " to " + to);
    }

    auto writable = original;
    writable.FileAttributes &= ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
    if (writable.FileAttributes == 0)
        writable.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    if (SetFileInformationByHandle(destination, FileBasicInfo, &writable, sizeof(writable)) == 0) {
        const auto error = GetLastError();
        CloseHandle(destination);
        CloseHandle(source);
        throw windows_error(error, "Unable to make file writable " + to);
    }

    if (rename()) {
        CloseHandle(destination);
        CloseHandle(source);
        return;
    }

    const auto rename_error = GetLastError();
    if (SetFileInformationByHandle(destination, FileBasicInfo, &original, sizeof(original)) == 0) {
        const auto restore_error = GetLastError();
        CloseHandle(destination);
        CloseHandle(source);
        throw windows_error(restore_error, "Unable to restore permissions after rename failed for " + to);
    }

    CloseHandle(destination);
    CloseHandle(source);
    throw windows_error(rename_error, "Unable to rename " + from + " to " + to);
}

void OpenDirectory::remove(const std::string& name) const
{
    require_component(name);

    const auto handle = create_file_relative(static_cast<HANDLE>(m_handle), to_native(name), DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, nt_file_open, nt_file_synchronous | nt_file_open_reparse_point);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open file " + name + " for removal");

    FILE_DISPOSITION_INFO information;
    information.DeleteFile = TRUE;
    if (SetFileInformationByHandle(handle, FileDispositionInfo, &information, sizeof(information)) == 0) {
        const auto error = GetLastError();
        CloseHandle(handle);
        throw windows_error(error, "Unable to remove file " + name);
    }
    CloseHandle(handle);
}

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#    define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

// Setting a reparse point directly needs a privilege the caller may not hold,
// while CreateSymbolicLinkW can also create a link for a developer-mode user
// without it. There is no handle-relative spelling of that call, so the link
// is named through this directory's path; the window that opens exists only
// where the privileged path is unavailable.
static void create_symlink_by_name(HANDLE directory, const std::string& target, const std::string& name, bool target_is_directory)
{
    const auto link_path = path_of_handle(directory) + L'\\' + to_native(name);

    auto native_target = to_native(target);
    for (auto& character : native_target) {
        if (character == L'/')
            character = L'\\';
    }

    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (target_is_directory)
        flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

    if (CreateSymbolicLinkW(link_path.c_str(), native_target.c_str(), flags) != 0)
        return;

    auto error = GetLastError();

    // Windows which does not know the unprivileged-create flag rejects it with
    // ERROR_INVALID_PARAMETER. If returned, retry without it.
    if (error == ERROR_INVALID_PARAMETER) {
        flags &= ~static_cast<DWORD>(SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
        if (CreateSymbolicLinkW(link_path.c_str(), native_target.c_str(), flags) != 0)
            return;
        error = GetLastError();
    }

    throw windows_error(error, "Unable to create symbolic link " + name);
}

void OpenDirectory::create_symlink(const std::string& target, const std::string& name) const
{
    require_component(name);
    const bool target_is_directory = names_directory(target);

    // A link to a directory must itself be created as a directory, otherwise
    // Windows will not traverse it even though the reparse point is correct.
    const auto handle = open_entry(static_cast<HANDLE>(m_handle), name, GENERIC_WRITE | DELETE | FILE_READ_ATTRIBUTES, nt_file_create, target_is_directory);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to create symbolic link " + name);

    bool relative = false;
    const auto substitute = normalized_symlink_target(target, relative);
    const auto print = to_native(target);
    const auto substitute_bytes = substitute.size() * sizeof(wchar_t);
    const auto print_bytes = print.size() * sizeof(wchar_t);
    const auto data_bytes = substitute_bytes + print_bytes;
    const auto fixed_size = offsetof(SymbolicLinkReparseData, path_buffer);

    if (fixed_size + data_bytes > MAXIMUM_REPARSE_DATA_BUFFER_SIZE || data_bytes > std::numeric_limits<USHORT>::max() - (fixed_size - 8)) {
        CloseHandle(handle);
        try {
            remove(name);
        } catch (...) {
        }
        throw std::system_error(std::make_error_code(std::errc::filename_too_long), "Symbolic link target is too long");
    }

    std::vector<unsigned char> buffer(fixed_size + data_bytes);
    auto* reparse = reinterpret_cast<SymbolicLinkReparseData*>(buffer.data());
    reparse->tag = IO_REPARSE_TAG_SYMLINK;
    reparse->data_length = static_cast<USHORT>(fixed_size - 8 + data_bytes);
    reparse->reserved = 0;
    reparse->substitute_name_offset = 0;
    reparse->substitute_name_length = static_cast<USHORT>(substitute_bytes);
    reparse->print_name_offset = static_cast<USHORT>(substitute_bytes);
    reparse->print_name_length = static_cast<USHORT>(print_bytes);
    reparse->flags = relative ? symlink_flag_relative : 0;
    std::memcpy(reparse->path_buffer, substitute.data(), substitute_bytes);
    std::memcpy(reinterpret_cast<unsigned char*>(reparse->path_buffer) + substitute_bytes, print.data(), print_bytes);

    DWORD returned = 0;
    if (DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, reparse, static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr) == 0) {
        const auto error = GetLastError();
        CloseHandle(handle);
        try {
            remove(name);
        } catch (...) {
        }

        if (error == ERROR_PRIVILEGE_NOT_HELD) {
            create_symlink_by_name(static_cast<HANDLE>(m_handle), target, name, target_is_directory);
            return;
        }

        throw windows_error(error, "Unable to create symbolic link " + name);
    }
    CloseHandle(handle);
}

bool OpenDirectory::remove_if_empty(const std::string& name) const
{
    require_component(name);

    const auto handle = open_entry(static_cast<HANDLE>(m_handle), name, DELETE | FILE_READ_ATTRIBUTES, nt_file_open, true);
    if (handle == INVALID_HANDLE_VALUE)
        throw windows_error(GetLastError(), "Unable to open directory " + name + " for removal");

    FILE_DISPOSITION_INFO information;
    information.DeleteFile = TRUE;
    if (SetFileInformationByHandle(handle, FileDispositionInfo, &information, sizeof(information)) != 0) {
        CloseHandle(handle);
        return true;
    }

    const auto error = GetLastError();
    CloseHandle(handle);
    if (error == ERROR_DIR_NOT_EMPTY)
        return false;
    throw windows_error(error, "Unable to remove directory " + name);
}

OpenDirectory OpenDirectory::open_parent() const
{
    // A relative NT open does not interpret "..", which is Win32 path syntax,
    // so name this directory and open its parent by that name. This is the one
    // walk Windows cannot make relative to a handle, and it is only reached
    // for a user-named path, which keeps ordinary path semantics anyway.
    const auto path = path_of_handle(static_cast<HANDLE>(m_handle));
    const auto root = windows_root(path);

    // Like "/..", the parent of a root is the root itself.
    if (path.size() <= root.size())
        return duplicate();

    const auto end = path.find_last_of(L"\\/");
    const auto parent = end < root.size() ? root : path.substr(0, end);
    return OpenDirectory(open_directory_path(parent));
}

} // namespace Patch
