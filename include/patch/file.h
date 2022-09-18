// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <cinttypes>
#include <cstdio>
#include <ios>
#include <patch/system.h>
#include <system_error>

namespace Patch {

enum class NewLine {
    LF,
    CRLF,
    None,
};

class File {
public:
    File() = default;

    ~File();

    File(const File& other) = delete;
    File& operator=(const File& other) = delete;

    File(File&& other) noexcept;
    File& operator=(File&&) noexcept;

    File& operator<<(const std::string& content)
    {
        if (std::fwrite(content.c_str(), sizeof(std::string::value_type), content.size(), m_file) != content.size())
            throw std::system_error(errno, std::generic_category(), "Failed writing content to file");
        return *this;
    }

    File& operator<<(const char* content)
    {
        if (std::fputs(content, m_file) == EOF)
            throw std::system_error(errno, std::generic_category(), "Failed writing content to file");
        return *this;
    }

    File& operator<<(char c)
    {
        if (std::fputc(c, m_file) == EOF)
            throw std::system_error(errno, std::generic_category(), "Failed writing content to file");
        return *this;
    }

    File& operator<<(int64_t c)
    {
        if (std::fprintf(m_file, "%" PRId64, c) < 0)
            throw std::system_error(errno, std::generic_category(), "Failed writing content to file");
        return *this;
    }

    static File create_temporary();

    static File create_temporary(FILE* initial_content);

    static File create_temporary_with_content(const std::string& initial_content);

    void write_entire_contents_to(FILE* file);

    void write_entire_contents_to(File& file)
    {
        write_entire_contents_to(file.m_file);
    }

    explicit File(const std::string& path, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);

    bool get_line(std::string& line, NewLine* newline = nullptr);

    bool open(const std::string& path, std::ios_base::openmode mode);

    fpos_t tellg();

    void seekg(const fpos_t& pos);

    void clear()
    {
        is_eof = false;
        is_bad = false;
    }

    char peek();

    bool eof() const { return is_eof; }

    void close()
    {
        if (m_file) {
            fclose(m_file);
            m_file = nullptr;
        }
    }

    explicit operator bool() const { return !fail(); }

    bool fail() const
    {
        return !m_file || is_bad;
    }

    std::string read_all_as_string();

private:
    explicit File(FILE* file)
        : m_file(file)
    {
    }

    bool is_bad { false };
    bool is_eof { false };
    FILE* m_file { nullptr };
};

} // namespace Patch