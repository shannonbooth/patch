// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/parser.h>
#include <stdexcept>

TEST(Strip, Linuxpath)
{
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 0), "/my/path/for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 1), "my/path/for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 2), "path/for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 3), "for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 4), "test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 5), "purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 6), "");
}

TEST(Strip, RemoveAllLeading)
{
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", -1), "purposes.txt");
    EXPECT_EQ(Patch::strip_path("/usr/bin/cat", -1), "cat");
    EXPECT_EQ(Patch::strip_path("noslash", -1), "noslash");

    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\thing.pdf", -1), "thing.pdf");
    EXPECT_EQ(Patch::strip_path("\\Program Files\\Utilities\\test.exe", -1), "test.exe");
    EXPECT_EQ(Patch::strip_path("2018\\thing.xlsx", -1), "thing.xlsx");
}

TEST(Strip, MultiSlash)
{
    EXPECT_EQ(Patch::strip_path("/path//with/multiple/slashes", 3), "multiple/slashes");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\\\Shannon\\another.txt", 2), "Shannon\\another.txt");
}

TEST(Strip, Windowspath)
{
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 0), "C:\\Users\\Shannon\\test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 1), "Users\\Shannon\\test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 2), "Shannon\\test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 3), "test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 4), "");
}

TEST(Strip, QuotedStringBad)
{
    std::string path;

    EXPECT_THROW(Patch::parse_file_line("\"path/with unterminated comma", -1, path), std::invalid_argument);
    EXPECT_THROW(Patch::parse_file_line("\"secondUnterminatedCommaButAfterBackslash\\", -1, path), std::invalid_argument);
    EXPECT_THROW(Patch::parse_file_line("\"badEscapeChar\\l\"", -1, path), std::invalid_argument);
}

TEST(Strip, QuotedStringGood)
{
    std::string path;
    std::string timestamp;

    path.clear();
    Patch::parse_file_line("someInput", -1, path, &timestamp);
    EXPECT_EQ(path, "someInput");
    EXPECT_EQ(timestamp, "");

    path.clear();
    Patch::parse_file_line("\"some spaced input\"", -1, path, &timestamp);
    EXPECT_EQ(path, "some spaced input");
    EXPECT_EQ(timestamp, "");

    path.clear();
    Patch::parse_file_line(R"("with backslash \\ escape char")", 0, path, &timestamp);
    EXPECT_EQ(path, "with backslash \\ escape char");
    EXPECT_EQ(timestamp, "");

    path.clear();
    Patch::parse_file_line(R"("with quote \" escape char")", 0, path, &timestamp);
    EXPECT_EQ(path, "with quote \" escape char");
    EXPECT_EQ(timestamp, "");

    path.clear();
    Patch::parse_file_line(R"("quoted string \\ then \" timestamp"	2022-06-10 19:28:11.018017172 +1200)", 0, path, &timestamp);
    EXPECT_EQ(path, "quoted string \\ then \" timestamp");
    EXPECT_EQ(timestamp, "\t2022-06-10 19:28:11.018017172 +1200"); // NOTE: in future we may want leading space trimmed.
}

TEST(Strip, StandardPath)
{
    std::string path;
    std::string timestamp;

    // Nothing
    path.clear();
    Patch::parse_file_line("a/file.txt", -1, path, &timestamp);
    EXPECT_EQ(path, "file.txt");
    EXPECT_EQ(timestamp, "");

    // Trailing space
    path.clear();
    Patch::parse_file_line("a/file.txt   2022-06-10 19:28:11.018017172 +1200", -1, path, &timestamp);
    EXPECT_EQ(path, "file.txt");
    EXPECT_EQ(timestamp, "  2022-06-10 19:28:11.018017172 +1200"); // NOTE: in future we may want leading space trimmed.

    // Trailing tab
    path.clear();
    Patch::parse_file_line("a/file.txt\t2022-06-10 19:28:11.018017172 +1200", -1, path, &timestamp);
    EXPECT_EQ(path, "file.txt");
    EXPECT_EQ(timestamp, "2022-06-10 19:28:11.018017172 +1200");

    // Empty path
    Patch::parse_file_line("", -1, path, &timestamp);
    EXPECT_EQ(path, "");
    EXPECT_EQ(timestamp, "");
}
