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
    , m_is_bad(other.m_is_bad)
    , m_is_eof(other.m_is_eof)
{
    other.m_file = nullptr;
}

File& File::operator=(File&& other) noexcept
{
    if (&other != this) {
        if (m_file)
            std::fclose(m_file);

        m_file = other.m_file;
        m_is_bad = other.m_is_bad;
        m_is_eof = other.m_is_eof;

        other.m_file = nullptr;
    }
    return *this;
}

File File::create_temporary()
{
    return File(create_temporary_file());
}

static void check_ferror(FILE* file, const char* operation)
{
    if (std::ferror(file) != 0)
        throw std::system_error(errno, std::generic_category(), operation);
}

static void fflush(FILE* file, const char* operation)
{
    if (std::fflush(file) != 0)
        throw std::system_error(errno, std::generic_category(), operation);
}

void File::copy_from(FILE* from, FILE* to)
{
    std::array<char, 4096> buffer;

    while (true) {

        auto from_n = std::fread(buffer.data(), sizeof(char), buffer.size(), from);
        if (from_n == 0) {
            check_ferror(from, "Error occurred reading from file");
            break;
        }

        fwrite(buffer.data(), from_n, to);
    }

    fflush(to, "Error occurred writing to file");
}

void File::write_entire_contents_to(FILE* file)
{
    std::rewind(m_file);
    copy_from(m_file, file);
}

File File::create_temporary(FILE* initial_content)
{
    File file(create_temporary_file());
    copy_from(initial_content, file.m_file);
    std::rewind(file.m_file);
    return file;
}

File File::create_temporary_with_content(const std::string& initial_content)
{
    File file = create_temporary();
    file << initial_content;
    fflush(file.m_file, "Unable to create temporary file");

    std::rewind(file.m_file);

    return file;
}

FILE* File::cfile_open_impl(const std::string& path, std::ios_base::openmode mode)
{
#ifdef _WIN32
    return _wfopen(to_native(path).c_str(), to_native(to_mode(mode)).c_str());
#else
    return std::fopen(path.c_str(), to_mode(mode).c_str());
#endif
}

File::File(const std::string& path, std::ios_base::openmode mode)
    : m_file(cfile_open_impl(path, mode))
{
    if (!m_file)
        throw std::system_error(errno, std::generic_category(), "Unable to open file " + path);
}

bool File::open(const std::string& path, std::ios_base::openmode mode)
{
    m_file = cfile_open_impl(path, mode);
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

    if (m_is_eof) {
        if (newline)
            *newline = NewLine::None;
        m_is_bad = true;
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
            check_ferror(m_file, "Failed reading line from file");
            if (newline)
                *newline = NewLine::None;
            m_is_eof = true;

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
            check_ferror(m_file, "Failed reading character from file");
            break;
        }

        content.push_back(static_cast<char>(c));
    }

    return content;
}

} // namespace Patch
