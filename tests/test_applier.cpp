// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/applier.h>
#include <patch/options.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <sstream>
#include <test.h>

TEST(applier_add_oneline_patch)
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
}
)");

    std::string expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_add_oneline_normal_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(2a3
>     return 0;
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
}
)");

    std::string expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_remove_oneline_normal_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(3d2
<     return 0;
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    return 0;
}
)");

    std::string expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();
    EXPECT_EQ(patch.hunks.size(), 1);

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_add_oneline_patch_no_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main1.cpp	2022-08-21 14:35:06.584242817 +1200
+++ main2.cpp	2022-08-21 14:19:47.509561172 +1200
@@ -2,0 +3 @@
+    return 0;
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
}
)");

    std::string expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_remove_oneline_patch_two_lines_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main2.cpp	2022-09-18 14:35:15.923795777 +1200
+++ main.cpp	2022-09-18 14:35:12.460038934 +1200
@@ -2,3 +2,2 @@
 {
-    return 0;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    return 0;
}
)");

    std::string expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_my_second1234567)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- 1	2022-09-18 15:44:34.900520464 +1200
+++ 2	2022-09-18 15:44:50.319066119 +1200
@@ -4,3 +4,2 @@
 4
-5
 6
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
3
4
5
6
7
8
9
)");

    std::string expected_output(R"(1
2
3
4
6
7
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File output = Patch::File::create_temporary();
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_two_lines_removed_with_no_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- /tmp/1.txt	2022-09-19 11:24:50.305063358 +1200
+++ /tmp/2.txt	2022-09-19 11:24:40.285189350 +1200
@@ -3 +2,0 @@
-3
@@ -7 +5,0 @@
-7
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
3
4
5
6
7
8
9
)");

    std::string expected_output(R"(1
2
4
5
6
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_two_lines_added_with_no_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- /tmp/2.txt	2022-09-19 11:24:40.285189350 +1200
+++ /tmp/1.txt	2022-09-19 11:24:50.305063358 +1200
@@ -2,0 +3 @@
+3
@@ -5,0 +7 @@
+7
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
4
5
6
8
9
)");

    std::string expected_output(R"(1
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
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_my_1234567)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(--- 1	2022-09-18 15:44:34.900520464 +1200
+++ 2	2022-09-18 15:44:50.319066119 +1200
@@ -5 +4,0 @@
-5
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
3
4
5
6
7
8
9
)");

    std::string expected_output(R"(1
2
3
4
6
7
8
9
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_remove_oneline_patch_no_context)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main2.cpp	2022-08-21 14:19:47.509561172 +1200
+++ main1.cpp	2022-08-21 14:35:06.584242817 +1200
@@ -3 +2,0 @@
-    return 0;
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    return 0;
}
)");

    std::string expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_add_two_line_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a/add_two_lines.cpp
+++ b/add_two_lines.cpp
@@ -1,3 +1,5 @@
 int main()
 {
+    int x = 1 + 2 - 3;
+    return x;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
}
)");

    std::string expected_output(R"(int main()
{
    int x = 1 + 2 - 3;
    return x;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_remove_one_line_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a/only_remove_return.cpp
+++ b/only_remove_return.cpp
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    return 0;
}
)");

    std::string expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_remove_two_line_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a/remove_two_lines.cpp
+++ b/remove_two_lines.cpp
@@ -1,5 +1,3 @@
 int main()
 {
-    int x = 1 + 2 - 3;
-    return x;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    int x = 1 + 2 - 3;
    return x;
}
)");

    std::string expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_normal_diff_remove_two_lines)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(3d2
< 3
7d5
< 7
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
3
4
5
6
7
8
9
)");

    std::string expected_output(R"(1
2
4
5
6
8
9
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_normal_diff_change_lines)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(3c3
< 3
---
> 3a
7d6
< 7
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
3
4
5
6
7
8
9
)");

    std::string expected_output(R"(1
2
3a
4
5
6
8
9
)");

    auto patch = Patch::parse_patch(patch_file, Patch::Format::Normal);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_add_one_remove_one_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- a/add_one_remove_one.cpp
+++ b/add_one_remove_one.cpp
@@ -1,4 +1,4 @@
 int main()
 {
-    return 1;
+    return 0;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    return 1;
}
)");

    std::string expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_multiple_hunks)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int sum(int x, int y)
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

    std::string expected_output(R"(
// Some comment

int subtract(int x, int y)
{
    return x - y;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_multiple_hunks_without_space)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int sum(int x, int y)
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

    std::string expected_output(R"(
// Some comment

int subtract(int x, int y)
{
    return x - y;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_context_patch)
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
}
)");

    std::string expected_output(R"(int main()
{
	return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File output = Patch::File::create_temporary();
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_multi_context_patch)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(#include <string>

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

    std::string expected_output(R"(#include <string>

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
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_context_diff_with_changed_line)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int subtract(int a, int b)
{
	return a - b;
}

int main()
{
	return 0;
}
)");

    std::string expected_output(R"(int plus(int a, int b)
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
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_context_diff_with_offset)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
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
    Patch::File input_file = Patch::File::create_temporary_with_content(R"(
int main()
{
	return 0;
}
)");

    std::string expected_output(R"(
int main()
{
	return 1;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_unified_diff_with_only_whitespace_change_ignore_whitespace)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main1.cpp	2022-01-29 16:51:39.296260776 +1300
+++ main2.cpp	2022-01-29 16:51:53.447839888 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-	return 0;
+    return 0;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
	return 0;
}
)");

    std::string expected_output(R"(int main()
{
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Options options;
    options.ignore_whitespace = true;

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_unified_diff_ignore_whitespace)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
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
    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    int x  =  0;
	return 0;
}
)");

    // Keep the changes from the input file instead of using what's in the patch
    std::string expected_output(R"(int main()
{
    int x  =  0;
    return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Options options;
    options.ignore_whitespace = true;

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_unified_diff_missing_new_line_at_end_of_file)
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

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
    return 0;
})");

    std::string expected_output(R"(int main()
{
    return 1;
})");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_single_line_addition_from_empty_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- test1	2022-05-28 16:16:12.487752844 +1200
+++ test	2022-05-28 16:16:08.015691593 +1200
@@ -0,0 +1 @@
+new line from empty
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"()");

    std::string expected_output(R"(new line from empty
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}

TEST(applier_multi_line_addition_from_empty_file)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ new_file.cpp	2022-05-28 19:23:11.013618316 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"()");

    std::string expected_output(R"(int main()
{
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);
    Patch::File output = Patch::File::create_temporary();

    Patch::Result result = Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.read_all_as_string(), expected_output);
    EXPECT_EQ(reject.read_all_as_string(), "");
    EXPECT_EQ(reject_writer.rejected_hunks(), 0);
}
