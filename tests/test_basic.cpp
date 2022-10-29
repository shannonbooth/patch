// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/system.h>
#include <process.h>
#include <test.h>

PATCH_TEST(basic)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_FILE_EQ("to_patch", R"(int main()
{
	return 0;
}
)");

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

PATCH_TEST(basic_context_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
*** a	2022-06-19 20:26:52.280538615 +1200
--- b	2022-06-19 20:26:59.968648316 +1200
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }

)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(normal_patch_trailing_newlines)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(2d1
< 2

)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "a", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n3\n");
}

PATCH_TEST(less_basic_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- x   2022-10-10 20:45:19.577907405 +1300
+++ x   2022-10-10 13:03:25.792099411 +1300
@@ -1,9 +1,8 @@
 1
 2
-3
+3a
 4
 5
 6
-7
 8
 9
)";
        file.close();
    }

    {
        Patch::File file("x", std::ios_base::out);
        file << "1\n2\n3\n4\n5\n6\n7\n8\n9\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file x\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x", "1\n2\n3a\n4\n5\n6\n8\n9\n");
}

PATCH_TEST(basic_patch_with_dryrun)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

)";
        file.close();
    }

    std::string to_patch = R"(int main()
{
}
)";
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", nullptr });

    EXPECT_EQ(process.stdout_data(), "checking file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", to_patch);
}

PATCH_TEST(basic_patch_with_dry_run_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

)";
        file.close();
    }

    std::string to_patch = R"(int main()
{
}
)";
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", "-o", "-", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(int main()
{
	return 0;
}
)");
    EXPECT_EQ(process.stderr_data(), "checking file - (read from to_patch)\n");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", to_patch);
}

PATCH_TEST(failed_patch_dry_run)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- 1	2022-06-26 12:22:22.161398905 +1200
+++ 2	2022-06-26 12:22:44.105278030 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
)";
        file.close();
    }

    std::string to_patch = "a\nb\nc\n";
    {
        Patch::File file("1", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", "--force", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(checking file 1
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("1", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("1.rej"));
    EXPECT_FALSE(Patch::filesystem::exists("1.orig"));
}

PATCH_TEST(set_patch_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-06-19 16:56:12.974516527 +1200
+++ b	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
)";
        file.close();
    }

    {
        Patch::File file("c", std::ios_base::out);
        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "c", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file c\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("c", R"(int main()
{
	return 0;
}
)");
}

PATCH_TEST(add_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ add	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("add"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file add\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("add", R"(int main()
{
}
)");
}

PATCH_TEST(add_file_using_basename)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ a/b/c/d/e	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("e"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file e\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("e", R"(int main()
{
}
)");
}

PATCH_TEST(add_file_missing_folders)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ a/b/c/d/e	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("a"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-p0", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a/b/c/d/e\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a/b/c/d/e", R"(int main()
{
}
)");
}

PATCH_TEST(basic_output_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/x.cpp	2022-06-10 19:28:11.018017172 +1200
+++ b/y.cpp	2022-06-10 19:28:21.841903003 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 1;
 }
)";
        file.close();
    }

    {
        Patch::File file("x.cpp", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-o", "-", nullptr });

    EXPECT_EQ(process.stderr_data(), "patching file - (read from x.cpp)\n");
    EXPECT_EQ(process.stdout_data(), R"(int main()
{
	return 1;
}
)");
    EXPECT_EQ(process.return_code(), 0);
}

PATCH_TEST(basic_unicode_patch)
{
    {
        Patch::File file("ハローワールド.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "ハローワールド.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", R"(int main()
{
	return 0;
}
)");
}

PATCH_TEST(write_empty_output_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-07-02 15:23:07.929349813 +1200
+++ /dev/null	2022-06-30 20:33:13.470250591 +1200
@@ -1 +0,0 @@
-1
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << "1\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-o", "-", nullptr });

    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), "patching file - (read from a)\n");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n");
}

PATCH_TEST(remove_file_in_folders)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/b/c/d/e	2022-07-03 14:32:47.081054897 +1200
+++ /dev/null	2022-06-30 20:33:13.470250591 +1200
@@ -1 +0,0 @@
-1

)";
        file.close();
    }

    const std::string file_path = "a/b/c/d/e";
    Patch::ensure_parent_directories(file_path);

    {
        Patch::File file(file_path, std::ios_base::out);

        file << "1\n";
        file.close();
    }

    // Create a second file which should not be removed
    const std::string second_file_contents = "some stuff!\n";
    {
        Patch::File file("a/1", std::ios_base::out);

        file << second_file_contents;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-p0", "--", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a/b/c/d/e\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::exists("a/b/c/d/e"));
    EXPECT_FALSE(Patch::filesystem::exists("a/b/c/d"));
    EXPECT_FALSE(Patch::filesystem::exists("a/b/c"));
    EXPECT_FALSE(Patch::filesystem::exists("a/b"));
    EXPECT_TRUE(Patch::filesystem::exists("a"));
    EXPECT_FILE_EQ("a/1", second_file_contents);
}

