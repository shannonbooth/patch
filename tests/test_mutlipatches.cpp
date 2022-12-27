// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/hunk.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <patch/test.h>

TEST(multi_patch_parse_unified_patch_simple)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff -u -r a/main1.cpp b/main1.cpp
--- a/main1.cpp	2022-11-06 12:51:37.191776249 +1300
+++ b/main1.cpp	2022-11-06 12:51:51.941802026 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
diff -u -r a/main2.cpp b/main2.cpp
--- a/main2.cpp	2022-11-06 12:52:24.101146380 +1300
+++ b/main2.cpp	2022-11-06 12:52:36.291771264 +1300
@@ -1,3 +1,2 @@
 //
-// just a main with a comment
-//
+// just a main with a changed comment
)");
    {
        auto patch1 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch1.hunks.size(), 1);

        const auto& lines = patch1.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "int main()");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "{");
        EXPECT_EQ(lines[1].operation, ' ');
        EXPECT_EQ(lines[2].line.content, "	return 0;");
        EXPECT_EQ(lines[2].operation, '+');
        EXPECT_EQ(lines[3].line.content, "}");
        EXPECT_EQ(lines[3].operation, ' ');
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch2.hunks.size(), 1);

        const auto& lines = patch2.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "//");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "// just a main with a comment");
        EXPECT_EQ(lines[1].operation, '-');
        EXPECT_EQ(lines[2].line.content, "//");
        EXPECT_EQ(lines[2].operation, '-');
        EXPECT_EQ(lines[3].line.content, "// just a main with a changed comment");
        EXPECT_EQ(lines[3].operation, '+');
    }
}

TEST(multi_patch_parse_unified_patch_simple_without_header)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff -u -r a/main1.cpp b/main1.cpp
--- a/main1.cpp	2022-11-06 12:51:37.191776249 +1300
+++ b/main1.cpp	2022-11-06 12:51:51.941802026 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
--- a/main2.cpp	2022-11-06 12:52:24.101146380 +1300
+++ b/main2.cpp	2022-11-06 12:52:36.291771264 +1300
@@ -1,3 +1,2 @@
 //
-// just a main with a comment
-//
+// just a main with a changed comment
)");
    {
        auto patch1 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch1.hunks.size(), 1);

        const auto& lines = patch1.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "int main()");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "{");
        EXPECT_EQ(lines[1].operation, ' ');
        EXPECT_EQ(lines[2].line.content, "	return 0;");
        EXPECT_EQ(lines[2].operation, '+');
        EXPECT_EQ(lines[3].line.content, "}");
        EXPECT_EQ(lines[3].operation, ' ');
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch2.hunks.size(), 1);

        const auto& lines = patch2.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "//");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "// just a main with a comment");
        EXPECT_EQ(lines[1].operation, '-');
        EXPECT_EQ(lines[2].line.content, "//");
        EXPECT_EQ(lines[2].operation, '-');
        EXPECT_EQ(lines[3].line.content, "// just a main with a changed comment");
        EXPECT_EQ(lines[3].operation, '+');
    }
}

TEST(multi_patch_parse_unified_patch_with_empty_space_header_after_first_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff -u -r a/main1.cpp b/main1.cpp
--- a/main1.cpp	2022-11-06 12:51:37.191776249 +1300
+++ b/main1.cpp	2022-11-06 12:51:51.941802026 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

NOTE that the line above is not part of the actual patch, and is
actually part of the header of the following patch :^)
--- a/main2.cpp	2022-11-06 12:52:24.101146380 +1300
+++ b/main2.cpp	2022-11-06 12:52:36.291771264 +1300
@@ -1,3 +1,2 @@
 //
-// just a main with a comment
-//
+// just a main with a changed comment
)");
    {
        auto patch1 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch1.hunks.size(), 1);

        const auto lines = patch1.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "int main()");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "{");
        EXPECT_EQ(lines[1].operation, ' ');
        EXPECT_EQ(lines[2].line.content, "	return 0;");
        EXPECT_EQ(lines[2].operation, '+');
        EXPECT_EQ(lines[3].line.content, "}");
        EXPECT_EQ(lines[3].operation, ' ');
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch2.hunks.size(), 1);

        const auto& lines = patch2.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "//");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "// just a main with a comment");
        EXPECT_EQ(lines[1].operation, '-');
        EXPECT_EQ(lines[2].line.content, "//");
        EXPECT_EQ(lines[2].operation, '-');
        EXPECT_EQ(lines[3].line.content, "// just a main with a changed comment");
        EXPECT_EQ(lines[3].operation, '+');
    }
}

