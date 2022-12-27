// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/test.h>
#include <patch/utils.h>

PATCH_TEST(version_message)
{
    Process process(patch_path, { patch_path, "--version", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patch 0.0.1
Copyright (C) 2022 Shannon Booth
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

PATCH_TEST(help_message)
{
    Process process(patch_path, { patch_path, "--help", nullptr });

    EXPECT_TRUE(Patch::starts_with(process.stdout_data(), R"(patch - (C) 2022 Shannon Booth

patch reads a patch file containing a difference (diff) and applies it to files.
)"));
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

PATCH_TEST(ed_patches_not_supported)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "3d\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ed", nullptr });

    EXPECT_TRUE(Patch::starts_with(process.stdout_data(), ""));
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** ed format patches are not supported by this version of patch\n");
    EXPECT_EQ(process.return_code(), 2);
}
