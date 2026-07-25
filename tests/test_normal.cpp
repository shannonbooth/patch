// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>

static void expect_invalid_normal_range(const char* patch_path, const std::string& range)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << range << '\n';
    }

    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "unchanged\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** Unable to parse normal range command: " + range + "\n");
    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("to_patch", "unchanged\n");
}

COMPAT_TEST(normal_patch_multiline_change_range)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(5,7c5,7
< 5
< 6
< 7
---
> five
> six
> seven
)";
    }

    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "1\n2\n3\n4\n5\n6\n7\n8\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", "1\n2\n3\n4\nfive\nsix\nseven\n8\n");
}

COMPAT_TEST(normal_patch_multiline_append_range)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(4a5,7
> five
> six
> seven
)";
    }

    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "1\n2\n3\n4\n8\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", "1\n2\n3\n4\nfive\nsix\nseven\n8\n");
}

COMPAT_TEST(normal_patch_multiline_delete_range)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(5,7d4
< 5
< 6
< 7
)";
    }

    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "1\n2\n3\n4\n5\n6\n7\n8\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", "1\n2\n3\n4\n8\n");
}

// GNU patch seems to reject these invalid ranges before printing the target file name, so its stdout differs from ours.
COMPAT_TEST(COMPAT_XFAIL_normal_patch_reversed_ranges_fail)
{
    expect_invalid_normal_range(patch_path, "7,5c5,7");
    expect_invalid_normal_range(patch_path, "5,7c7,5");
    expect_invalid_normal_range(patch_path, "4a7,5");
    expect_invalid_normal_range(patch_path, "7,5d4");
}

COMPAT_TEST(COMPAT_XFAIL_normal_patch_overflowing_ranges_fail)
{
    expect_invalid_normal_range(patch_path, "0,9223372036854775807c1");
    expect_invalid_normal_range(patch_path, "1c0,9223372036854775807");
    expect_invalid_normal_range(patch_path, "1a0,9223372036854775807");
    expect_invalid_normal_range(patch_path, "0,9223372036854775807d0");
    expect_invalid_normal_range(patch_path, "9223372036854775808c1");
}

COMPAT_TEST(normal_patch_corrupted_add_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(3c3
< 3
---
x 4
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** '>' followed by space or tab expected at line 4 of patch\n");
    EXPECT_EQ(process.return_code(), 2);
}

COMPAT_TEST(normal_patch_corrupted_remove_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(2,3c2,3
< 2
d 3
---
> a
> b
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** '<' followed by space or tab expected at line 3 of patch\n");
    EXPECT_EQ(process.return_code(), 2);
}

COMPAT_TEST(normal_patch_with_tab)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(2c2
< 2
---
>	c
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(normal_patch_corrupted_no_space_or_tab)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(2c2
< 2
---
>	c
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(normal_patch_corrupted_missing_lines)
{

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(2c2
< 2
---
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** unexpected end of file in patch at line 3\n");
    EXPECT_EQ(process.return_code(), 2);
}

COMPAT_TEST(normal_patch_add_file)
{

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(0a1
> 1
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "a", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n");
}

COMPAT_TEST(normal_patch_remove_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(1,3d0
< 1
< 2
< 3
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-n", "to_patch", "-E", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FALSE(Patch::filesystem::exists("to_patch"));
}
