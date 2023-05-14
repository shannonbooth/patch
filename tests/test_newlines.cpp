// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/test.h>
#include <sstream>

PATCH_TEST(newline_strategy_is_unknown)
{
    Process process(patch_path, { patch_path, "-i", "diff.patch", "--newline-output", "unknown-option", nullptr });

    std::ostringstream ss;
    ss << patch_path << ": unrecognized newline strategy unknown-option\n"
       << patch_path << ": Try '" << patch_path << " --help' for more information.\n";

    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), ss.str());
    EXPECT_EQ(process.return_code(), 2);
}

PATCH_TEST(both_patch_and_input_as_crlf_output_keep)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);

        file << "--- to_patch	2022-06-19 16:56:12.974516527 +1200\n"
                "+++ to_patch	2022-06-19 16:56:24.666877199 +1200\n"
                "@@ -1,3 +1,4 @@\n"
                " int main()\r\n"
                " {\r\n"
                "+	return 0;\r\n"
                " }\r\n";

        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "int main()\r\n{\r\n}\r\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--newline-output", "preserve", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_BINARY_EQ("to_patch",
        "int main()\r\n"
        "{\r\n"
        "	return 0;\r\n"
        "}\r\n");
}

PATCH_TEST(mix_patch_and_input_as_crlf_with_preserve)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);

        file << "--- to_patch	2022-06-19 16:56:12.974516527 +1200\n"
                "+++ to_patch	2022-06-19 16:56:24.666877199 +1200\n"
                "@@ -1,3 +1,4 @@\n"
                " int main()\r\n"
                " {\r\n"
                "+	return 0;\r\n"
                " }\r\n";

        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "int main()\n{\n}\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--newline-output", "preserve", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patching file to_patch
Hunk #1 succeeded at 1 with fuzz 2.
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_BINARY_EQ("to_patch",
        "int main()\n"
        "{\n"
        "	return 0;\r\n"
        "}\n");
}

PATCH_TEST(mix_patch_and_input_as_crlf_with_preserve_ignore_whitespace)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);

        file << "--- to_patch	2022-06-19 16:56:12.974516527 +1200\n"
                "+++ to_patch	2022-06-19 16:56:24.666877199 +1200\n"
                "@@ -1,3 +1,4 @@\n"
                " int main()\r\n"
                " {\r\n"
                "+	return 0;\r\n"
                " }\r\n";

        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "int main()\n{\n}\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--ignore-whitespace", "--newline-output", "preserve", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_BINARY_EQ("to_patch",
        "int main()\n"
        "{\n"
        "	return 0;\r\n"
        "}\n");
}

PATCH_TEST(mix_patch_and_input_as_crlf_with_crlf)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);

        file << "--- to_patch	2022-06-19 16:56:12.974516527 +1200\n"
                "+++ to_patch	2022-06-19 16:56:24.666877199 +1200\n"
                "@@ -1,3 +1,4 @@\n"
                " int main()\r\n"
                " {\r\n"
                "+	return 0;\r\n"
                " }\r\n";

        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "int main()\n{\r\n}\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-l", "-i", "diff.patch", "--newline-output", "crlf", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_BINARY_EQ("to_patch",
        "int main()\r\n"
        "{\r\n"
        "	return 0;\r\n"
        "}\r\n");
}

PATCH_TEST(mix_patch_and_input_as_crlf_with_native)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);

        file << "--- to_patch	2022-06-19 16:56:12.974516527 +1200\n"
                "+++ to_patch	2022-06-19 16:56:24.666877199 +1200\n"
                "@@ -1,3 +1,4 @@\n"
                " int main()\r\n"
                " {\r\n"
                "+	return 0;\r\n"
                " }\r\n";

        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "int main()\n{\r\n}\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-l", "-i", "diff.patch", "--newline-output", "native", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch",
        "int main()\n"
        "{\n"
        "	return 0;\n"
        "}\n");
}

PATCH_TEST(mix_patch_and_input_as_crlf_with_lf)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);

        file << "--- to_patch	2022-06-19 16:56:12.974516527 +1200\n"
                "+++ to_patch	2022-06-19 16:56:24.666877199 +1200\n"
                "@@ -1,3 +1,4 @@\n"
                " int main()\r\n"
                " {\r\n"
                "+	return 0;\r\n"
                " }\r\n";

        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "int main()\n{\r\n}\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "--ignore-whitespace", "-i", "diff.patch", "--newline-output", "lf", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch",
        "int main()\n"
        "{\n"
        "	return 0;\n"
        "}\n");
}
