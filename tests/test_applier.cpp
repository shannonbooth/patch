// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/process.h>
#include <patch/test.h>

PATCH_TEST(applier_add_oneline_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a/only_add_return.cpp
+++ b/only_add_return.cpp
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)";
    }

    {
        Patch::File file("only_add_return.cpp", std::ios_base::out);
        file << R"(int main()
{
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file only_add_return.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("only_add_return.cpp", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(applier_add_oneline_normal_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(2a3
>     return 0;
)";
    }

    {
        Patch::File file("test", std::ios_base::out);
        file << R"(int main()
{
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "test", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file test\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("test", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(applier_remove_oneline_normal_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(3d2
<     return 0;
)";
    }

    {
        Patch::File file("test", std::ios_base::out);
        file << R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "test", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file test\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("test", R"(int main()
{
}
)");
}

PATCH_TEST(applier_add_oneline_patch_no_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main1.cpp	2022-08-21 14:35:06.584242817 +1200
+++ main2.cpp	2022-08-21 14:19:47.509561172 +1200
@@ -2,0 +3 @@
+    return 0;
)";
    }

    {
        Patch::File file("main1.cpp", std::ios_base::out);
        file << R"(int main()
{
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file main1.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main1.cpp", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(applier_remove_oneline_patch_two_lines_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main2.cpp	2022-09-18 14:35:15.923795777 +1200
+++ main.cpp	2022-09-18 14:35:12.460038934 +1200
@@ -2,3 +2,2 @@
 {
-    return 0;
 }
)";
    }

    {
        Patch::File file("main2.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file main2.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main2.cpp", R"(int main()
{
}
)");
}

PATCH_TEST(applier_my_second1234567)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- 1	2022-09-18 15:44:34.900520464 +1200
+++ 2	2022-09-18 15:44:50.319066119 +1200
@@ -4,3 +4,2 @@
 4
-5
 6
)";
    }

    {
        Patch::File file("1", std::ios_base::out);

        file << R"(1
2
3
4
5
6
7
8
9
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file 1\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("1", R"(1
2
3
4
6
7
8
9
)");
}

PATCH_TEST(applier_two_lines_removed_with_no_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- /tmp/1.txt	2022-09-19 11:24:50.305063358 +1200
+++ /tmp/2.txt	2022-09-19 11:24:40.285189350 +1200
@@ -3 +2,0 @@
-3
@@ -7 +5,0 @@
-7
)";
    }

    {
        Patch::File file("1.txt", std::ios_base::out);
        file << R"(1
2
3
4
5
6
7
8
9
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file 1.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("1.txt", R"(1
2
4
5
6
8
9
)");
}

PATCH_TEST(applier_two_lines_added_with_no_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- /tmp/2.txt	2022-09-19 11:24:40.285189350 +1200
+++ /tmp/1.txt	2022-09-19 11:24:50.305063358 +1200
@@ -2,0 +3 @@
+3
@@ -5,0 +7 @@
+7
)";
    }

    {
        Patch::File file("2.txt", std::ios_base::out);
        file << R"(1
2
4
5
6
8
9
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file 2.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("2.txt", R"(1
2
3
4
5
6
7
8
9
)");
}

PATCH_TEST(applier_my_1234567)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- 1	2022-09-18 15:44:34.900520464 +1200
+++ 2	2022-09-18 15:44:50.319066119 +1200
@@ -5 +4,0 @@
-5
)";
    }

    {
        Patch::File file("1", std::ios_base::out);
        file << R"(1
2
3
4
5
6
7
8
9
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file 1\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("1", R"(1
2
3
4
6
7
8
9
)");
}

PATCH_TEST(applier_remove_oneline_patch_no_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main2.cpp	2022-08-21 14:19:47.509561172 +1200
+++ main1.cpp	2022-08-21 14:35:06.584242817 +1200
@@ -3 +2,0 @@
-    return 0;
)";
    }

    {
        Patch::File file("main2.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file main2.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main2.cpp", R"(int main()
{
}
)");
}

PATCH_TEST(applier_add_two_line_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a/add_two_lines.cpp
+++ b/add_two_lines.cpp
@@ -1,3 +1,5 @@
 int main()
 {
+    int x = 1 + 2 - 3;
+    return x;
 }
)";
    }

    {
        Patch::File file("add_two_lines.cpp", std::ios_base::out);
        file << R"(int main()
{
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file add_two_lines.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("add_two_lines.cpp", R"(int main()
{
    int x = 1 + 2 - 3;
    return x;
}
)");
}

PATCH_TEST(applier_remove_one_line_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a/only_remove_return.cpp
+++ b/only_remove_return.cpp
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)";
    }

    {
        Patch::File file("only_remove_return.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file only_remove_return.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("only_remove_return.cpp", R"(int main()
{
}
)");
}

PATCH_TEST(applier_remove_two_line_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a/remove_two_lines.cpp
+++ b/remove_two_lines.cpp
@@ -1,5 +1,3 @@
 int main()
 {
-    int x = 1 + 2 - 3;
-    return x;
 }
)";
    }

    {
        Patch::File file("remove_two_lines.cpp", std::ios_base::out);
        file << R"(int main()
{
    int x = 1 + 2 - 3;
    return x;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file remove_two_lines.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("remove_two_lines.cpp", R"(int main()
{
}
)");
}

PATCH_TEST(applier_normal_diff_remove_two_lines)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(3d2
< 3
7d5
< 7
)";
    }

    {
        Patch::File file("normal_content", std::ios_base::out);
        file << R"(1
2
3
4
5
6
7
8
9
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "normal_content", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file normal_content\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("normal_content", R"(1
2
4
5
6
8
9
)");
}

