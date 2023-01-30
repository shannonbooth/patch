// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/hunk.h>
#include <patch/parser.h>
#include <patch/system.h>
#include <patch/test.h>

TEST(parser_simple)
{
    Patch::Hunk hunk;
    EXPECT_TRUE(Patch::parse_unified_range(hunk, "@@ -1,3 +1,4 @@"));
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 3);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
}

TEST(parser_one_context_line)
{
    Patch::Hunk hunk;
    EXPECT_TRUE(Patch::parse_unified_range(hunk, "@@ -2,0 +3 @@"));
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
}

TEST(bad_unified_ranges)
{
    Patch::Hunk hunk;
    EXPECT_FALSE(Patch::parse_unified_range(hunk, "@@ -a,0 +3 @@"));
    EXPECT_FALSE(Patch::parse_unified_range(hunk, "@@ -2,0 +3,x @@"));
}

TEST(parser_normal_diff_header)
{
    Patch::Hunk hunk;
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "2a3"));
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
}

TEST(parser_normal_diff_remove_header)
{
    // Regression test for a previously miscalculated normal range
    Patch::Hunk hunk;
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "1,3d0"));
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 3);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 0);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
}

TEST(parser_normal_diff_simple)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(2a3
> 	return 0;
)");
    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.format, Patch::Format::Normal);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
}

TEST(parser_one_hunk)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a/only_add_return.cpp
+++ b/only_add_return.cpp
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "only_add_return.cpp");
    EXPECT_EQ(patch.new_file_path, "only_add_return.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 3);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);
}

TEST(parser_one_hunk_add_no_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main1.cpp	2022-08-21 14:35:06.584242817 +1200
+++ main2.cpp	2022-08-21 14:19:47.509561172 +1200
@@ -2,0 +3 @@
+    return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
}

TEST(parser_one_unified_hunk_remove_no_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main1.cpp	2022-01-04 13:29:06.799930273 +1300
+++ main2.cpp	2022-01-04 13:29:05.599932817 +1300
@@ -3 +2,0 @@
-	return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 2);
}

TEST(parser_unified_hunk_with_prereq_line)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main1.cpp	2022-01-04 13:29:06.799930273 +1300
+++ main2.cpp	2022-01-04 13:29:05.599932817 +1300
Prereq: some_version-1.2.3
@@ -3 +2,0 @@
-	return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(patch.prerequisite, "some_version-1.2.3");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 2);
}

TEST(parser_one_context_hunk_no_context_remove_line)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
*** main1.cpp	2022-01-04 13:29:06.799930273 +1300
--- main2.cpp	2022-01-04 13:29:05.599932817 +1300
***************
*** 3 ****
- 	return 0;
--- 2 ----
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Context);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 2);
}

TEST(parser_one_context_hunk_no_context_add_line)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
*** main1.cpp	2022-08-21 14:35:06.584242817 +1200
--- main2.cpp	2022-08-21 14:19:47.509561172 +1200
***************
*** 2 ****
--- 3 ----
+     return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Context);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
}

TEST(parser_unified_no_newline_at_end_of_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- no_newline1.cpp	2022-01-30 13:57:31.173528027 +1300
+++ no_newline2.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
+    return 1;
 }
\ No newline at end of file
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "no_newline1.cpp");
    EXPECT_EQ(patch.new_file_path, "no_newline2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);

    EXPECT_EQ(patch.hunks.back().lines.back().line.newline, Patch::NewLine::None);
}

TEST(parser_no_newline_in_middle_of_hunk)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a.cpp	2022-05-08 14:42:02.601222193 +1200
+++ b.cpp	2022-04-24 18:21:59.931984592 +1200
@@ -1,3 +1,4 @@
 int main()
 {
-}
\ No newline at end of file
+	return 0;
+}
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);

    const auto& lines = patch.hunks.back().lines;
    EXPECT_EQ(lines.size(), 5);
    EXPECT_EQ(lines[2].line.newline, Patch::NewLine::None);
}

TEST(parser_context_no_newline_at_end_of_file_both_sides)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
*** no_newline1.cpp	2022-01-31 12:26:11.209333486 +1300
--- no_newline2.cpp	2022-01-31 12:26:14.089325436 +1300
***************
*** 1,4 ****
  int main()
  {
!     return 0;
  }
\ No newline at end of file
--- 1,4 ----
  int main()
  {
!     return 1;
  }
\ No newline at end of file
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Context);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "no_newline1.cpp");
    EXPECT_EQ(patch.new_file_path, "no_newline2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);

    EXPECT_EQ(hunk.lines.size(), 5);

    // Both the last from and to line have no newline at the end of the file.
    const auto& last_line = hunk.lines.back();
    EXPECT_EQ(last_line.operation, ' ');
    EXPECT_EQ(last_line.line.content, "}");
    EXPECT_EQ(last_line.line.newline, Patch::NewLine::None);
}