PATCH_TEST(remove_file_successfully)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_remove	2022-06-06 14:40:51.547454892 +1200
+++ /dev/null	2022-06-01 16:08:26.275921370 +1200
@@ -1,3 +0,0 @@
-int main()
-{
-}
)";
        file.close();
    }

    {
        Patch::File file("to_remove", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    EXPECT_TRUE(Patch::filesystem::exists("to_remove"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_remove\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::exists("to_remove"));
}

PATCH_TEST(remove_file_that_has_trailing_garbage)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- remove	2022-06-06 14:40:51.547454892 +1200
+++ /dev/null	2022-06-01 16:08:26.275921370 +1200
@@ -1,3 +0,0 @@
-int main()
-{
-}

)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
}
// some trailing garbage
)";
    {
        Patch::File file("remove", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patching file remove
Not deleting file remove as content differs from patch
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("remove", "// some trailing garbage\n");
}

PATCH_TEST(remove_git_submodule)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/.gitmodules b/.gitmodules
index 066b99a..e69de29 100644
--- a/.gitmodules
+++ b/.gitmodules
@@ -1,3 +0,0 @@
-[submodule "libarchive"]
-	path = libarchive
-	url = https://github.com/libarchive/libarchive.git
diff --git a/libarchive b/libarchive
deleted file mode 160000
index a45905b..0000000
--- a/libarchive
+++ /dev/null
@@ -1 +0,0 @@
-Subproject commit a45905b0166713760a2fb4f2e908d7ce47488371
)";
        file.close();
    }

    {
        Patch::File file(".gitmodules", std::ios_base::out);

        file << R"([submodule "libarchive"]
	path = libarchive
	url = https://github.com/libarchive/libarchive.git
)";
        file.close();
    }

    EXPECT_TRUE(Patch::filesystem::create_directory("libarchive"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patching file .gitmodules
File libarchive is not a regular file -- refusing to patch
1 out of 1 hunk ignored -- saving rejects to file libarchive.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
}

PATCH_TEST(git_binary_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
From f933cb15f717a43ef1961d797874ca4a5650ff08 Mon Sep 17 00:00:00 2001
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
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "File a.txt: git binary diffs are not supported.\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
}

PATCH_TEST(basic_unicode_patch_filepaths)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- "\327\251\327\234\327\225\327\235 \327\242\327\225\327\234\327\235!"	2022-09-03 14:51:28.429821767 +1200
+++ another	2022-09-03 14:52:15.250024346 +1200
@@ -1,3 +1,2 @@
 a
-b
 c
)";
        file.close();
    }

    {
        Patch::File file("שלום עולם!", std::ios_base::out);
        file << "a\nb\nc\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file 'שלום עולם!'\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("שלום עולם!", "a\nc\n");
}

PATCH_TEST(override_reject_file)
{
    const std::string patch = R"(--- a	2022-07-08 10:34:03.860546761 +1200
+++ b	2022-07-08 10:34:20.096714313 +1200
@@ -1,3 +1,3 @@
-a
-b
-c
+1
+2
+3
)";

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << patch;
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--force", "-r", "override.rej", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file a
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file override.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("override.rej", patch);
}

PATCH_TEST(reject_format_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/reject	2022-07-30 11:40:37.280248088 +1200
+++ b/reject	2022-07-30 11:40:49.032177150 +1200
@@ -1,3 +1,3 @@
 abc
-def
+123
 ghi
)";
        file.close();
    }

    const std::string to_patch = "apc\nzxy\nghi\n";

    {
        Patch::File file("reject", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--reject-format", "context", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file reject
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file reject.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("reject", to_patch);
    EXPECT_FILE_EQ("reject.rej", R"(*** reject	2022-07-30 11:40:37.280248088 +1200
--- reject	2022-07-30 11:40:49.032177150 +1200
***************
*** 1,3 ****
  abc
! def
  ghi
--- 1,3 ----
  abc
! 123
  ghi
)");
}

PATCH_TEST(unicode_rename_git_no_quoting)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
commit f3f4654d6a154907dbd36c47d49c910b0c10c072
Author: Shannon Booth <shannon.ml.booth@gmail.com>
Date:   Sun Sep 4 11:03:05 2022 +1200

    Add unicode path using core.quotePath false

diff --git a/file b/지배
similarity index 66%
rename from file
rename to 지배
index de98044..0f7bc76 100644
--- a/նախքան
+++ b/지배
@@ -1,3 +1,2 @@
 Мир
-b
 c
)";
        file.close();
    }

    {
        Patch::File file("նախքան", std::ios_base::out);
        file << "Мир\nb\nc\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-idiff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file 지배 (renamed from նախքան)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("지배", "Мир\nc\n");
}

static void test_chdir(const char* patch_path, const std::string& folder)
{
    EXPECT_TRUE(Patch::filesystem::create_directory(folder));

    {
        Patch::File file(folder + "/diff.patch", std::ios_base::out);

        file << R"(
--- 1	2022-06-26 11:17:58.948060133 +1200
+++ 2	2022-06-26 11:18:03.500001858 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
)";
        file.close();
    }

    {
        Patch::File file(folder + "/1", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-d", folder.c_str(), nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file 1\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ(folder + "/1", "1\n3\n");
}

PATCH_TEST(chdir_unicode)
{
    test_chdir(patch_path, "गिलास");
}

PATCH_TEST(chdir_good_case)
{
    test_chdir(patch_path, "folder");
}
