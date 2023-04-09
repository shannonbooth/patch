// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <ios>
#include <patch/file.h>
#include <patch/system.h>
#include <patch/test.h>
#include <system_error>

TEST(file_get_line_lf)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(
        "first line\n"
        "second line\n"
        "last line, trailing newline\n");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "last line, trailing newline");

    EXPECT_FALSE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(file_lf_missing_at_end_of_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(
        "first line\n"
        "second line\n"
        "last line, no trailing newline");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "last line, no trailing newline");

    EXPECT_FALSE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(file_new_line_crlf)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(
        "first line\r\n"
        "second line\r\n"
        "last line, trailing newline\r\n");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "last line, trailing newline");

    EXPECT_FALSE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(file_crlf_missing_at_end_of_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(
        "first line\r\n"
        "second line\r\n"
        "last line, missing newline");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "last line, missing newline");

    EXPECT_FALSE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(file_mixed_new_lines)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(
        "lf line\n"
        "crlf line\r\n"
        "missing newline");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "lf line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "crlf line");

    EXPECT_TRUE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "missing newline");

    EXPECT_FALSE(patch_file.get_line(line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(file_move_construct_move_assign)
{
    // Construct a temporary file, get_line until eof and bad.
    Patch::File file_orig = Patch::File::create_temporary_with_content("abc\n");
    EXPECT_FALSE(file_orig.eof());
    EXPECT_TRUE(file_orig);

    std::string content;
    EXPECT_TRUE(file_orig.get_line(content));
    EXPECT_EQ(content, "abc");
    EXPECT_FALSE(file_orig.eof());
    EXPECT_TRUE(file_orig);

    EXPECT_FALSE(file_orig.get_line(content));
    EXPECT_EQ(content, "");
    EXPECT_TRUE(file_orig.eof());
    EXPECT_TRUE(file_orig);

    EXPECT_FALSE(file_orig.get_line(content));
    EXPECT_EQ(content, "");
    EXPECT_TRUE(file_orig.eof());
    EXPECT_FALSE(file_orig);

    // Move construct a new file, it should inherit state.
    Patch::File file_move_constructed = std::move(file_orig);
    EXPECT_TRUE(file_move_constructed.eof());
    EXPECT_FALSE(file_move_constructed);

    // Move assign a new file, it should inherit state.
    Patch::File file_move_assigned = Patch::File::create_temporary_with_content("some stuff");
    file_move_assigned = std::move(file_move_constructed);

    EXPECT_TRUE(file_move_assigned.eof());
    EXPECT_FALSE(file_move_assigned);

    // Should still be able to read from the file, but using new content!
    EXPECT_EQ(file_move_assigned.read_all_as_string(), "abc\n");
}

PATCH_TEST(file_append)
{
    (void)patch_path;

    {
        Patch::File file("some-file", std::ios_base::out);
        file << "first line!\n";
    }

    {
        Patch::File file("some-file", std::ios_base::out | std::ios_base::app);
        file << "second line!\n";
    }

    EXPECT_FILE_EQ("some-file", "first line!\nsecond line!\n");
}

PATCH_TEST(file_open_failure)
{
    EXPECT_THROW(Patch::File("file-that-does-not-exist"), std::system_error);
}