TEST(parser_space_separated_filename_and_timestamp)
{
    // Inspired by an editor which mangled the tabs to spaces and patch was not
    // parsing this correctly. GNU patch could parse this correctly, so now
    // do we!
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- file  2022-08-21 14:35:06.584242817 +1200
+++ file  2022-08-21 14:19:47.509561172 +1200
@@ -1 +0,0 @@
-    int;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "file");
    EXPECT_EQ(patch.new_file_path, "file");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 0);
}

TEST(parser_context_no_newline_at_end_of_file_one_side)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
*** no_newline1.cpp	2022-01-31 12:26:11.209333486 +1300
--- no_newline2.cpp	2022-01-31 12:28:16.076964354 +1300
***************
*** 1,4 ****
  int main()
  {
!     return 0;
! }
\ No newline at end of file
--- 1,4 ----
  int main()
  {
!     return 1;
! }
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Context);
    EXPECT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "no_newline1.cpp");
    EXPECT_EQ(patch.new_file_path, "no_newline2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);

    EXPECT_EQ(hunk.lines.size(), 6);

    // The last 'from-line' has no newline for it's last line.
    const auto& last_old_line = hunk.lines[3];
    EXPECT_EQ(last_old_line.operation, '-');
    EXPECT_EQ(last_old_line.line.content, "}");
    EXPECT_EQ(last_old_line.line.newline, Patch::NewLine::None);

    // The same is not true for the last 'to-line'.
    const auto& last_to_line = hunk.lines[5];
    EXPECT_EQ(last_to_line.operation, '+');
    EXPECT_EQ(last_to_line.line.content, "}");
    EXPECT_EQ(last_to_line.line.newline, Patch::NewLine::LF);
}

TEST(parser_two_hunks)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a/only_add_return.cpp
+++ b/only_add_return.cpp
@@ -1,7 +1,3 @@
-int sum(int x, int y)
-{
-    return x + y;
-}

 // Some comment

@@ -9,8 +5,3 @@
 {
     return x - y;
 }
-
-int main()
-{
-    return sum(sumbtract(1, 2), 3);
-}
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 2);
    EXPECT_EQ(patch.old_file_path, "only_add_return.cpp");
    EXPECT_EQ(patch.new_file_path, "only_add_return.cpp");
}

TEST(parser_one_hunk_name_in_timestamp)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main1.cpp	2022-06-06 10:19:48.246931254 +1200
+++ main2.cpp	2022-06-06 15:47:25.948226810 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
}

TEST(parser_git_diff_simple)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff --git a/main.cpp b/main.cpp
index 5047a34..905869d 100644
--- a/main.cpp
+++ b/main.cpp
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "main.cpp");
    EXPECT_EQ(patch.new_file_path, "main.cpp");
}

TEST(parser_git_rename_with_quoted_filename)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff --git a/a.txt "b/b\nc"
similarity index 100%
rename from a.txt
rename to "b\nc"
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.old_file_path, "a.txt");
    EXPECT_EQ(patch.new_file_path, "b\nc");
    EXPECT_EQ(patch.operation, Patch::Operation::Rename);
    EXPECT_EQ(patch.hunks.size(), 0);
    EXPECT_EQ(patch.index_file_path, "");
    EXPECT_EQ(patch.format, Patch::Format::Git);
}

TEST(parser_git_rename_with_strip_zero)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
From 89629b257f091dd0ff78509ca0ad626089defaa7 Mon Sep 17 00:00:00 2001
From: Shannon Booth <shannon.ml.booth@gmail.com>
Date: Tue, 5 Jul 2022 18:53:32 +1200
Subject: [PATCH] move a to b

---
 a => b | 0
 1 file changed, 0 insertions(+), 0 deletions(-)
 rename a => b (100%)

diff --git a/a b/b
similarity index 100%
rename from a
rename to b
--
2.25.1

)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Unknown, 0);
    EXPECT_EQ(patch.old_file_path, "a/a");
    EXPECT_EQ(patch.new_file_path, "b/b");
    EXPECT_EQ(patch.operation, Patch::Operation::Rename);
    EXPECT_EQ(patch.hunks.size(), 0);
    EXPECT_EQ(patch.index_file_path, "");
    EXPECT_EQ(patch.format, Patch::Format::Git);
}