PATCH_TEST(applier_normal_diff_change_lines)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(3c3
< 3
---
> 3a
7d6
< 7
)";
    }

    {
        Patch::File file("normal_stuff", std::ios_base::out);
        file << R"(1
2
3
4
5
6
7
8
9
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "normal_stuff", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file normal_stuff\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("normal_stuff", R"(1
2
3a
4
5
6
8
9
)");
}

PATCH_TEST(applier_add_one_remove_one_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a/add_one_remove_one.cpp
+++ b/add_one_remove_one.cpp
@@ -1,4 +1,4 @@
 int main()
 {
-    return 1;
+    return 0;
 }
)";
    }

    {
        Patch::File file("add_one_remove_one.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 1;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file add_one_remove_one.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("add_one_remove_one.cpp", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(applier_multiple_hunks)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
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
)";
    }

    {
        Patch::File file("two_hunks2.cpp", std::ios_base::out);
        file << R"(int sum(int x, int y)
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
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file two_hunks2.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("two_hunks2.cpp", R"(
// Some comment

int subtract(int x, int y)
{
    return x - y;
}
)");
}

PATCH_TEST(applier_multiple_hunks_without_space)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
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
)";
    }

    {
        Patch::File file("two_hunks2.cpp", std::ios_base::out);
        file << R"(int sum(int x, int y)
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
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file two_hunks2.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("two_hunks2.cpp", R"(
// Some comment

int subtract(int x, int y)
{
    return x - y;
}
)");
}

PATCH_TEST(applier_context_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
*** test1_input.cpp	2022-06-19 17:14:31.743526819 +1200
--- test1_output.cpp	2022-06-19 17:14:31.743526819 +1200
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+ 	return 0;
  }
)";
    }

    {
        Patch::File file("test1_input.cpp", std::ios_base::out);
        file << R"(int main()
{
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file test1_input.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("test1_input.cpp", R"(int main()
{
	return 0;
}
)");
}

PATCH_TEST(applier_multi_context_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
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
    }

    {
        Patch::File file("main2.cpp", std::ios_base::out);
        file << R"(#include <string>

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
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file main2.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main2.cpp", R"(#include <string>

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
}

PATCH_TEST(applier_context_diff_with_changed_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
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
    }

    {
        Patch::File file("main1.cpp", std::ios_base::out);
        file << R"(int subtract(int a, int b)
{
	return a - b;
}

int main()
{
	return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file main1.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main1.cpp", R"(int plus(int a, int b)
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
}

PATCH_TEST(applier_context_diff_with_offset)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a.c	2022-10-23 21:25:13.856207590 +1300
+++ b.c	2022-10-23 21:25:25.511912172 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-	return 0;
+	return 1;
 }
)";
    }

    // There is a newline at the top of the file in the input, which is not present in the patch
    {
        Patch::File file("a.c", std::ios_base::out);
        file << R"(
int main()
{
	return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a.c\nHunk #1 succeeded at 2 (offset 1 line).\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("a.c", R"(
int main()
{
	return 1;
}
)");
}

PATCH_TEST(applier_unified_diff_with_only_whitespace_change_ignore_whitespace)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main1.cpp	2022-01-29 16:51:39.296260776 +1300
+++ main2.cpp	2022-01-29 16:51:53.447839888 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-	return 0;
+    return 0;
 }
)";
    }

    {
        Patch::File file("main1.cpp", std::ios_base::out);
        file << R"(int main()
{
	return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "--ignore-whitespace", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file main1.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main1.cpp", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(applier_unified_diff_ignore_whitespace)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main1.cpp	2022-01-29 16:54:43.523734499 +1300
+++ main2.cpp	2022-01-29 16:54:57.691462273 +1300
@@ -1,5 +1,5 @@
 int main()
 {
     int x = 0;
-	return 0;
+    return 0;
 }
)";
    }

    // Different indentation for x in input file than what patch says.
    {
        Patch::File file("main1.cpp", std::ios_base::out);
        file << R"(int main()
{
    int x  =  0;
	return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "--ignore-whitespace", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file main1.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    // Keep the changes from the input file instead of using what's in the patch
    EXPECT_FILE_EQ("main1.cpp", R"(int main()
{
    int x  =  0;
    return 0;
}
)");
}

PATCH_TEST(applier_unified_diff_missing_new_line_at_end_of_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- no_newline1.cpp	2022-01-30 13:57:31.173528027 +1300
+++ no_newline2.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
+    return 1;
 }
\ No newline at end of file
)";
    }

    {
        Patch::File file("no_newline1.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 0;
})";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file no_newline1.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("no_newline1.cpp", R"(int main()
{
    return 1;
})");
}

PATCH_TEST(applier_single_line_addition_from_empty_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- test1	2022-05-28 16:16:12.487752844 +1200
+++ test	2022-05-28 16:16:08.015691593 +1200
@@ -0,0 +1 @@
+new line from empty
)";
    }

    Patch::File::touch("test1");

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file test1\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("test1", "new line from empty\n");
}

PATCH_TEST(applier_multi_line_addition_from_empty_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ new_file.cpp	2022-05-28 19:23:11.013618316 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file new_file.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("new_file.cpp", R"(int main()
{
}
)");
}
