// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>

static void test_quoting(const char* patch_path, const std::string& actual_filename, const std::string& displayed_filename, const std::string& output)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ )" << displayed_filename
             << R"(	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
    }

    EXPECT_FALSE(Patch::filesystem::exists(actual_filename));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), output);
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ(actual_filename, R"(int main()
{
}
)");
}

static void test_quoting(const char* patch_path, const std::string& filename, const std::string& output)
{
    test_quoting(patch_path, filename, filename, output);
}

PATCH_TEST(quoting_c_style_basic)
{
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "add", "patching file \"add\"\n");
}

PATCH_TEST(quoting_c_style_space)
{
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "add with space", "patching file \"add with space\"\n");
}

PATCH_TEST(quoting_c_style_single_quote)
{
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "with'quote", "patching file \"with'quote\"\n");
}

PATCH_TEST(quoting_c_style_double_quote)
{
#ifndef _WIN32
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "with\"quote", "patching file \"with\\\"quote\"\n");
#endif
}

PATCH_TEST(quoting_c_style_backslash)
{
#ifndef _WIN32
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "with\\slash", "patching file \"with\\\\slash\"\n");
#endif
}

PATCH_TEST(quoting_c_style_newline)
{
#ifndef _WIN32
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "with\nnewline", R"("with\nnewline")", "patching file \"with\\nnewline\"\n");
#endif
}

PATCH_TEST(quoting_c_style_tab)
{
#ifndef _WIN32
    Patch::set_env("QUOTING_STYLE", "c");
    test_quoting(patch_path, "with\ttab", R"("with\ttab")", "patching file \"with\\ttab\"\n");
#endif
}

PATCH_TEST(quoting_shell_always_style_basic)
{
    Patch::set_env("QUOTING_STYLE", "shell-always");
    test_quoting(patch_path, "add", "patching file 'add'\n");
}

PATCH_TEST(quoting_shell_always_style_single_quote_only)
{
    Patch::set_env("QUOTING_STYLE", "shell-always");
    test_quoting(patch_path, "with'quote", "patching file \"with'quote\"\n");
}

PATCH_TEST(quoting_shell_always_style_double_quote_only)
{
#ifndef _WIN32
    Patch::set_env("QUOTING_STYLE", "shell-always");
    test_quoting(patch_path, "with\"quote", "patching file 'with\"quote'\n");
#endif
}

PATCH_TEST(quoting_shell_style_double_quote_only)
{
#ifndef _WIN32
    Patch::set_env("QUOTING_STYLE", "shell");
    test_quoting(patch_path, "with\"quote", "patching file 'with\"quote'\n");
#endif
}

PATCH_TEST(quoting_shell_style_single_quote_only)
{
    Patch::set_env("QUOTING_STYLE", "shell");
    test_quoting(patch_path, "with'quote", "patching file \"with'quote\"\n");
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
