// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/pty_spawn.h>
#include <patch/test.h>

PATCH_TEST(pty_reversed_patch)
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