TEST(parser_git_change_mode)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
From e8e9fc10f0915e2dfa02db34cce97aa7e66b4d61 Mon Sep 17 00:00:00 2001
From: Shannon Booth <shannon.ml.booth@gmail.com>
Date: Sun, 10 Jul 2022 09:50:24 +1200
Subject: [PATCH] add executable bit

---
 a | 0
 1 file changed, 0 insertions(+), 0 deletions(-)
 mode change 100644 => 100755 a

diff --git a/a b/a
old mode 100644
new mode 100755
--
2.25.1
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Unknown);
    EXPECT_EQ(patch.old_file_path, "a");
    EXPECT_EQ(patch.new_file_path, "a");
    EXPECT_EQ(patch.operation, Patch::Operation::Change);
    EXPECT_EQ(patch.hunks.size(), 0);
    EXPECT_EQ(patch.index_file_path, "");
    EXPECT_EQ(patch.format, Patch::Format::Git);
    EXPECT_EQ(patch.new_file_mode, 0100755);
    EXPECT_EQ(patch.old_file_mode, 0100644);
}

TEST(parser_git_change_mode_with_tabbed_filename)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff --git "a/some\tname" "b/some\tname"
old mode 100644
new mode 100755
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Unknown);
    EXPECT_EQ(patch.old_file_path, "some\tname");
    EXPECT_EQ(patch.new_file_path, "some\tname");
    EXPECT_EQ(patch.operation, Patch::Operation::Change);
    EXPECT_EQ(patch.hunks.size(), 0);
    EXPECT_EQ(patch.index_file_path, "");
    EXPECT_EQ(patch.format, Patch::Format::Git);
    EXPECT_EQ(patch.new_file_mode, 0100755);
    EXPECT_EQ(patch.old_file_mode, 0100644);
}

TEST(parser_git_change_mode_with_spaced_filename)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff --git a/with space b/with space
old mode 100755
new mode 100644
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Unknown);
    EXPECT_EQ(patch.old_file_path, "with space");
    EXPECT_EQ(patch.new_file_path, "with space");
    EXPECT_EQ(patch.operation, Patch::Operation::Change);
    EXPECT_EQ(patch.hunks.size(), 0);
    EXPECT_EQ(patch.index_file_path, "");
    EXPECT_EQ(patch.format, Patch::Format::Git);
    EXPECT_EQ(patch.old_file_mode, 0100755);
    EXPECT_EQ(patch.new_file_mode, 0100644);
}

TEST(parser_test_with_tab_in_timestamp_header)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- ../test/main2.cpp	2022-06-06 15:47:25.948226810 +1200
+++ ../test/main1.cpp	2022-06-06 10:19:48.246931254 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "main2.cpp");
    EXPECT_EQ(patch.new_file_path, "main1.cpp");
    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 4);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 1);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 3);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 1);
    EXPECT_FALSE(patch.hunks[0].lines.empty());
}

TEST(parser_with_index_header)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
Index: test/Makefile
===================================================================
--- test-server-tree.orig/Makefile
+++ test-server-test_v3_0_4a/Makefile
@@ -323,7 +323,8 @@
 install: install_help

 install_bin: install_dirs
