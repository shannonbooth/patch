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
