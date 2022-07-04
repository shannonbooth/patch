// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/hunk.h>
#include <patch/parser.h>
#include <patch/system.h>

TEST(Parser, Simple)
{
    Patch::Hunk hunk;
    Patch::parse_unified_range(hunk, "@@ -1,3 +1,4 @@");
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 3);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
}

TEST(Parser, OneContextLine)
{
    Patch::Hunk hunk;
    Patch::parse_unified_range(hunk, "@@ -2,0 +3 @@");
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
}

TEST(Parser, NormalDiffHeader)
{
    Patch::Hunk hunk;
    EXPECT_TRUE(Patch::parse_normal_range(hunk, "2a3"));
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
}

TEST(Parser, NormalDiffSimple)
{
    std::stringstream patch_file(R"(2a3
> 	return 0;
)");
    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.format, Patch::Format::Normal);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
}

TEST(Parser, OneHunk)
{
    std::stringstream patch_file(R"(
--- a/only_add_return.cpp
+++ b/only_add_return.cpp
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "only_add_return.cpp");
    EXPECT_EQ(patch.new_file_path, "only_add_return.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 3);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);
}

TEST(Parser, OneHunkAddNoContext)
{
    std::stringstream patch_file(R"(
--- main1.cpp	2022-08-21 14:35:06.584242817 +1200
+++ main2.cpp	2022-08-21 14:19:47.509561172 +1200
@@ -2,0 +3 @@
+    return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
}

TEST(Parser, OneUnifiedHunkRemoveNoContext)
{
    std::stringstream patch_file(R"(
--- main1.cpp	2022-01-04 13:29:06.799930273 +1300
+++ main2.cpp	2022-01-04 13:29:05.599932817 +1300
@@ -3 +2,0 @@
-	return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 2);
}

TEST(Parser, OneContextHunkNoContextRemoveLine)
{
    std::stringstream patch_file(R"(
*** main1.cpp	2022-01-04 13:29:06.799930273 +1300
--- main2.cpp	2022-01-04 13:29:05.599932817 +1300
***************
*** 3 ****
- 	return 0;
--- 2 ----
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Context);
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 3);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 2);
}

TEST(Parser, OneContextHunkNoContextAddLine)
{
    std::stringstream patch_file(R"(
*** main1.cpp	2022-08-21 14:35:06.584242817 +1200
--- main2.cpp	2022-08-21 14:19:47.509561172 +1200
***************
*** 2 ****
--- 3 ----
+     return 0;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Context);
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.old_file_range.start_line, 2);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.new_file_range.start_line, 3);
}

TEST(Parser, UnifiedNoNewlineAtEndOfFile)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "no_newline1.cpp");
    EXPECT_EQ(patch.new_file_path, "no_newline2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);

    EXPECT_EQ(patch.hunks.back().lines.back().line.newline, Patch::NewLine::None);
}

TEST(Parser, NoNewlineInMiddleOfHunk)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);

    const auto& lines = patch.hunks.back().lines;
    ASSERT_EQ(lines.size(), 5);
    EXPECT_EQ(lines[2].line.newline, Patch::NewLine::None);
}

TEST(Parser, ContextNoNewlineAtEndOfFileBothSides)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "no_newline1.cpp");
    EXPECT_EQ(patch.new_file_path, "no_newline2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);

    ASSERT_EQ(hunk.lines.size(), 5);

    // Both the last from and to line have no newline at the end of the file.
    const auto& last_line = hunk.lines.back();
    EXPECT_EQ(last_line.operation, ' ');
    EXPECT_EQ(last_line.line.content, "}");
    EXPECT_EQ(last_line.line.newline, Patch::NewLine::None);
}

TEST(Parser, SpaceSeparatedFilenameAndTimestamp)
{
    // Inspired by an editor which mangled the tabs to spaces and patch was not
    // parsing this correctly. GNU patch could parse this correctly, so now
    // do we!
    std::stringstream patch_file(R"(
--- file  2022-08-21 14:35:06.584242817 +1200
+++ file  2022-08-21 14:19:47.509561172 +1200
@@ -1 +0,0 @@
-    int;
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Unified);
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "file");
    EXPECT_EQ(patch.new_file_path, "file");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 1);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 0);
    EXPECT_EQ(hunk.new_file_range.start_line, 0);
}

