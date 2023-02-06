// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>

PATCH_TEST(normal_patch_corrupted_add_line)
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

PATCH_TEST(normal_patch_corrupted_remove_line)
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

PATCH_TEST(normal_patch_with_tab)
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

PATCH_TEST(normal_patch_corrupted_no_space_or_tab)
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

PATCH_TEST(normal_patch_corrupted_missing_lines)
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

PATCH_TEST(normal_patch_add_file)
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

PATCH_TEST(normal_patch_remove_file)
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
