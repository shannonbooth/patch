// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/test.h>

PATCH_TEST(defines_add_one_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+    return 1;
 }
)";
    }

    {
        Patch::File file("file.cpp", std::ios_base::out);
        file << R"(int main()
{
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ifdef", "TEST_PATCH", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file file.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("file.cpp", R"(int main()
{
#ifdef TEST_PATCH
    return 1;
#endif
}
)");
}

PATCH_TEST(defines_remove_one_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)";
    }

    {
        Patch::File file("file.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ifdef", "TEST_PATCH", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file file.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("file.cpp", R"(int main()
{
#ifndef TEST_PATCH
    return 0;
#endif
}
)");
}

PATCH_TEST(defines_remove_add)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
+    return 1;
 }
)";
    }

    {
        Patch::File file("file.cpp", std::ios_base::out);
        file <<
            R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ifdef", "TEST_PATCH", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file file.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("file.cpp", R"(int main()
{
#ifndef TEST_PATCH
    return 0;
#else
    return 1;
#endif
}
)");
}

PATCH_TEST(defines_add_remove)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,4 @@
 int main()
 {
+    return 1;
-    return 0;
 }
)";
    }

    {
        Patch::File file("file.cpp", std::ios_base::out);
        file <<
            R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ifdef", "TEST_PATCH", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file file.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    // Allow either of these forms as they are (effectively) equivalent.
    try {
        EXPECT_FILE_EQ("file.cpp", R"(int main()
{
#ifdef TEST_PATCH
    return 1;
#else
    return 0;
#endif
}
)");
    } catch (const Patch::test_assertion_failure&) {
        EXPECT_FILE_EQ("file.cpp", R"(int main()
{
#ifndef TEST_PATCH
    return 0;
#else
    return 1;
#endif
}
)");
    }
}

PATCH_TEST(defines_changes_at_end_of_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main.cpp	2022-02-20 19:38:33.685996435 +1300
+++ main.cpp	2022-02-20 19:38:48.069436313 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
-}
+    return 1;
+} // some change to this last line.
)";
    }

    {
        Patch::File file("main.cpp", std::ios_base::out);
        file << R"(int main()
{
    return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ifdef", "TEST_PATCH", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file main.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("main.cpp", R"(int main()
{
#ifndef TEST_PATCH
    return 0;
}
#else
    return 1;
} // some change to this last line.
#endif
)");
}

PATCH_TEST(defines_more_complex_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- add.cpp	2022-02-26 13:15:21.624635744 +1300
+++ sub.cpp	2022-02-26 13:15:07.396899118 +1300
@@ -1,17 +1,17 @@
-int add(int a, int b)
+int subtract(int a, int b)
 {
-	return a + b;
+	return a - b;
 }
 
-int subtract(int a, int b)
+int add(int a, int b)
 {
-	return a - b;
+	return a + b;
 }
 
-// To be removed comment
 int main()
 {
 	int x = add(1, 2);
 	int y = subtract(3, 4);
+	// Some comment
 	return x + y;
 }
)";
    }

    {
        Patch::File file("add.cpp", std::ios_base::out);
        file << R"(int add(int a, int b)
{
	return a + b;
}

int subtract(int a, int b)
{
	return a - b;
}

// To be removed comment
int main()
{
	int x = add(1, 2);
	int y = subtract(3, 4);
	return x + y;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ifdef", "MY_PATCH", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file add.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("add.cpp", R"(#ifndef MY_PATCH
int add(int a, int b)
#else
int subtract(int a, int b)
#endif
{
#ifndef MY_PATCH
	return a + b;
#else
	return a - b;
#endif
}

#ifndef MY_PATCH
int subtract(int a, int b)
#else
int add(int a, int b)
#endif
{
#ifndef MY_PATCH
	return a - b;
#else
	return a + b;
#endif
}

#ifndef MY_PATCH
// To be removed comment
#endif
int main()
{
	int x = add(1, 2);
	int y = subtract(3, 4);
#ifdef MY_PATCH
	// Some comment
#endif
	return x + y;
}
)");
}