TEST(Parser, ContextNoNewlineAtEndOfFileOneSide)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);
    auto& hunk = patch.hunks[0];
    EXPECT_EQ(patch.old_file_path, "no_newline1.cpp");
    EXPECT_EQ(patch.new_file_path, "no_newline2.cpp");
    EXPECT_EQ(hunk.old_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.old_file_range.start_line, 1);
    EXPECT_EQ(hunk.new_file_range.number_of_lines, 4);
    EXPECT_EQ(hunk.new_file_range.start_line, 1);

    ASSERT_EQ(hunk.lines.size(), 6);

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

TEST(Parser, TwoHunks)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 2);
    EXPECT_EQ(patch.old_file_path, "only_add_return.cpp");
    EXPECT_EQ(patch.new_file_path, "only_add_return.cpp");
}

TEST(Parser, OneHunkNameInTimestamp)
{
    std::stringstream patch_file(R"(
--- main1.cpp	2022-06-06 10:19:48.246931254 +1200
+++ main2.cpp	2022-06-06 15:47:25.948226810 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");
}

TEST(Parser, GitDiffSimple)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "main.cpp");
    EXPECT_EQ(patch.new_file_path, "main.cpp");
}

TEST(Parser, GitRenameWithQuotedFilename)
{
    std::stringstream patch_file(R"(
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
    EXPECT_EQ(patch.format, Patch::Format::Unified);
}

TEST(Parser, TestWithTabInTimestampHeader)
{
    std::stringstream patch_file(R"(
--- ../test/main2.cpp	2022-06-06 15:47:25.948226810 +1200
+++ ../test/main1.cpp	2022-06-06 10:19:48.246931254 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    auto patch = Patch::parse_patch(patch_file);
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "main2.cpp");
    EXPECT_EQ(patch.new_file_path, "main1.cpp");
    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 4);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 1);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 3);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 1);
    EXPECT_FALSE(patch.hunks[0].lines.empty());
}

TEST(Parser, WithIndexHeader)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "Makefile");
    EXPECT_EQ(patch.new_file_path, "Makefile");
    EXPECT_EQ(patch.index_file_path, "Makefile");
    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 7);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 323);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 8);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 323);
    EXPECT_FALSE(patch.hunks[0].lines.empty());
}

TEST(Parser, ContextDiff)
{
    std::stringstream patch_file(R"(
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
    ASSERT_EQ(patch.hunks.size(), 1);
    EXPECT_EQ(patch.old_file_path, "test1_input.cpp");
    EXPECT_EQ(patch.new_file_path, "test1_output.cpp");
    EXPECT_EQ(patch.hunks[0].old_file_range.number_of_lines, 3);
    EXPECT_EQ(patch.hunks[0].old_file_range.start_line, 1);
    EXPECT_EQ(patch.hunks[0].new_file_range.number_of_lines, 4);
    EXPECT_EQ(patch.hunks[0].new_file_range.start_line, 1);
    EXPECT_FALSE(patch.hunks[0].lines.empty());
}

TEST(Parse, ComplexContextDiff)
{
    std::stringstream patch_file(R"(
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
)");
    ASSERT_EQ(patch_file.str().size(), 447); // keep trailing whitespace

    auto patch = Patch::parse_patch(patch_file);

    EXPECT_EQ(patch.old_file_path, "main2.cpp");
    EXPECT_EQ(patch.old_file_time, "2022-06-26 15:43:50.743831486 +1200");
    EXPECT_EQ(patch.new_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_time, "2022-06-26 15:44:36.224763329 +1200");

    ASSERT_EQ(patch.hunks.size(), 2);

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

TEST(Parse, ContextDiffWithChangedLine)
{
    std::stringstream patch_file(R"(
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
)");
    ASSERT_EQ(patch_file.str().size(), 364); // keep trailing whitespace

    auto patch = Patch::parse_patch(patch_file);

    EXPECT_EQ(patch.old_file_path, "main1.cpp");
    EXPECT_EQ(patch.new_file_path, "main2.cpp");

    ASSERT_EQ(patch.hunks.size(), 2);

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

TEST(Parse, NormalDiffNoNewLineAtEndOfFile)
{
    std::stringstream patch_file(R"(0a1
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

TEST(Parse, NormalDiffSpaceBeforeNormalCommand)
{
    std::stringstream patch_file(R"(
0a1
> a
)");

    auto patch = Patch::parse_patch(patch_file);
    const auto& lines = patch.hunks.at(0).lines;
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines.at(0).operation, '+');
    EXPECT_EQ(lines.at(0).line.content, "a");
}
