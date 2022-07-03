// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/applier.h>
#include <patch/options.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <sstream>

TEST(Applier, AddOnelinePatch)
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

    std::stringstream input_file(R"(int main()
{
}
)");

    std::stringstream expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, AddOnelineNormalPatch)
{
    std::stringstream patch_file(R"(2a3
>     return 0;
)");

    std::stringstream input_file(R"(int main()
{
}
)");

    std::stringstream expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, RemoveOnelineNormalPatch)
{
    std::stringstream patch_file(R"(3d2
<     return 0;
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    std::stringstream reject;
    std::stringstream output;
    EXPECT_EQ(patch.hunks.size(), 1);

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
}

TEST(Applier, AddOnelinePatchNoContext)
{
    std::stringstream patch_file(R"(
--- main1.cpp	2022-08-21 14:35:06.584242817 +1200
+++ main2.cpp	2022-08-21 14:19:47.509561172 +1200
@@ -2,0 +3 @@
+    return 0;
)");

    std::stringstream input_file(R"(int main()
{
}
)");

    std::stringstream expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, RemoveOnelinePatchTwoLinesContext)
{
    std::stringstream patch_file(R"(
--- main2.cpp	2022-09-18 14:35:15.923795777 +1200
+++ main.cpp	2022-09-18 14:35:12.460038934 +1200
@@ -2,3 +2,2 @@
 {
-    return 0;
 }
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, MySecond1234567)
{
    std::stringstream patch_file(R"(
--- 1	2022-09-18 15:44:34.900520464 +1200
+++ 2	2022-09-18 15:44:50.319066119 +1200
@@ -4,3 +4,2 @@
 4
-5
 6
)");

    std::stringstream input_file(R"(1
2
3
4
5
6
7
8
9
)");

    std::stringstream expected_output(R"(1
2
3
4
6
7
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, TwoLinesRemovedWithNoContext)
{
    std::stringstream patch_file(R"(
--- /tmp/1.txt	2022-09-19 11:24:50.305063358 +1200
+++ /tmp/2.txt	2022-09-19 11:24:40.285189350 +1200
@@ -3 +2,0 @@
-3
@@ -7 +5,0 @@
-7
)");

    std::stringstream input_file(R"(1
2
3
4
5
6
7
8
9
)");

    std::stringstream expected_output(R"(1
2
4
5
6
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, TwoLinesAddedWithNoContext)
{
    std::stringstream patch_file(R"(
--- /tmp/2.txt	2022-09-19 11:24:40.285189350 +1200
+++ /tmp/1.txt	2022-09-19 11:24:50.305063358 +1200
@@ -2,0 +3 @@
+3
@@ -5,0 +7 @@
+7
)");

    std::stringstream input_file(R"(1
2
4
5
6
8
9
)");

    std::stringstream expected_output(R"(1
2
3
4
5
6
7
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, My1234567)
{
    std::stringstream patch_file(R"(--- 1	2022-09-18 15:44:34.900520464 +1200
+++ 2	2022-09-18 15:44:50.319066119 +1200
@@ -5 +4,0 @@
-5
)");

    std::stringstream input_file(R"(1
2
3
4
5
6
7
8
9
)");

    std::stringstream expected_output(R"(1
2
3
4
6
7
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, RemoveOnelinePatchNoContext)
{
    std::stringstream patch_file(R"(
--- main2.cpp	2022-08-21 14:19:47.509561172 +1200
+++ main1.cpp	2022-08-21 14:35:06.584242817 +1200
@@ -3 +2,0 @@
-    return 0;
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, AddTwoLinePatch)
{
    std::stringstream patch_file(R"(
--- a/add_two_lines.cpp
+++ b/add_two_lines.cpp
@@ -1,3 +1,5 @@
 int main()
 {
+    int x = 1 + 2 - 3;
+    return x;
 }
)");

    std::stringstream input_file(R"(int main()
{
}
)");

    std::stringstream expected_output(R"(int main()
{
    int x = 1 + 2 - 3;
    return x;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, RemoveOneLinePatch)
{
    std::stringstream patch_file(R"(
--- a/only_remove_return.cpp
+++ b/only_remove_return.cpp
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, RemoveTwoLinePatch)
{
    std::stringstream patch_file(R"(
--- a/remove_two_lines.cpp
+++ b/remove_two_lines.cpp
@@ -1,5 +1,3 @@
 int main()
 {
-    int x = 1 + 2 - 3;
-    return x;
 }
)");

    std::stringstream input_file(R"(int main()
{
    int x = 1 + 2 - 3;
    return x;
}
)");

    std::stringstream expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, NormalDiffRemoveTwoLines)
{
    std::stringstream patch_file(R"(3d2
< 3
7d5
< 7
)");

    std::stringstream input_file(R"(1
2
3
4
5
6
7
8
9
)");

    std::stringstream expected_output(R"(1
2
4
5
6
8
9
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, NormalDiffChangeLines)
{
    std::stringstream patch_file(R"(3c3
< 3
---
> 3a
7d6
< 7
)");

    std::stringstream input_file(R"(1
2
3
4
5
6
7
8
9
)");

    std::stringstream expected_output(R"(1
2
3a
4
5
6
8
9
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, AddOneRemoveOnePatch)
{
    std::stringstream patch_file(R"(
--- a/add_one_remove_one.cpp
+++ b/add_one_remove_one.cpp
@@ -1,4 +1,4 @@
 int main()
 {
-    return 1;
+    return 0;
 }
)");

    std::stringstream input_file(R"(int main()
{
    return 1;
}
)");

    std::stringstream expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, MultipleHunks)
{
    std::stringstream patch_file(R"(
--- a/two_hunks1.cpp	2022-06-06 10:24:53.821028961 +1200
+++ b/two_hunks2.cpp	2022-06-06 10:25:09.666166040 +1200
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
    // ensure that the trailing spaces remain
    ASSERT_EQ(patch_file.str().size(), 298);

    std::stringstream input_file(R"(int sum(int x, int y)
{
    return x + y;
}

// Some comment

int subtract(int x, int y)
{
    return x - y;
}

int main()
{
    return sum(sumbtract(1, 2), 3);
}
)");

    std::stringstream expected_output(R"(
// Some comment

int subtract(int x, int y)
{
    return x - y;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, MultipleHunksWithoutSpace)
{
    std::stringstream patch_file(R"(
--- a/two_hunks1.cpp	2022-06-06 10:24:53.821028961 +1200
+++ b/two_hunks2.cpp	2022-06-06 10:25:09.666166040 +1200
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

    std::stringstream input_file(R"(int sum(int x, int y)
{
    return x + y;
}

// Some comment

int subtract(int x, int y)
{
    return x - y;
}

int main()
{
    return sum(sumbtract(1, 2), 3);
}
)");

    std::stringstream expected_output(R"(
// Some comment

int subtract(int x, int y)
{
    return x - y;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, ContextPatch)
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

    std::stringstream input_file(R"(int main()
{
}
)");

    std::stringstream expected_output(R"(int main()
{
	return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, MultiContextPatch)
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

    std::stringstream input_file(R"(#include <string>

std::string something()
{
    return "some data?!";
}

int some_negative(int a, int b)
{
    int c = a - b;
    return c;
}

int some_addition(int a, int b)
{
    int c = a + b;
    return c;
}

int main()
{
    printf("This is a hello world!\n");
    return -1;
}
)");

    std::stringstream expected_output(R"(#include <string>

std::string something()
{
    return "some data?!";
}

int some_addition(int a, int b)
{
    int c = a + b;
    return c;
}

int main()
{
    printf("This is a hello world!\n");
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, ContextDiffWithChangedLine)
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

    std::stringstream input_file(R"(int subtract(int a, int b)
{
	return a - b;
}

int main()
{
	return 0;
}
)");

    std::stringstream expected_output(R"(int plus(int a, int b)
{
	return a + b;
}

int subtract(int a, int b)
{
	return a - b;
}

int main()
{
	return 1;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, ContextDiffWithOffset)
{
    std::stringstream patch_file(R"(
--- a.c	2022-10-23 21:25:13.856207590 +1300
+++ b.c	2022-10-23 21:25:25.511912172 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-	return 0;
+	return 1;
 }
)");

    // There is a newline at the top of the file in the input, which is not present in the patch
    std::stringstream input_file(R"(
int main()
{
	return 0;
}
)");

    std::stringstream expected_output(R"(
int main()
{
	return 1;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, UnifiedDiffWithOnlyWhitespaceChangeIgnoreWhitespace)
{
    std::stringstream patch_file(R"(
--- main1.cpp	2022-01-29 16:51:39.296260776 +1300
+++ main2.cpp	2022-01-29 16:51:53.447839888 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-	return 0;
+    return 0;
 }
)");

    std::stringstream input_file(R"(int main()
{
	return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Options options;
    options.ignore_whitespace = true;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, UnifiedDiffIgnoreWhitespace)
{
    std::stringstream patch_file(R"(
--- main1.cpp	2022-01-29 16:54:43.523734499 +1300
+++ main2.cpp	2022-01-29 16:54:57.691462273 +1300
@@ -1,5 +1,5 @@
 int main()
 {
     int x = 0;
-	return 0;
+    return 0;
 }
)");

    // Different indentation for x in input file than what patch says.
    std::stringstream input_file(R"(int main()
{
    int x  =  0;
	return 0;
}
)");

    // Keep the changes from the input file instead of using what's in the patch
    std::stringstream expected_output(R"(int main()
{
    int x  =  0;
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Options options;
    options.ignore_whitespace = true;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, UnifiedDiffMissingNewLineAtEndOfFile)
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

    std::stringstream input_file(R"(int main()
{
    return 0;
})");

    std::stringstream expected_output(R"(int main()
{
    return 1;
})");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, SingleLineAdditionFromEmptyFile)
{
    std::stringstream patch_file(R"(
--- test1	2022-05-28 16:16:12.487752844 +1200
+++ test	2022-05-28 16:16:08.015691593 +1200
@@ -0,0 +1 @@
+new line from empty
)");

    std::stringstream input_file(R"()");

    std::stringstream expected_output(R"(new line from empty
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Applier, MultiLineAdditionFromEmptyFile)
{
    std::stringstream patch_file(R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ new_file.cpp	2022-05-28 19:23:11.013618316 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)");

    std::stringstream input_file(R"()");

    std::stringstream expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream reject;
    std::stringstream output;

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}