-       cp -r xebin LICENSE* INSTALL $(TARGET_DIR) include
+       cp -r pbins include $(TARGET_DIRS)
+       cp --recursive LICENSES* $(DOC_INSTALL_DIRS)

        chmod 0755 scripts/*
        for script in env all_tests control logs; do \
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "Makefile");
    EXPECT_EQ(patch.new_file_path, "Makefile");
    EXPECT_EQ(patch.index_file_path, "Makefile");
    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 7);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 323);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 8);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 323);
    EXPECT_FALSE(patch.hunks[0].lines.empty());
}

TEST(parser_context_diff)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
*** test1_input.cpp	2022-06-19 17:14:31.743526819 +1200
--- test1_output.cpp	2022-06-19 17:14:31.743526819 +1200
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+ 	return 0;
  }
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "test1_input.cpp");
    EXPECT_EQ(patch.new_file_path, "test1_output.cpp");
    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 3);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 1);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 4);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 1);
    EXPECT_FALSE(patch.hunks[0].lines.empty());
}

TEST(parser_complex_context_diff)
{
    std::string patch_file_str = R"(
*** main2.cpp	2022-06-26 15:43:50.743831486 +1200
--- main1.cpp	2022-06-26 15:44:36.224763329 +1200
***************
*** 5,16 ****
      return "some data?!";
  }
  
- int some_negative(int a, int b)
- {
-     int c = a - b;
-     return c;
- }
- 
  int some_addition(int a, int b)
  {
      int c = a + b;
--- 5,10 ----
***************
*** 20,24 ****
  int main()
  {
      printf("This is a hello world!\n");
-     return -1;
  }
--- 14,17 ----
)";
    EXPECT_EQ(patch_file_str.size(), 447); // keep trailing whitespace
    Patch::File patch_file = Patch::File::create_temporary_with_content(patch_file_str);

    auto patch = Patch::parse_patch(patch_file);

    EXPECT_EQ(patch.old_file_path, "main2.cpp");
    EXPECT_EQ(patch.old_file_time, "2022-06-26 15:43:50.743831486 +1200");
    EXPECT_EQ(patch.new_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_time, "2022-06-26 15:44:36.224763329 +1200");

    EXPECT_EQ(patch.hunks.size(), 2);

    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 12);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 5);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 6);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 5);
    EXPECT_FALSE(patch.hunks[0].lines.empty());

    EXPECT_EQ(patch.hunks[1].old_file_range.number_of_lines, 5);
    EXPECT_EQ(patch.hunks[1].old_file_range.start_line, 20);
    EXPECT_EQ(patch.hunks[1].new_file_range.number_of_lines, 4);
    EXPECT_EQ(patch.hunks[1].new_file_range.start_line, 14);
    EXPECT_FALSE(patch.hunks[1].lines.empty());
}

TEST(parser_context_diff_with_changed_line)
{
    std::string patch_file_str = R"(
*** main1.cpp	2022-07-14 17:02:39.711744921 +1200
--- main2.cpp	2022-07-14 17:03:34.110450111 +1200
***************
*** 1,3 ****
--- 1,8 ----
+ int plus(int a, int b)
+ {
+ 	return a + b;
+ }
+ 
  int subtract(int a, int b)
  {
  	return a - b;
***************
*** 5,9 ****
  
  int main()
  {
! 	return 0;
  }
--- 10,14 ----
  
  int main()
  {
! 	return 1;
  }
)";
    EXPECT_EQ(patch_file_str.size(), 364); // keep trailing whitespace

    Patch::File patch_file = Patch::File::create_temporary_with_content(patch_file_str);
    auto patch = Patch::parse_patch(patch_file);

    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");

    EXPECT_EQ(patch.hunks.size(), 2);

    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 3);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 1);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 8);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 1);
    EXPECT_FALSE(patch.hunks[0].lines.empty());

    EXPECT_EQ(patch.hunks[1].old_file_range.number_of_lines, 5);
    EXPECT_EQ(patch.hunks[1].old_file_range.start_line, 5);
    EXPECT_EQ(patch.hunks[1].new_file_range.number_of_lines, 5);
    EXPECT_EQ(patch.hunks[1].new_file_range.start_line, 10);
    EXPECT_FALSE(patch.hunks[1].lines.empty());
}

TEST(parser_normal_diff_add_no_new_line_at_end_of_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(0a1
> a
\ No newline at end of file
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    const auto& lines = patch.hunks.at(0).lines;
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines.at(0).operation, '+');
    EXPECT_EQ(lines.at(0).line.content, "a");
    EXPECT_EQ(lines.at(0).line.newline, Patch::NewLine::None);
}

TEST(parser_normal_diff_remove_no_new_line_at_end_of_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(1d0
< d
\ No newline at end of file
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.hunks.size(), 1);
    const auto& lines = patch.hunks.at(0).lines;
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines.at(0).operation, '-');
    EXPECT_EQ(lines.at(0).line.content, "d");
    EXPECT_EQ(lines.at(0).line.newline, Patch::NewLine::None);
}

TEST(parser_normal_diff_space_before_normal_command)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
0a1
> a
)");

    auto patch = Patch::parse_patch(patch_file);
    const auto& lines = patch.hunks.at(0).lines;
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines.at(0).operation, '+');
    EXPECT_EQ(lines.at(0).line.content, "a");
}

TEST(DISABLED_parser_malformed_range_line_succeeds)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- /dev/null	2022-12-24 13:56:41.421181954 +1300
+++ a	2022-12-27 15:23:05.525596290 +1300
@@ -0,0 +1,1 @
+1
)");
    auto patch = Patch::parse_patch(patch_file);
    const auto& lines = patch.hunks.at(0).lines;
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines.at(0).operation, '+');
    EXPECT_EQ(lines.at(0).line.content, "1");
}

TEST(parser_malformed_range_line_fails)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(
        "--- /dev/null	2022-12-24 13:56:41.421181954 +1300\n"
        "+++ a	2022-12-27 15:23:05.525596290 +1300\n"
        "@@ -0,0 +1,1 \n"
        "+1\n");
    EXPECT_THROW(Patch::parse_patch(patch_file), std::runtime_error);
}
