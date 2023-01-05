// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/process.h>
#include <patch/test.h>

PATCH_TEST(out_of_memory)
{
    Process process(patch_path, { patch_path, nullptr });
    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** out of memory\n");
    EXPECT_EQ(process.return_code(), 2);
}
