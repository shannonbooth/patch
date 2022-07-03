// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/parser.h>
#include <patch/patch.h>

TEST(MultiPatchParse, UnifiedPatchSimple)
{
    std::stringstream patch_file(R"(
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
        ASSERT_EQ(patch1.hunks.size(), 1);
        ASSERT_EQ(patch1.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch1.hunks[0].lines[0], " int main()");
        EXPECT_EQ(patch1.hunks[0].lines[1], " {");
        EXPECT_EQ(patch1.hunks[0].lines[2], "+	return 0;");
        EXPECT_EQ(patch1.hunks[0].lines[3], " }");
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        ASSERT_EQ(patch2.hunks.size(), 1);
        ASSERT_EQ(patch2.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch2.hunks[0].lines[0], " //");
        EXPECT_EQ(patch2.hunks[0].lines[1], "-// just a main with a comment");
        EXPECT_EQ(patch2.hunks[0].lines[2], "-//");
        EXPECT_EQ(patch2.hunks[0].lines[3], "+// just a main with a changed comment");
    }
}

TEST(MultiPatchParse, UnifiedPatchSimpleWithoutHeader)
{
    std::stringstream patch_file(R"(
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
        ASSERT_EQ(patch1.hunks.size(), 1);
        ASSERT_EQ(patch1.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch1.hunks[0].lines[0], " int main()");
        EXPECT_EQ(patch1.hunks[0].lines[1], " {");
        EXPECT_EQ(patch1.hunks[0].lines[2], "+	return 0;");
        EXPECT_EQ(patch1.hunks[0].lines[3], " }");
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        ASSERT_EQ(patch2.hunks.size(), 1);
        ASSERT_EQ(patch2.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch2.hunks[0].lines[0], " //");
        EXPECT_EQ(patch2.hunks[0].lines[1], "-// just a main with a comment");
        EXPECT_EQ(patch2.hunks[0].lines[2], "-//");
        EXPECT_EQ(patch2.hunks[0].lines[3], "+// just a main with a changed comment");
    }
}

TEST(MultiPatchParse, UnifiedPatchWithEmptySpaceHeaderAfterFirstPatch)
{
    std::stringstream patch_file(R"(
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
        ASSERT_EQ(patch1.hunks.size(), 1);
        ASSERT_EQ(patch1.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch1.hunks[0].lines[0], " int main()");
        EXPECT_EQ(patch1.hunks[0].lines[1], " {");
        EXPECT_EQ(patch1.hunks[0].lines[2], "+	return 0;");
        EXPECT_EQ(patch1.hunks[0].lines[3], " }");
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        ASSERT_EQ(patch2.hunks.size(), 1);
        ASSERT_EQ(patch2.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch2.hunks[0].lines[0], " //");
        EXPECT_EQ(patch2.hunks[0].lines[1], "-// just a main with a comment");
        EXPECT_EQ(patch2.hunks[0].lines[2], "-//");
        EXPECT_EQ(patch2.hunks[0].lines[3], "+// just a main with a changed comment");
    }
}

TEST(MultiPatchParse, UnifiedPatchFollowedByContextPatch)
{
    std::stringstream patch_file(R"(
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
        ASSERT_EQ(patch1.hunks.size(), 1);
        ASSERT_EQ(patch1.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch1.hunks[0].lines[0], " int main()");
        EXPECT_EQ(patch1.hunks[0].lines[1], " {");
        EXPECT_EQ(patch1.hunks[0].lines[2], "+	return 0;");
        EXPECT_EQ(patch1.hunks[0].lines[3], " }");
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
        ASSERT_EQ(patch2.hunks.size(), 1);
        ASSERT_EQ(patch2.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch2.hunks[0].lines[0], " //");
        EXPECT_EQ(patch2.hunks[0].lines[1], "-// just a main with a comment");
        EXPECT_EQ(patch2.hunks[0].lines[2], "-//");
        EXPECT_EQ(patch2.hunks[0].lines[3], "+// just a main with a changed comment");
    }
}

TEST(MultiPatchParse, ContextDiffSimple)
{
    std::stringstream patch_file(R"(
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
        ASSERT_EQ(patch1.hunks.size(), 1);
        ASSERT_EQ(patch1.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch1.hunks[0].lines[0], " int main()");
        EXPECT_EQ(patch1.hunks[0].lines[1], " {");
        EXPECT_EQ(patch1.hunks[0].lines[2], "+	return 0;");
        EXPECT_EQ(patch1.hunks[0].lines[3], " }");
    }

    {
        auto patch2 = Patch::parse_patch(patch_file);
        ASSERT_EQ(patch2.hunks.size(), 1);
        ASSERT_EQ(patch2.hunks[0].lines.size(), 4);
        EXPECT_EQ(patch2.hunks[0].lines[0], " //");
        EXPECT_EQ(patch2.hunks[0].lines[1], "-// just a main with a comment");
        EXPECT_EQ(patch2.hunks[0].lines[2], "-//");
        EXPECT_EQ(patch2.hunks[0].lines[3], "+// just a main with a changed comment");
    }
}
