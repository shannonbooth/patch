// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <cstdio>
#include <patch/file.h>
#include <patch/system.h>

namespace Patch {

static std::string to_mode(std::ios_base::openmode mode)
{
    std::string cmode;

    bool append = false;

    if (mode & std::ios_base::app) {
        cmode += "a";
        append = mode & std::ios_base::in;
    } else if (((mode & std::ios_base::out) && !(mode & std::ios_base::in)) || mode & std::ios_base::trunc) {
        cmode += "w";
        append = mode & std::ios_base::in;
    } else {
        cmode += "r";
        append = mode & std::ios_base::out;
    }

    if (append)
        cmode += "+";

    if (mode & std::ios_base::binary)
        cmode += "b";

    return cmode;
}

File::~File()
{
    if (m_file)
        std::fclose(m_file);
}

File::File(File&& other) noexcept
    : m_file(other.m_file)
{
    other.m_file = nullptr;
}

File& File::operator=(File&& other) noexcept
{
    if (&other != this) {
        if (m_file)
            std::fclose(m_file);
        m_file = other.m_file;
        other.m_file = nullptr;
    }
    return *this;
}

File File::create_temporary()
{
    FILE* file = std::tmpfile();
    if (!file)
        throw std::system_error(errno, std::generic_category(), "Unable to create temporary file");

    return File(file);
}

static void copy_from(FILE* from, FILE* to)
{
    std::array<char, 4096> buffer;

    while (true) {

        auto from_n = std::fread(buffer.data(), sizeof(char), buffer.size(), from);
        if (from_n == 0) {
            if (std::ferror(from) != 0)
                throw std::system_error(errno, std::generic_category(), "Error ocurred reading from file");
            break;
        }

        auto out_n = std::fwrite(buffer.data(), sizeof(char), from_n, to);
        if (out_n == 0) {
            if (std::ferror(to) != 0)
                throw std::system_error(errno, std::generic_category(), "Error ocurred writing to file");
            break;
        }
    }

    if (std::fflush(to) != 0)
        throw std::system_error(errno, std::generic_category(), "Error ocurred writing to file");
}

void File::write_entire_contents_to(FILE* file)
{
    std::rewind(m_file);
    copy_from(m_file, file);
}

File File::create_temporary(FILE* initial_content)
{
    FILE* file = std::tmpfile();
    if (!file)
        throw std::system_error(errno, std::generic_category(), "Unable to create temporary file");

    File real_file(file);

    copy_from(initial_content, file);

    std::rewind(file);

    return real_file;
}

File File::create_temporary_with_content(const std::string& initial_content)
{
    File file = create_temporary();
    file << initial_content;
    if (std::fflush(file.m_file) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to create temporary");

    std::rewind(file.m_file);

    return file;
}

File::File(const std::string& path, std::ios_base::openmode mode)
#ifdef _WIN32
    : m_file(_wfopen(to_native(path).c_str(), to_native(to_mode(mode)).c_str()))
#else
    : m_file(std::fopen(path.c_str(), to_mode(mode).c_str()))
#endif
{
    if (!m_file)
        throw std::system_error(errno, std::generic_category(), "Unable to open file " + path);
}

bool File::open(const std::string& path, std::ios_base::openmode mode)
{
#ifdef _WIN32
    m_file = _wfopen(to_native(path).c_str(), to_native(to_mode(mode)).c_str());
#else
    m_file = std::fopen(path.c_str(), to_mode(mode).c_str());
#endif

    return m_file;
}

fpos_t File::tellg()
{
    fpos_t pos;
    if (fgetpos(m_file, &pos) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to get file position");
    return pos;
}

void File::seekg(const fpos_t& pos)
{
    if (fsetpos(m_file, &pos) != 0)
        throw std::system_error(errno, std::generic_category(), "Unable to get file position");
}

char File::peek()
{
    int c = fgetc(m_file);
    ungetc(c, m_file);

    return static_cast<char>(c);
}

bool File::get_line(std::string& line, NewLine* newline)
{
    line.clear();

    if (is_eof) {
        if (newline)
            *newline = NewLine::None;
        is_bad = true;
        return false;
    }

    if (fail()) {
        if (newline)
            *newline = NewLine::None;
        return false;
    }

    while (true) {
        int c = std::getc(m_file);

        if (c == EOF) {
            if (std::ferror(m_file) != 0)
                throw std::system_error(errno, std::generic_category(), "Failed reading line from file");
            if (newline)
                *newline = NewLine::None;
            is_eof = true;

            return !line.empty();
        }

        if (c == '\n')
            break;

        line.push_back(static_cast<char>(c));
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
        if (newline)
            *newline = NewLine::CRLF;
    } else {
        if (newline)
            *newline = NewLine::LF;
    }

    return true;
}

std::string File::read_all_as_string()
{
    std::rewind(m_file);

    std::string content;

    while (true) {
        int c = std::getc(m_file);

        if (c == EOF) {
            if (std::ferror(m_file) != 0)
                throw std::system_error(errno, std::generic_category(), "Failed reading character from file");
            break;
        }

        content.push_back(static_cast<char>(c));
    }

    return content;
}

} // namespace Patch
