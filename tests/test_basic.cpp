// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
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
