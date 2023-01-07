// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <istream>
#include <patch/hunk.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <patch/test.h>
#include <sstream>

TEST(determine_format_unified)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(--- a.cpp	2022-03-20 12:42:14.665007336 +1300
+++ b.cpp	2022-03-20 12:42:20.772998512 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+	return 1;
 }
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info, -1);
    EXPECT_EQ(patch.format, Patch::Format::Unified);

    std::stringstream output;
    parser.print_header_info(info, output);
    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|--- a.cpp	2022-03-20 12:42:14.665007336 +1300
|+++ b.cpp	2022-03-20 12:42:20.772998512 +1300
--------------------------
)");
}

TEST(determine_format_git)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(diff --git a/b.cpp b/b.cpp
index 5047a34..a46866d 100644
--- a/b.cpp
+++ b/b.cpp
@@ -1,3 +1,4 @@
 int main()
 {
+       return 0;
 }
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info, -1);
    EXPECT_EQ(patch.format, Patch::Format::Git);

    std::stringstream output;
    parser.print_header_info(info, output);
    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|diff --git a/b.cpp b/b.cpp
|index 5047a34..a46866d 100644
|--- a/b.cpp
|+++ b/b.cpp
--------------------------
)");
}

TEST(determine_format_git_extended_rename_no_hunk)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(diff --git a/new_file b/another_new
similarity index 100%
rename from new_file
rename to another_new
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info, -1);
    EXPECT_EQ(patch.format, Patch::Format::Git);
    EXPECT_EQ(patch.operation, Patch::Operation::Rename);
    EXPECT_EQ(patch.old_file_path, "new_file");
    EXPECT_EQ(patch.new_file_path, "another_new");

    std::stringstream output;
    parser.print_header_info(info, output);

    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|diff --git a/new_file b/another_new
|similarity index 100%
|rename from new_file
|rename to another_new
--------------------------
)");
}

TEST(determine_format_git_extended_rename_with_hunk)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(diff --git a/file b/test
similarity index 87%
rename from a/b/c/d/thing
rename to a/b/c/d/e/test
index 71ac1b5..fc3102f 100644
--- a/thing
+++ b/test
@@ -2,7 +2,6 @@ a
 b
 c
 d
-e
 f
 g
 h
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info, -1);
    EXPECT_EQ(patch.format, Patch::Format::Git);
    EXPECT_EQ(patch.operation, Patch::Operation::Rename);
    EXPECT_EQ(patch.old_file_path, "thing");
    EXPECT_EQ(patch.new_file_path, "test");

    std::stringstream output;
    parser.print_header_info(info, output);

    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|diff --git a/file b/test
|similarity index 87%
|rename from a/b/c/d/thing
|rename to a/b/c/d/e/test
|index 71ac1b5..fc3102f 100644
|--- a/thing
|+++ b/test
--------------------------
)");
}

TEST(determine_format_git_binary)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(From f933cb15f717a43ef1961d797874ca4a5650ff08 Mon Sep 17 00:00:00 2001
From: Shannon Booth <shannon.ml.booth@gmail.com>
Date: Mon, 18 Jul 2022 10:16:19 +1200
Subject: [PATCH] add utf16

---
 a.txt | Bin 0 -> 14 bytes
 1 file changed, 0 insertions(+), 0 deletions(-)
 create mode 100644 a.txt

diff --git a/a.txt b/a.txt
new file mode 100644
index 0000000000000000000000000000000000000000..c193b2437ca5bca3eaee833d9cc40b04875da742
GIT binary patch
literal 14
ScmezWFOh+ZAqj|+ffxWJ!UIA8

literal 0
HcmV?d00001

--
2.25.1
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info, -1);
    EXPECT_EQ(patch.format, Patch::Format::Git);
    EXPECT_EQ(patch.old_file_path, "a.txt");
    EXPECT_EQ(patch.new_file_path, "a.txt");
    EXPECT_EQ(patch.operation, Patch::Operation::Binary);

    std::stringstream output;
    parser.print_header_info(info, output);

    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|From f933cb15f717a43ef1961d797874ca4a5650ff08 Mon Sep 17 00:00:00 2001
|From: Shannon Booth <shannon.ml.booth@gmail.com>
|Date: Mon, 18 Jul 2022 10:16:19 +1200
|Subject: [PATCH] add utf16
|
|---
| a.txt | Bin 0 -> 14 bytes
| 1 file changed, 0 insertions(+), 0 deletions(-)
| create mode 100644 a.txt
|
|diff --git a/a.txt b/a.txt
|new file mode 100644
|index 0000000000000000000000000000000000000000..c193b2437ca5bca3eaee833d9cc40b04875da742
--------------------------
)");
}

