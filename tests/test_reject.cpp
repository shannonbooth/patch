// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/process.h>
#include <patch/test.h>

static void test_reject_unified_remove_line_no_offset(const char* patch_path, const std::vector<const char*>& extra_args = {})
{
    const std::string patch = R"(--- 1	2022-04-24 12:58:33.100280281 +1200
+++ 2	2022-04-24 12:58:31.560276720 +1200
@@ -2,7 +2,6 @@
 2
 3
 4
-5
 6
 7
 8
)";

    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << patch;
    }

    {
        Patch::File file("1", std::ios_base::out);
        file << R"(1
2
3
5
5
5
7
8
9
10
)";
    }

    std::vector<const char*> args { patch_path, "-i", "diff.patch", "--force" };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.emplace_back(nullptr);

    Process process(patch_path, args);
    EXPECT_EQ(process.stdout_data(), R"(patching file 1
Hunk #1 FAILED at 2.
1 out of 1 hunk FAILED -- saving rejects to file 1.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);

    EXPECT_FILE_EQ("1.rej", patch);
}

PATCH_TEST(reject_unified_remove_line_no_offset_no_args)
{
    test_reject_unified_remove_line_no_offset(patch_path);
}

PATCH_TEST(reject_unified_remove_line_no_offset_flag_reject_unified)
{
    test_reject_unified_remove_line_no_offset(patch_path, { "--reject-format=unified" });
}

PATCH_TEST(reject_context_remove_line)
{
    const std::string patch = R"(*** a.cpp	2022-04-25 10:47:18.388073392 +1200
--- b.cpp	2022-04-25 10:04:06.012170915 +1200
***************
*** 1,4 ****
  int main()
  {
- 	return 0;
  }
--- 1,3 ----
)";

    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << patch;
    }

    {
        Patch::File file("a.cpp", std::ios_base::out);
        file << R"(int main()
{
/	return 0;
}
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--force", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file a.cpp
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file a.cpp.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);

    EXPECT_FILE_EQ("a.cpp.rej", patch);
}

PATCH_TEST(reject_offset_in_previous_hunk_applies_to_reject)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- main.cpp	2022-06-07 20:08:07.722685716 +1200
+++ second_main.cpp	2022-06-07 20:08:22.863130397 +1200
@@ -1,5 +1,6 @@
 int main()
 {
+	return 0;
 }
 //
 int another()
@@ -9,5 +10,5 @@
 //
 int one_more()
 {
-	return 0;
+	return 1;
 }
)";
    }

    {
        Patch::File file("main.cpp", std::ios_base::out);
        file << R"(// newly added line
// ... and another
int main()
{
}
//
int another()
{
	return 1 - 2;
}
// some
int one_more() // comments to
{ // cause
	return 0;
} // reject
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--force", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file main.cpp
Hunk #1 succeeded at 3 (offset 2 lines).
Hunk #2 FAILED at 11.
1 out of 2 hunks FAILED -- saving rejects to file main.cpp.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);

    EXPECT_FILE_EQ("main.cpp.rej", R"(--- main.cpp	2022-06-07 20:08:07.722685716 +1200
+++ second_main.cpp	2022-06-07 20:08:22.863130397 +1200
@@ -10,5 +11,5 @@
 //
 int one_more()
 {
-	return 0;
+	return 1;
 }
)");
}
