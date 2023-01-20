// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/pty_spawn.h>
#include <patch/test.h>

PATCH_TEST(prerequisite_is_met)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(Prereq: version-1.2.3
--- a	2023-01-18 18:46:47.745632233 +1300
+++ b	2023-01-18 18:46:53.339025728 +1300
@@ -3,5 +3,5 @@
 2
 3
 4
-5
+4
 6
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << R"(version-1.2.3
1
2
3
4
5
6
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", R"(version-1.2.3
1
2
3
4
4
6
)");
}

PATCH_TEST(prerequisite_is_not_met_continue_success)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(Prereq: version-1.2.4
--- a	2023-01-18 18:46:47.745632233 +1300
+++ b	2023-01-18 18:46:53.339025728 +1300
@@ -3,5 +3,5 @@
 2
 3
 4
-5
+4
 6
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << R"(version-1.2.3
1
2
3
4
5
6
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "y\n");

    EXPECT_EQ(term.output(), "This file doesn't appear to be the version-1.2.4 version -- patch anyway? [n] patching file a\n");
    EXPECT_EQ(term.return_code(), 0);
}

PATCH_TEST(prerequisite_is_not_met_abort)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(Prereq: version-1.2.4
--- a	2023-01-18 18:46:47.745632233 +1300
+++ b	2023-01-18 18:46:53.339025728 +1300
@@ -3,5 +3,5 @@
 2
 3
 4
-5
+4
 6
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << R"(version-1.2.3
1
2
3
4
5
6
)";
        file.close();
    }

    PtySpawn term(patch_path, { patch_path, "-i", "diff.patch", nullptr }, "\n");

    EXPECT_EQ(term.output(), "This file doesn't appear to be the version-1.2.4 version -- patch anyway? [n] " + std::string(patch_path) + ": **** aborted\n");
    EXPECT_EQ(term.return_code(), 2);
}

PATCH_TEST(prerequisite_is_not_met_force)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(Prereq: version-1.2.4
--- a	2023-01-18 18:46:47.745632233 +1300
+++ b	2023-01-18 18:46:53.339025728 +1300
@@ -3,5 +3,5 @@
 2
 3
 4
-5
+4
 6
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << R"(version-1.2.3
1
2
3
4
5
6
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--force", nullptr });
    EXPECT_EQ(process.stdout_data(), "Warning: this file doesn't appear to be the version-1.2.4 version -- patching anyway.\npatching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

PATCH_TEST(prerequisite_is_not_met_batch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(Prereq: version-1.2.4
--- a	2023-01-18 18:46:47.745632233 +1300
+++ b	2023-01-18 18:46:53.339025728 +1300
@@ -3,5 +3,5 @@
 2
 3
 4
-5
+4
 6
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << R"(version-1.2.3
1
2
3
4
5
6
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--batch", nullptr });
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** This file doesn't appear to be the version-1.2.4 version -- aborting.\n");
    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.return_code(), 2);
}