TEST(multi_patch_parse_unified_patch_followed_by_context_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff -u -r a/main1.cpp b/main1.cpp
--- a/main1.cpp	2022-11-06 12:51:37.191776249 +1300
+++ b/main1.cpp	2022-11-06 12:51:51.941802026 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
diff -rc a/main2.cpp b/main2.cpp
*** a/main2.cpp	2022-11-06 12:52:24.101146380 +1300
--- b/main2.cpp	2022-11-06 12:52:36.291771264 +1300
***************
*** 1,3 ****
  //
! // just a main with a comment
! //
--- 1,2 ----
  //
! // just a main with a changed comment
)");
    {
        auto patch1 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch1.hunks.size(), 1);

        const auto& lines = patch1.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "int main()");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "{");
        EXPECT_EQ(lines[1].operation, ' ');
        EXPECT_EQ(lines[2].line.content, "	return 0;");
        EXPECT_EQ(lines[2].operation, '+');
        EXPECT_EQ(lines[3].line.content, "}");
        EXPECT_EQ(lines[3].operation, ' ');
    }

    // This doesn't work as the determination of the patch format will
    // be context, as the above parsing of first the patch incorrectly
    // leaves the file pointer to begin the next patch from the '---'
    // as the unified parser does not know that `***` is the start of
    // another patch.
    //
    // An easy fix would be to teach the unified diff parser to know
    // about *** as the beginning of a new patch, but I am not sure if
    // that is correct or if there is even a use case for this test.
    // There may be a more generic way of handling this?
    {
        auto patch2 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch2.hunks.size(), 1);

        const auto& lines = patch2.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "//");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "// just a main with a comment");
        EXPECT_EQ(lines[1].operation, '-');
        EXPECT_EQ(lines[2].line.content, "//");
        EXPECT_EQ(lines[2].operation, '-');
        EXPECT_EQ(lines[3].line.content, "// just a main with a changed comment");
        EXPECT_EQ(lines[3].operation, '+');
    }
}

TEST(multi_patch_parse_context_diff_simple)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff -rc a/main1.cpp b/main1.cpp
*** a/main1.cpp	2022-11-06 12:51:37.191776249 +1300
--- b/main1.cpp	2022-11-06 12:51:51.941802026 +1300
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+ 	return 0;
  }
diff -rc a/main2.cpp b/main2.cpp
*** a/main2.cpp	2022-11-06 12:52:24.101146380 +1300
--- b/main2.cpp	2022-11-06 12:52:36.291771264 +1300
***************
*** 1,3 ****
  //
! // just a main with a comment
! //
--- 1,2 ----
  //
! // just a main with a changed comment
)");
    {
        auto patch1 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch1.hunks.size(), 1);

        const auto& lines = patch1.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "int main()");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "{");
        EXPECT_EQ(lines[1].operation, ' ');
        EXPECT_EQ(lines[2].line.content, "	return 0;");
        EXPECT_EQ(lines[2].operation, '+');
        EXPECT_EQ(lines[3].line.content, "}");
        EXPECT_EQ(lines[3].operation, ' ');
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch2.hunks.size(), 1);

        const auto& lines = patch2.hunks[0].lines;
        EXPECT_EQ(lines.size(), 4);

        EXPECT_EQ(lines[0].line.content, "//");
        EXPECT_EQ(lines[0].operation, ' ');
        EXPECT_EQ(lines[1].line.content, "// just a main with a comment");
        EXPECT_EQ(lines[1].operation, '-');
        EXPECT_EQ(lines[2].line.content, "//");
        EXPECT_EQ(lines[2].operation, '-');
        EXPECT_EQ(lines[3].line.content, "// just a main with a changed comment");
        EXPECT_EQ(lines[3].operation, '+');
    }
}

TEST(multi_patch_parse_git_diff_rename_and_copy)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
diff --git a/b b/copy
similarity index 100%
copy from b
copy to copy
diff --git a/a b/rename
similarity index 100%
rename from a
rename to rename
)");
    {
        auto patch1 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch1.hunks.size(), 0);
        EXPECT_EQ(patch1.operation, Patch::Operation::Copy);
        EXPECT_EQ(patch1.old_file_path, "b");
        EXPECT_EQ(patch1.new_file_path, "copy");
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        EXPECT_EQ(patch2.hunks.size(), 0);
        EXPECT_EQ(patch2.operation, Patch::Operation::Rename);
        EXPECT_EQ(patch2.old_file_path, "a");
        EXPECT_EQ(patch2.new_file_path, "rename");
    }
}
