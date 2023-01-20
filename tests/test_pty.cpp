// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/pty_spawn.h>
#include <patch/test.h>

PATCH_TEST(pty_reversed_patch_answer_yes)
{
    {
        Patch::File file("a", std::ios_base::out);
        file << "1\nb\n3\n";
        file.close();
    }

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-09-27 19:32:58.112960376 +1300
+++ b	2022-09-27 19:33:04.088209645 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "y\n");

    EXPECT_EQ(term.output(), R"(patching file a
Reversed (or previously applied) patch detected!  Assume -R? [n] )");
    EXPECT_FILE_EQ("a", "1\n2\n3\n");
    EXPECT_EQ(term.return_code(), 0);
}

PATCH_TEST(pty_file_not_found)
{
    {
        Patch::File file("file", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- a	2023-01-03 11:23:35.634966282 +1300
+++ b	2023-01-03 11:23:41.598960625 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "file\n");
    EXPECT_EQ(term.output(), R"(can't find file to patch at input line 3
Perhaps you should have used the -p or --strip option?
The text leading up to this was:
--------------------------
|--- a	2023-01-03 11:23:35.634966282 +1300
|+++ b	2023-01-03 11:23:41.598960625 +1300
--------------------------
File to patch: patching file file
)");
    EXPECT_EQ(term.return_code(), 0);
}

PATCH_TEST(pty_file_not_found_verbose)
{
    {
        Patch::File file("file", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- a	2023-01-03 11:23:35.634966282 +1300
+++ b	2023-01-03 11:23:41.598960625 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", "--verbose", nullptr }, "file\n");
    EXPECT_EQ(term.output(), R"(Hmm...  Looks like a unified diff to me...
can't find file to patch at input line 3
Perhaps you should have used the -p or --strip option?
The text leading up to this was:
--------------------------
|--- a	2023-01-03 11:23:35.634966282 +1300
|+++ b	2023-01-03 11:23:41.598960625 +1300
--------------------------
File to patch: patching file file
Using Plan A...
Hunk #1 succeeded at 1.
done
)");
    EXPECT_EQ(term.return_code(), 0);
}

PATCH_TEST(pty_file_not_found_strip_option_given)
{
    {
        Patch::File file("file", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- a/a	2023-01-03 11:23:35.634966282 +1300
+++ b/b	2023-01-03 11:23:41.598960625 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", "--strip=0", nullptr }, "file\n");
    EXPECT_EQ(term.output(), R"(can't find file to patch at input line 3
Perhaps you used the wrong -p or --strip option?
The text leading up to this was:
--------------------------
|--- a/a	2023-01-03 11:23:35.634966282 +1300
|+++ b/b	2023-01-03 11:23:41.598960625 +1300
--------------------------
File to patch: patching file file
)");
    EXPECT_EQ(term.return_code(), 0);
}

PATCH_TEST(pty_file_not_found_skip_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- a/a	2023-01-03 11:23:35.634966282 +1300
+++ b/b	2023-01-03 11:23:41.598960625 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "some file name that does not exist!\ny\n");
    EXPECT_EQ(term.output(), R"(can't find file to patch at input line 3
Perhaps you should have used the -p or --strip option?
The text leading up to this was:
--------------------------
|--- a/a	2023-01-03 11:23:35.634966282 +1300
|+++ b/b	2023-01-03 11:23:41.598960625 +1300
--------------------------
File to patch: some file name that does not exist!: No such file or directory
Skip this patch? [y] Skipping patch.
1 out of 1 hunk ignored
)");
    EXPECT_EQ(term.return_code(), 1);
}

PATCH_TEST(pty_file_not_found_two_patches_skip_both)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- 1	2023-01-06 12:55:54.499099611 +1300
+++ 2	2023-01-06 12:56:01.379092530 +1300
@@ -1,3 +1,3 @@
 1
-2
-3
+b
+c
--- a	2023-01-06 12:56:26.595066547 +1300
+++ b	2023-01-06 12:56:36.727056093 +1300
@@ -1,3 +1,3 @@
 qwerty
-asdfgh
+aSdFgH
 zxcvbn
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "some file name that does not exist!\ny\nanother\ny\n");
    EXPECT_EQ(term.output(), R"(can't find file to patch at input line 3
Perhaps you should have used the -p or --strip option?
The text leading up to this was:
--------------------------
|--- 1	2023-01-06 12:55:54.499099611 +1300
|+++ 2	2023-01-06 12:56:01.379092530 +1300
--------------------------
File to patch: some file name that does not exist!: No such file or directory
Skip this patch? [y] Skipping patch.
1 out of 1 hunk ignored
can't find file to patch at input line 11
Perhaps you should have used the -p or --strip option?
The text leading up to this was:
--------------------------
|--- a	2023-01-06 12:56:26.595066547 +1300
|+++ b	2023-01-06 12:56:36.727056093 +1300
--------------------------
File to patch: another: No such file or directory
Skip this patch? [y] Skipping patch.
1 out of 1 hunk ignored
)");
    EXPECT_EQ(term.return_code(), 1);
}

PATCH_TEST(pty_file_cannot_apply_forwards_but_can_reversed_with_fuzz)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- X	2023-01-06 12:55:54.499099611 +1300
+++ a	2023-01-06 12:56:01.379092530 +1300
@@ -1,3 +1,3 @@
 1
-2
-3
+b
+c
)";
        file.close();
    }

    const std::string content = "a\nb\nc\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << content;
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "n\nn\n");
    EXPECT_EQ(term.output(), R"(patching file a
Reversed (or previously applied) patch detected!  Assume -R? [n] Apply anyway? [n] Skipping patch.
1 out of 1 hunk ignored -- saving rejects to file a.rej
)");
    EXPECT_EQ(term.return_code(), 1);
}

PATCH_TEST(pty_patch_is_reversed_apply_anyway)
{
    const std::string patch = R"(--- a	2023-01-08 10:32:38.112719302 +1300
+++ b	2023-01-08 10:32:44.400714493 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";

    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << patch;
        file.close();
    }

    const std::string content = "1\nb\n3\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << content;
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "n\ny\n");
    EXPECT_EQ(term.output(), R"(patching file a
Reversed (or previously applied) patch detected!  Assume -R? [n] Apply anyway? [n] Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file a.rej
)");
    EXPECT_EQ(term.return_code(), 1);
    EXPECT_FILE_EQ("a", content);
    EXPECT_FILE_EQ("a.rej", patch);
}
