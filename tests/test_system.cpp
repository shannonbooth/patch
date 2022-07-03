// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/system.h>

TEST(System, GetLineLF)
{
    std::stringstream patch_file(
        "first line\n"
        "second line\n"
        "last line, trailing newline\n");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "last line, trailing newline");

    EXPECT_FALSE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(System, LFMissingAtEndOfFile)
{
    std::stringstream patch_file(
        "first line\n"
        "second line\n"
        "last line, no trailing newline");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "last line, no trailing newline");

    EXPECT_FALSE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(System, NewLineCRLF)
{
    std::stringstream patch_file(
        "first line\r\n"
        "second line\r\n"
        "last line, trailing newline\r\n");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "last line, trailing newline");

    EXPECT_FALSE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(System, CRLFMissingAtEndOfFile)
{
    std::stringstream patch_file(
        "first line\r\n"
        "second line\r\n"
        "last line, missing newline");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "first line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "second line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "last line, missing newline");

    EXPECT_FALSE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}

TEST(System, MixedNewLines)
{
    std::stringstream patch_file(
        "lf line\n"
        "crlf line\r\n"
        "missing newline");

    Patch::NewLine newline;
    std::string line;

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::LF);
    EXPECT_EQ(line, "lf line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::CRLF);
    EXPECT_EQ(line, "crlf line");

    EXPECT_TRUE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "missing newline");

    EXPECT_FALSE(Patch::get_line(patch_file, line, &newline));
    EXPECT_EQ(newline, Patch::NewLine::None);
    EXPECT_EQ(line, "");
}
