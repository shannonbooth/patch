// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>

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

PATCH_TEST(basic_verbose_context_patch_with_trailing_garbage)
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

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--verbose", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(Hmm...  Looks like a new-style context diff to me...
The text leading up to this was:
--------------------------
|
|*** a	2022-06-19 20:26:52.280538615 +1200
|--- b	2022-06-19 20:26:59.968648316 +1200
--------------------------
patching file a
Using Plan A...
Hunk #1 succeeded at 1.
Hmm...  Ignoring the trailing garbage.
done
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", R"(int main()
{
    return 0;
}
)");
}

PATCH_TEST(context_patch_with_corrupted_operation_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(*** a	2023-01-06 19:30:41.906713589 +1300
--- b	2023-01-06 19:30:45.686710928 +1300
***************
*** 1,3 ****
x  1
  2
! 3
--- 1,3 ----
  1
  2
! 4
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << "1\n2\n3\n4\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    // FIXME: Should we output 'patching file a' to stdout for this case?
    // EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** malformed patch at line 5: x  1\n\n");
    EXPECT_EQ(process.return_code(), 2);
}

PATCH_TEST(context_patch_missing_from_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(*** a	2023-01-08 21:10:53.195723396 +1300
--- b	2023-01-08 21:10:13.699815158 +1300
***************
*** 1,3 ****
! 2
! 3
--- 1,3 ----
  1
! b
! c
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    // FIXME: Should we output 'patching file a' to stdout for this case?
    /* EXPECT_EQ(process.stdout_data(), ""); */
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** Premature '---' at line 7; check line numbers at line 4\n");
    EXPECT_EQ(process.return_code(), 2);
}

PATCH_TEST(context_patch_missing_to_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(*** a	2023-01-08 21:10:53.195723396 +1300
--- b	2023-01-08 21:10:13.699815158 +1300
***************
*** 1,3 ****
  1
! 2
! 3
--- 1,3 ----
  1
! b
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    // FIXME: Should we output 'patching file a' to stdout for this case?
    /* EXPECT_EQ(process.stdout_data(), ""); */
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** context mangled in hunk at line 4\n");
    EXPECT_EQ(process.return_code(), 2);
}
