// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>

static void test_quoting(const char* patch_path, const std::string& filename, const std::string& output)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ )" << filename
             << R"(	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
    }

    EXPECT_FALSE(Patch::filesystem::exists(filename));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), output);
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ(filename, R"(int main()
{
}
)");
}

PATCH_TEST(quoting_c_style)
{
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "add", "patching file \"add\"\n");
}

PATCH_TEST(quoting_shell_always_style)
{
    Patch::set_env("QUOTING_STYLE", "shell-always");
    test_quoting(patch_path, "add", "patching file 'add'\n");
}

PATCH_TEST(quoting_shell_no_quote)
{
    Patch::set_env("QUOTING_STYLE", "shell");
    test_quoting(patch_path, "add", "patching file add\n");
}

PATCH_TEST(quoting_shell_with_quote)
{
    test_quoting(patch_path, "add!", "patching file 'add!'\n");
}

PATCH_TEST(quoting_literal_style)
{
    Patch::set_env("QUOTING_STYLE", "literal");
    test_quoting(patch_path, "add", "patching file add\n");
}
