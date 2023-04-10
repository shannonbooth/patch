// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/parser.h>
#include <patch/test.h>
#include <stdexcept>

static void parse_file_line(const std::string& line, int strip, std::string& path, std::string* timestamp = nullptr)
{
    Patch::LineParser parser(line);
    parser.parse_file_line(strip, path, timestamp);
}

TEST(strip_linux_path)
{
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 0), "/my/path/for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 1), "my/path/for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 2), "path/for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 3), "for/test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 4), "test/purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 5), "purposes.txt");
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", 6), "");
}

TEST(strip_remove_all_leading)
{
    EXPECT_EQ(Patch::strip_path("/my/path/for/test/purposes.txt", -1), "purposes.txt");
    EXPECT_EQ(Patch::strip_path("/usr/bin/cat", -1), "cat");
    EXPECT_EQ(Patch::strip_path("noslash", -1), "noslash");

#ifdef _WIN32
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\thing.pdf", -1), "thing.pdf");
    EXPECT_EQ(Patch::strip_path("\\Program Files\\Utilities\\test.exe", -1), "test.exe");
    EXPECT_EQ(Patch::strip_path("2018\\thing.xlsx", -1), "thing.xlsx");
#endif
}

TEST(strip_multi_slash)
{
    EXPECT_EQ(Patch::strip_path("/path//with/multiple/slashes", 3), "multiple/slashes");
#ifdef _WIN32
    EXPECT_EQ(Patch::strip_path("C:\\Users\\\\Shannon\\another.txt", 2), "Shannon\\another.txt");
#endif
}

TEST(strip_windows_path)
{
#ifdef _WIN32
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 0), "C:\\Users\\Shannon\\test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 1), "Users\\Shannon\\test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 2), "Shannon\\test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 3), "test.pdf");
    EXPECT_EQ(Patch::strip_path("C:\\Users\\Shannon\\test.pdf", 4), "");
#endif
}

TEST(strip_quoted_string_bad)
{
    std::string path;

    EXPECT_THROW(parse_file_line("\"path/with unterminated comma", -1, path), std::invalid_argument);
    EXPECT_THROW(parse_file_line("\"secondUnterminatedCommaButAfterBackslash\\", -1, path), std::invalid_argument);
    EXPECT_THROW(parse_file_line("\"badEscapeChar\\l\"", -1, path), std::invalid_argument);
}

TEST(strip_quoted_string_good)
{
    std::string path;
    std::string timestamp;

    path.clear();
    parse_file_line("someInput", -1, path, &timestamp);
    EXPECT_EQ(path, "someInput");
    EXPECT_EQ(timestamp, "");

    path.clear();
    parse_file_line("\"some spaced input\"", -1, path, &timestamp);
    EXPECT_EQ(path, "some spaced input");
    EXPECT_EQ(timestamp, "");

    path.clear();
    parse_file_line(R"("with backslash \\ escape char")", 0, path, &timestamp);
    EXPECT_EQ(path, "with backslash \\ escape char");
    EXPECT_EQ(timestamp, "");

    path.clear();
    parse_file_line(R"("with quote \" escape char")", 0, path, &timestamp);
    EXPECT_EQ(path, "with quote \" escape char");
    EXPECT_EQ(timestamp, "");

    // Octal simple
    path.clear();
    parse_file_line(R"("\110\145\154\154\157\054\040\167\157\162\154\144\041\040\061\062\063")", 0, path, &timestamp);
    EXPECT_EQ(path, "Hello, world! 123");
    EXPECT_EQ(timestamp, "");

    // Octal escape codes (+ ASCII char)
    path.clear();
    parse_file_line(R"("\327\251\327\234\327\225\327\235 \327\242\327\225\327\234\327\235!")", 0, path, &timestamp);
    EXPECT_EQ(path, "שלום עולם!");
    EXPECT_EQ(timestamp, "");

    // Octal escape codes, not all 3 chars
    path.clear();
    parse_file_line(R"("\110\145\154\154\157\054\40cruel \167\157\162\154\144\041\40\061\62\063123")", 0, path, &timestamp);
    EXPECT_EQ(path, "Hello, cruel world! 123123");
    EXPECT_EQ(timestamp, "");

    path.clear();
    parse_file_line(R"("quoted string \\ then \" timestamp"	2022-06-10 19:28:11.018017172 +1200)", 0, path, &timestamp);
    EXPECT_EQ(path, "quoted string \\ then \" timestamp");
    EXPECT_EQ(timestamp, "\t2022-06-10 19:28:11.018017172 +1200"); // NOTE: in future we may want leading space trimmed.
}

TEST(strip_standard_path)
{
    std::string path;
    std::string timestamp;

    // Nothing
    path.clear();
    parse_file_line("a/file.txt", -1, path, &timestamp);
    EXPECT_EQ(path, "file.txt");
    EXPECT_EQ(timestamp, "");

    // Trailing space
    path.clear();
    parse_file_line("a/file.txt   2022-06-10 19:28:11.018017172 +1200", -1, path, &timestamp);
    EXPECT_EQ(path, "file.txt");
    EXPECT_EQ(timestamp, "  2022-06-10 19:28:11.018017172 +1200"); // NOTE: in future we may want leading space trimmed.

    // Trailing tab
    path.clear();
    parse_file_line("a/file.txt\t2022-06-10 19:28:11.018017172 +1200", -1, path, &timestamp);
    EXPECT_EQ(path, "file.txt");
    EXPECT_EQ(timestamp, "2022-06-10 19:28:11.018017172 +1200");

    // Space with no timestamp
    path.clear();
    timestamp.clear();
    parse_file_line("b/a file name\t", -1, path, &timestamp);
    EXPECT_EQ(path, "a file name");
    EXPECT_EQ(timestamp, "");

    // Space with timestamp
    path.clear();
    parse_file_line("b/a file name\t2022-06-10 19:28:11.018017172 +1200", -1, path, &timestamp);
    EXPECT_EQ(path, "a file name");
    EXPECT_EQ(timestamp, "2022-06-10 19:28:11.018017172 +1200");

    // Empty path
    parse_file_line("", -1, path, &timestamp);
    EXPECT_EQ(path, "");
    EXPECT_EQ(timestamp, "");
}