TEST(determine_format_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(*** a.cpp	2022-04-03 18:41:54.611014944 +1200
--- c.cpp	2022-04-03 18:42:00.850801875 +1200
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info);
    EXPECT_EQ(patch.format, Patch::Format::Context);

    std::stringstream output;
    parser.print_header_info(info, output);
    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|*** a.cpp	2022-04-03 18:41:54.611014944 +1200
|--- c.cpp	2022-04-03 18:42:00.850801875 +1200
--------------------------
)");
}

TEST(determine_format_context_with_unified_range_in_header)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
Some text
@@ -1,29 +0,0 @@

*** a.cpp	2022-04-03 18:41:54.611014944 +1200
--- c.cpp	2022-04-03 18:42:00.850801875 +1200
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info);
    EXPECT_EQ(patch.format, Patch::Format::Context);

    std::stringstream output;
    parser.print_header_info(info, output);

    EXPECT_EQ(output.str(),
        R"(The text leading up to this was:
--------------------------
|
|Some text
|@@ -1,29 +0,0 @@
|
|*** a.cpp	2022-04-03 18:41:54.611014944 +1200
|--- c.cpp	2022-04-03 18:42:00.850801875 +1200
--------------------------
)");
}

TEST(determine_format_normal)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(2a3
> 	return 0;
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info);
    EXPECT_EQ(patch.format, Patch::Format::Normal);
    std::stringstream output;
    parser.print_header_info(info, output);
    EXPECT_EQ(output.str(), "");
}

TEST(determine_format_normal_with_from_and_to_file_lines)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(Index: thing
+++ a.cpp
--- b.cpp
*** c.cpp
2a3
> 	return 0;
)");

    Patch::Parser parser(patch_file);
    Patch::Patch patch;
    Patch::PatchHeaderInfo info;
    parser.parse_patch_header(patch, info);
    EXPECT_EQ(patch.format, Patch::Format::Normal);
    std::stringstream output;
    parser.print_header_info(info, output);
    EXPECT_EQ(output.str(), R"(The text leading up to this was:
--------------------------
|Index: thing
|+++ a.cpp
|--- b.cpp
|*** c.cpp
--------------------------
)");
    EXPECT_EQ(patch.index_file_path, "thing");
    EXPECT_EQ(patch.new_file_path, "");
    EXPECT_EQ(patch.old_file_path, "");
}

TEST(determine_format_looks_like_normal_command)
{
    Patch::Hunk hunk;

    // Possibilities given in POSIX diff utility guidelines, with %d substituted for a random integer.
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "1a2"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "1a23,3"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "12d2"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "1,2d3"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "10c20"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "1,2c31"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "9c2,3"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "1c5,93"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "18c2,3"));
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "5,7c8,10"));

    // Can only find two commas for a change command.
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "5,7d8,10"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "5,7a8,10"));

    // Some other invalid combinations.
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "> Some normal addition"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "5,7c8,10 "));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, " 5,7c8,10 "));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "5.7c8,10 "));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "1,2x3"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "1a2."));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "1a~2'"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "1,"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "1"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, "5,7c8,not_a_number"));
    EXPECT_FALSE(Patch::parse_normal_range(hunk, ""));
}

TEST(determine_format_looks_like_unified_range)
{
    Patch::Hunk hunk;
    EXPECT_TRUE(Patch::parse_unified_range(hunk, "@@ -1,3 +1,4 @@"));
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 3);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);

    EXPECT_TRUE(Patch::parse_unified_range(hunk, "@@ -2,0 +3 @@"));
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);

    EXPECT_TRUE(Patch::parse_unified_range(hunk, "@@ -3 +2,0 @@"));
    EXPECT_EQ(hunk.old_file_range.start_line, 3);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);

    EXPECT_FALSE(Patch::parse_unified_range(hunk, "@@ -3 +2,0 @"));
    EXPECT_FALSE(Patch::parse_unified_range(hunk, "@@ -3 +2.0 @@"));
    EXPECT_FALSE(Patch::parse_unified_range(hunk, "@@ -5,1a +9,8 @@"));
}

TEST(determine_format_string_to_uint32)
{
    Patch::LineNumber output = 0;
    EXPECT_TRUE((Patch::string_to_line_number("2", output)));
    EXPECT_EQ(output, 2);

    EXPECT_TRUE((Patch::string_to_line_number("100", output)));
    EXPECT_EQ(output, 100);

    EXPECT_TRUE((Patch::string_to_line_number("9223372036854775807", output)));
    EXPECT_EQ(output, 9223372036854775807);

    // Overflow
    EXPECT_FALSE((Patch::string_to_line_number("9223372036854775808", output)));

    // Empty!?
    EXPECT_FALSE((Patch::string_to_line_number("", output)));

    // Bad char
    EXPECT_FALSE((Patch::string_to_line_number("1a2", output)));
    EXPECT_FALSE((Patch::string_to_line_number("a1", output)));
}
