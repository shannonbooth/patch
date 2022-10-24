#include <patch/system.h>
PATCH_TEST(basic_output_to_stdout)

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