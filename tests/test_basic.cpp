// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/options.h>
#include <patch/patch.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>
#include <system_error>

COMPAT_TEST(basic_unified_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_FILE_EQ("to_patch", R"(int main()
{
	return 0;
}
)");

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(basic_verbose_unified_patch_with_trailing_garbage)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
Hmm...  Looks like a unified diff to me...
The text leading up to this was:
--------------------------
|
|--- to_patch	2022-06-19 16:56:12.974516527 +1200
|+++ to_patch	2022-06-19 16:56:24.666877199 +1200
--------------------------
patching file to_patch
Using Plan A...
Hunk #1 succeeded at 1.
Hmm...  Ignoring the trailing garbage.
done
)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--verbose", nullptr });

    EXPECT_FILE_EQ("to_patch", R"(int main()
{
	return 0;
}
)");

    EXPECT_EQ(process.stdout_data(), R"(Hmm...  Looks like a unified diff to me...
The text leading up to this was:
--------------------------
|
|--- to_patch	2022-06-19 16:56:12.974516527 +1200
|+++ to_patch	2022-06-19 16:56:24.666877199 +1200
--------------------------
patching file to_patch
Using Plan A...
Hunk #1 succeeded at 1.
Hmm...  Ignoring the trailing garbage.
done
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(normal_patch_trailing_newlines)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(2d1
< 2

)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "a", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n3\n");
}

COMPAT_TEST(less_basic_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- x   2022-10-10 20:45:19.577907405 +1300
+++ x   2022-10-10 13:03:25.792099411 +1300
@@ -1,9 +1,8 @@
 1
 2
-3
+3a
 4
 5
 6
-7
 8
 9
)";
        file.close();
    }

    {
        Patch::File file("x", std::ios_base::out);
        file << "1\n2\n3\n4\n5\n6\n7\n8\n9\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file x\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x", "1\n2\n3a\n4\n5\n6\n8\n9\n");
}

COMPAT_TEST(unknown_command_line)
{
    std::string expected_stderr = patch_path;
    expected_stderr += ": unrecognized option '--garbage'\n";
    expected_stderr += patch_path;
    expected_stderr += ": Try '";
    expected_stderr += patch_path;
    expected_stderr += " --help' for more information.\n";

    Process process(patch_path, { patch_path, "--garbage", nullptr });
    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), expected_stderr);
    EXPECT_EQ(process.return_code(), 2);
}

static void read_patch_from_stdin(const char* patch_path, const std::vector<const char*>& extra_args = {})
{
    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n4\n\n";
        file.close();
    }

    const std::string patch = R"(--- a
+++ b
@@ -1,5 +1,4 @@
 1
 2
-3
-4
 
+4
)";

    std::vector<const char*> args = { patch_path };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.push_back(nullptr);

    Process process(patch_path, args, patch);

    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(read_patch_from_stdin)
{
    read_patch_from_stdin(patch_path);
}

COMPAT_TEST(read_patch_from_stdin_dash_arg)
{
    read_patch_from_stdin(patch_path, { "-i", "-" });
}

COMPAT_TEST(dash_filename)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/- b/-
new file mode 100644
index 0000000..7a1c613
--- /dev/null
+++ b/-
@@ -0,0 +1 @@
+some file

)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file -\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("-", "some file\n");
}

COMPAT_TEST(basic_patch_with_dryrun)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

)";
        file.close();
    }

    std::string to_patch = R"(int main()
{
}
)";
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", nullptr });

    EXPECT_EQ(process.stdout_data(), "checking file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", to_patch);
}

COMPAT_TEST(basic_patch_with_dry_run_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

)";
        file.close();
    }

    std::string to_patch = R"(int main()
{
}
)";
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", "-o", "-", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(int main()
{
	return 0;
}
)");
    EXPECT_EQ(process.stderr_data(), "checking file - (read from to_patch)\n");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", to_patch);
}

COMPAT_TEST(failed_patch_to_stdout)
{
    const std::string patch = R"(--- 1	2022-06-26 12:22:22.161398905 +1200
+++ 2	2022-06-26 12:22:44.105278030 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
)";
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << patch;
        file.close();
    }

    std::string to_patch = "a\nb\nc\n";
    {
        Patch::File file("1", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-o", "-", "--force", nullptr });

    EXPECT_EQ(process.stdout_data(), to_patch);
    EXPECT_EQ(process.stderr_data(), R"(patching file - (read from 1)
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file -.rej
)");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("1", to_patch);
    EXPECT_FILE_EQ("-.rej", patch);
}

COMPAT_TEST(failed_patch_dry_run)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- 1	2022-06-26 12:22:22.161398905 +1200
+++ 2	2022-06-26 12:22:44.105278030 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
)";
        file.close();
    }

    std::string to_patch = "a\nb\nc\n";
    {
        Patch::File file("1", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", "--force", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(checking file 1
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("1", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("1.rej"));
    EXPECT_FALSE(Patch::filesystem::exists("1.orig"));
}

COMPAT_TEST(basic_patch_dry_run_remove_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(
--- a
+++ /dev/null
@@ -1 +0,0 @@
-1
)";
    }

    std::string to_patch = "1\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--dry-run", nullptr });

    EXPECT_EQ(process.stdout_data(), "checking file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n");
}

COMPAT_TEST(set_patch_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-06-19 16:56:12.974516527 +1200
+++ b	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
)";
        file.close();
    }

    {
        Patch::File file("c", std::ios_base::out);
        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "c", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file c\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("c", R"(int main()
{
	return 0;
}
)");
}

COMPAT_TEST(add_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ add	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("add"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file add\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("add", R"(int main()
{
}
)");
}

COMPAT_TEST(add_file_without_dev_null_header)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- add
+++ add
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
    }

    EXPECT_FALSE(Patch::filesystem::exists("add"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file add\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("add", R"(int main()
{
}
)");
}

static void file_not_found_does_not_prompt(const char* patch_path, const char* option)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- a	2023-01-03 11:23:35.634966282 +1300
+++ b	2023-01-03 11:23:41.598960625 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", option, nullptr });

    EXPECT_EQ(process.stdout_data(), R"(can't find file to patch at input line 3
Perhaps you should have used the -p or --strip option?
The text leading up to this was:
--------------------------
|--- a	2023-01-03 11:23:35.634966282 +1300
|+++ b	2023-01-03 11:23:41.598960625 +1300
--------------------------
No file to patch.  Skipping patch.
1 out of 1 hunk ignored
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
}

COMPAT_TEST(file_not_found_force_does_not_prompt)
{
    file_not_found_does_not_prompt(patch_path, "--force");
}

COMPAT_TEST(file_not_found_batch_does_not_prompt)
{
    file_not_found_does_not_prompt(patch_path, "--batch");
}

COMPAT_TEST(add_file_using_basename)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ a/b/c/d/e	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("e"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file e\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("e", R"(int main()
{
}
)");
}

COMPAT_TEST(add_file_missing_folders)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ a/b/c/d/e	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("a"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-p0", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a/b/c/d/e\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a/b/c/d/e", R"(int main()
{
}
)");
}

COMPAT_TEST(basic_output_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/x.cpp	2022-06-10 19:28:11.018017172 +1200
+++ b/y.cpp	2022-06-10 19:28:21.841903003 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 1;
 }
)";
        file.close();
    }

    {
        Patch::File file("x.cpp", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-o", "-", nullptr });

    EXPECT_EQ(process.stderr_data(), "patching file - (read from x.cpp)\n");
    EXPECT_EQ(process.stdout_data(), R"(int main()
{
	return 1;
}
)");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(basic_unicode_patch)
{
    {
        Patch::File file("ハローワールド.patch", std::ios_base::out);

        file << R"(
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }

)";
        file.close();
    }

    {
        Patch::File file("to_patch", std::ios_base::out);

        file << R"(int main()
{
}
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "ハローワールド.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", R"(int main()
{
	return 0;
}
)");
}

COMPAT_TEST(write_empty_output_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-07-02 15:23:07.929349813 +1200
+++ /dev/null	2022-06-30 20:33:13.470250591 +1200
@@ -1 +0,0 @@
-1
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << "1\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-o", "-", nullptr });

    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), "patching file - (read from a)\n");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n");
}

COMPAT_TEST(remove_file_in_folders)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/b/c/d/e	2022-07-03 14:32:47.081054897 +1200
+++ /dev/null	2022-06-30 20:33:13.470250591 +1200
@@ -1 +0,0 @@
-1

)";
        file.close();
    }

    const std::string file_path = "a/b/c/d/e";
    Patch::ensure_parent_directories(file_path);

    {
        Patch::File file(file_path, std::ios_base::out);

        file << "1\n";
        file.close();
    }

    // Create a second file which should not be removed
    const std::string second_file_contents = "some stuff!\n";
    {
        Patch::File file("a/1", std::ios_base::out);

        file << second_file_contents;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-p0", "--", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a/b/c/d/e\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::exists("a/b/c/d/e"));
    EXPECT_FALSE(Patch::filesystem::exists("a/b/c/d"));
    EXPECT_FALSE(Patch::filesystem::exists("a/b/c"));
    EXPECT_FALSE(Patch::filesystem::exists("a/b"));
    EXPECT_TRUE(Patch::filesystem::exists("a"));
    EXPECT_FILE_EQ("a/1", second_file_contents);
}

static void remove_file(const char* patch_path, bool expect_removed, const std::vector<const char*>& extra_args)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- to_remove	2022-06-06 14:40:51.547454892 +1200
+++ /dev/null	2022-06-01 16:08:26.275921370 +1200
@@ -1,3 +0,0 @@
-int main()
-{
-}
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
}
)";

    {
        Patch::File file("to_remove", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    std::vector<const char*> args { patch_path, "-i", "diff.patch" };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.emplace_back(nullptr);

    Process process(patch_path, args);

    EXPECT_EQ(process.stdout_data(), "patching file to_remove\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    if (expect_removed)
        EXPECT_FALSE(Patch::filesystem::exists("to_remove"));
    else
        EXPECT_FILE_EQ("to_remove", "");
}

COMPAT_TEST(remove_file_successfully_no_args)
{
    remove_file(patch_path, true, {});
}

COMPAT_TEST(no_remove_file_posix)
{
    remove_file(patch_path, false, { "--posix" });
}

COMPAT_TEST(no_remove_file_posix_as_env)
{
    Patch::set_env("POSIXLY_CORRECT", "1");
    remove_file(patch_path, false, {});
}

COMPAT_TEST(remove_file_successfully_posix_and_remove_flag)
{
    remove_file(patch_path, true, { "--posix", "--remove-empty-files" });
}

COMPAT_TEST(deleting_to_explicit_output_writes_an_empty_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- source.txt
+++ /dev/null
@@ -1 +0,0 @@
-content
)";
    }

    {
        Patch::File file("source.txt", std::ios_base::out);
        file << "content\n";
    }

    Patch::filesystem::create_directory("outside");
    Patch::filesystem::create_directory("outside/deep");
    {
        Patch::File file("outside/deep/target.txt", std::ios_base::out);
        file << "old content\n";
    }

    const auto output = Patch::current_path() + "/outside/deep/target.txt";
    Process process(patch_path, { patch_path, "--batch", "-i", "diff.patch", "-o", output.c_str(), nullptr });

    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("outside/deep/target.txt", "");
    EXPECT_TRUE(Patch::filesystem::exists("outside/deep"));
    EXPECT_TRUE(Patch::filesystem::exists("outside"));
    EXPECT_FILE_EQ("source.txt", "content\n");
}

COMPAT_TEST(unified_patch_that_empties_file_keeps_empty_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- old.txt	2026-04-05 13:10:37
+++ new.txt	2026-04-05 13:10:37
@@ -1,2 +0,0 @@
-
-n[@CuBYRF+w.Ul.jw]M)e=XYaAc
)";
    }

    {
        Patch::File file("target.txt", std::ios_base::out);
        file << "\n"
                "n[@CuBYRF+w.Ul.jw]M)e=XYaAc\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "target.txt", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file target.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_TRUE(Patch::filesystem::exists("target.txt"));
    EXPECT_FILE_EQ("target.txt", "");
}

COMPAT_TEST(git_patch_modifying_the_same_file_twice)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/f.txt b/f.txt
index 1111111..2222222 100644
--- a/f.txt
+++ b/f.txt
@@ -1 +1 @@
-one
+two
diff --git a/f.txt b/f.txt
index 2222222..3333333 100644
--- a/f.txt
+++ b/f.txt
@@ -1 +1 @@
-two
+three
)";
        file.close();
    }

    {
        Patch::File file("f.txt", std::ios_base::out);
        file << "one\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p1", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file f.txt\npatching file f.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("f.txt", "three\n");
}

COMPAT_TEST(git_patch_creating_then_modifying_a_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/c.txt b/c.txt
new file mode 100644
index 0000000..1111111
--- /dev/null
+++ b/c.txt
@@ -0,0 +1 @@
+first
diff --git a/c.txt b/c.txt
index 1111111..2222222 100644
--- a/c.txt
+++ b/c.txt
@@ -1 +1 @@
-first
+second
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p1", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file c.txt\npatching file c.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("c.txt", "second\n");
}

// Two renames which swap a pair of files are both written against the original
// content, so neither may be installed until the whole input has been read.
COMPAT_TEST(git_patch_renames_swapping_two_files)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/a.txt b/b.txt
similarity index 100%
rename from a.txt
rename to b.txt
diff --git a/b.txt b/a.txt
similarity index 100%
rename from b.txt
rename to a.txt
)";
        file.close();
    }

    {
        Patch::File file("a.txt", std::ios_base::out);
        file << "content A\n";
        file.close();
    }

    {
        Patch::File file("b.txt", std::ios_base::out);
        file << "content B\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p1", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a.txt", "content B\n");
    EXPECT_FILE_EQ("b.txt", "content A\n");
}

COMPAT_TEST(git_patch_renaming_into_a_directory_which_does_not_exist)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/old.txt b/newdir/new.txt
rename from old.txt
rename to newdir/new.txt
index 1111111..2222222 100644
--- a/old.txt
+++ b/newdir/new.txt
@@ -1 +1 @@
-content
+edited
)";
        file.close();
    }

    {
        Patch::File file("old.txt", std::ios_base::out);
        file << "content\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p1", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file newdir/new.txt (renamed from old.txt)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("newdir/new.txt", "edited\n");
    EXPECT_FALSE(Patch::filesystem::exists("old.txt"));
}

COMPAT_TEST(git_patch_copying_into_a_directory_which_does_not_exist)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/x b/newdir/y
similarity index 100%
copy from x
copy to newdir/y
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
    return 0;
}
)";
    {
        Patch::File file("x", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-p1", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file newdir/y (copied from x)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x", to_patch);
    EXPECT_FILE_EQ("newdir/y", to_patch);
}

COMPAT_TEST(git_patch_remove_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
diff --git a/a b/a
deleted file mode 100644
index 01e79c3..0000000
--- a/a
+++ /dev/null
@@ -1,3 +0,0 @@
-1
-2
-3
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    EXPECT_TRUE(Patch::filesystem::exists("a"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::exists("a"));
}

COMPAT_TEST(remove_file_that_has_trailing_garbage)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- remove	2022-06-06 14:40:51.547454892 +1200
+++ /dev/null	2022-06-01 16:08:26.275921370 +1200
@@ -1,3 +0,0 @@
-int main()
-{
-}

)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
}
// some trailing garbage
)";
    {
        Patch::File file("remove", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patching file remove
Not deleting file remove as content differs from patch
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("remove", "// some trailing garbage\n");
}

COMPAT_TEST(remove_git_submodule)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/.gitmodules b/.gitmodules
index 066b99a..e69de29 100644
--- a/.gitmodules
+++ b/.gitmodules
@@ -1,3 +0,0 @@
-[submodule "libarchive"]
-	path = libarchive
-	url = https://github.com/libarchive/libarchive.git
diff --git a/libarchive b/libarchive
deleted file mode 160000
index a45905b..0000000
--- a/libarchive
+++ /dev/null
@@ -1 +0,0 @@
-Subproject commit a45905b0166713760a2fb4f2e908d7ce47488371
)";
        file.close();
    }

    {
        Patch::File file(".gitmodules", std::ios_base::out);

        file << R"([submodule "libarchive"]
	path = libarchive
	url = https://github.com/libarchive/libarchive.git
)";
        file.close();
    }

    EXPECT_TRUE(Patch::filesystem::create_directory("libarchive"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patching file .gitmodules
File libarchive is not a regular file -- refusing to patch
1 out of 1 hunk ignored -- saving rejects to file libarchive.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
}

COMPAT_TEST(git_binary_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
From f933cb15f717a43ef1961d797874ca4a5650ff08 Mon Sep 17 00:00:00 2001
From: Shannon Booth <shannon.ml.booth@gmail.com>
Date: Mon, 18 Jul 2022 10:16:19 +1200
Subject: [PATCH] add utf16

---
 a.txt | Bin 0 -> 14 bytes
 1 file changed, 0 insertions(+), 0 deletions(-)
 create mode 100644 a.txt

diff --git a/a.txt b/a.txt
new file mode 100644
index 0000000000000000000000000000000000000000..c193b2437ca5bca3eaee833d9cc40b04875da742
GIT binary patch
literal 14
ScmezWFOh+ZAqj|+ffxWJ!UIA8

literal 0
HcmV?d00001

--
2.25.1
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "File a.txt: git binary diffs are not supported.\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
}

COMPAT_TEST(basic_unicode_patch_filepaths)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- "\327\251\327\234\327\225\327\235 \327\242\327\225\327\234\327\235!"	2022-09-03 14:51:28.429821767 +1200
+++ another	2022-09-03 14:52:15.250024346 +1200
@@ -1,3 +1,2 @@
 a
-b
 c
)";
        file.close();
    }

    {
        Patch::File file("שלום עולם!", std::ios_base::out);
        file << "a\nb\nc\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file 'שלום עולם!'\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("שלום עולם!", "a\nc\n");
}

COMPAT_TEST(override_reject_file)
{
    const std::string patch = R"(--- a	2022-07-08 10:34:03.860546761 +1200
+++ b	2022-07-08 10:34:20.096714313 +1200
@@ -1,3 +1,3 @@
-a
-b
-c
+1
+2
+3
)";

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << patch;
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--force", "-r", "override.rej", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file a
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file override.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("override.rej", patch);
}

COMPAT_TEST(reject_format_context)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/reject	2022-07-30 11:40:37.280248088 +1200
+++ b/reject	2022-07-30 11:40:49.032177150 +1200
@@ -1,3 +1,3 @@
 abc
-def
+123
 ghi
)";
        file.close();
    }

    const std::string to_patch = "apc\nzxy\nghi\n";

    {
        Patch::File file("reject", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--reject-format", "context", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file reject
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file reject.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("reject", to_patch);
    EXPECT_FILE_EQ("reject.rej", R"(*** reject	2022-07-30 11:40:37.280248088 +1200
--- reject	2022-07-30 11:40:49.032177150 +1200
***************
*** 1,3 ****
  abc
! def
  ghi
--- 1,3 ----
  abc
! 123
  ghi
)");
}

COMPAT_TEST(unicode_rename_git_no_quoting)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
commit f3f4654d6a154907dbd36c47d49c910b0c10c072
Author: Shannon Booth <shannon.ml.booth@gmail.com>
Date:   Sun Sep 4 11:03:05 2022 +1200

    Add unicode path using core.quotePath false

diff --git a/file b/지배
similarity index 66%
rename from file
rename to 지배
index de98044..0f7bc76 100644
--- a/նախքան
+++ b/지배
@@ -1,3 +1,2 @@
 Мир
-b
 c
)";
        file.close();
    }

    {
        Patch::File file("նախքան", std::ios_base::out);
        file << "Мир\nb\nc\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-idiff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file 지배 (renamed from նախքան)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("지배", "Мир\nc\n");
}

static void test_chdir(const char* patch_path, const std::string& folder)
{
    EXPECT_TRUE(Patch::filesystem::create_directory(folder));

    {
        Patch::File file(folder + "/diff.patch", std::ios_base::out);

        file << R"(
--- 1	2022-06-26 11:17:58.948060133 +1200
+++ 2	2022-06-26 11:18:03.500001858 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
)";
        file.close();
    }

    {
        Patch::File file(folder + "/1", std::ios_base::out);
        file << "1\n2\n3\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-d", folder.c_str(), nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file 1\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ(folder + "/1", "1\n3\n");
}

COMPAT_TEST(chdir_unicode)
{
    test_chdir(patch_path, "गिलास");
}

COMPAT_TEST(chdir_good_case)
{
    test_chdir(patch_path, "folder");
}

COMPAT_TEST(rename_no_change)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/orig_file b/another_new
similarity index 100%
rename from orig_file
rename to another_new
)";
        file.close();
    }

    const std::string to_patch = "a\nb\nc\nd\n";
    {
        Patch::File file("orig_file", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file another_new (renamed from orig_file)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("another_new", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("orig_file"));
}

COMPAT_TEST(reverse_rename_no_change)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/x b/y
similarity index 100%
rename from x
rename to y
)";
        file.close();
    }

    const std::string to_patch = "a\nb\nc\nd\n";
    {
        Patch::File file("y", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-R", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file x (renamed from y)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("y"));
}

COMPAT_TEST(rename_with_change)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/file b/test
similarity index 87%
rename from thing
rename to test
index 71ac1b5..fc3102f 100644
--- a/thing
+++ b/test
@@ -2,7 +2,6 @@ a
 b
 c
 d
-e
 f
 g
 h
)";
        file.close();
    }

    {
        Patch::File file("thing", std::ios_base::out);
        file << "a\nb\nc\nd\ne\nf\ng\nh\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file test (renamed from thing)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("test", "a\nb\nc\nd\nf\ng\nh\n");
    EXPECT_FALSE(Patch::filesystem::exists("thing"));
}

COMPAT_TEST(copy_no_change)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/x b/y
similarity index 100%
copy from x
copy to y
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
    return 0;
}
)";
    {
        Patch::File file("x", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file y (copied from x)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x", to_patch);
    EXPECT_FILE_EQ("y", to_patch);
}

COMPAT_TEST(copy_with_change)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(diff --git a/x b/y
similarity index 51%
copy from x
copy to y
index 905869d..2227c3a 100644
--- a/x
+++ b/y
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
+    return 1;
 }
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
    return 0;
}
)";
    {
        Patch::File file("x", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file y (copied from x)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x", to_patch);
    EXPECT_FILE_EQ("y", R"(int main()
{
    return 1;
}
)");
}

COMPAT_TEST(rename_already_exists_no_content)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
From 89629b257f091dd0ff78509ca0ad626089defaa7 Mon Sep 17 00:00:00 2001
From: Shannon Booth <shannon.ml.booth@gmail.com>
Date: Tue, 5 Jul 2022 18:53:32 +1200
Subject: [PATCH] move a to b

---
 a => b | 0
 1 file changed, 0 insertions(+), 0 deletions(-)
 rename a => b (100%)

diff --git a/a b/b
similarity index 100%
rename from a
rename to b
--
2.25.1

)";
        file.close();
    }

    const std::string to_patch = "1\n2\n3\n";
    {
        Patch::File file("b", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file b (already renamed from a)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("b", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("a"));
}

COMPAT_TEST(rename_already_exists_with_content)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
diff --git a/b b/a
similarity index 66%
rename from b
rename to a
index de98044..0f673f8 100644
--- a/b
+++ b/a
@@ -1,3 +1,3 @@
 a
-b
+2
 c
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "a\nb\nc\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a (already renamed from b)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "a\n2\nc\n");
    EXPECT_FALSE(Patch::filesystem::exists("b"));
}

COMPAT_TEST(reverse_rename_already_exists_with_content)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
diff --git a/a b/b
similarity index 66%
rename from a
rename to b
index de98044..0f673f8 100644
--- a/a
+++ b/b
@@ -1,3 +1,3 @@
 a
-b
+2
 c
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "a\n2\nc\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--reverse", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a (already renamed from b)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "a\nb\nc\n");
    EXPECT_FALSE(Patch::filesystem::exists("b"));
}

COMPAT_TEST(backup_rename_patch)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        // NOTE: this might very well be a bug. However, GNU patch
        // also has this behavior, so this test is to ensure any
        // change here is made deliberately.

        file << R"(
commit 61593eeba9cf1663927cbccec2a15a987b6f9e53
Author: Shannon Booth <shannon.ml.booth@gmail.com>
Date:   Sun Sep 4 11:27:52 2022 +1200

    rename

diff --git a/a b/b
similarity index 100%
rename from a
rename to b

)";
        file.close();
    }

    const std::string to_patch = "ab";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-b", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file b (renamed from a)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("b", to_patch);
    EXPECT_FILE_EQ("b.orig", "");
    EXPECT_FALSE(Patch::filesystem::exists("a"));
    EXPECT_FILE_EQ("a.orig", to_patch);
}

COMPAT_TEST(backup_of_removed_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- gone.txt
+++ /dev/null
@@ -1 +0,0 @@
-content to lose
)";
        file.close();
    }

    const std::string to_patch = "content to lose\n";
    {
        Patch::File file("gone.txt", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p0", "-b", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file gone.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    // The file is removed, so the backup is the only copy left of its content.
    EXPECT_FALSE(Patch::filesystem::exists("gone.txt"));
    EXPECT_FILE_EQ("gone.txt.orig", to_patch);
}

COMPAT_TEST(backup_of_removed_file_in_subdirectory)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- sub/nested.txt
+++ /dev/null
@@ -1 +0,0 @@
-content to lose
)";
        file.close();
    }

    const std::string to_patch = "content to lose\n";
    {
        Patch::filesystem::create_directory("sub");
        Patch::File file("sub/nested.txt", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p0", "-b", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file sub/nested.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::exists("sub/nested.txt"));
    EXPECT_FILE_EQ("sub/nested.txt.orig", to_patch);
}

COMPAT_TEST(no_backup_of_removed_file_without_flag)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- gone.txt
+++ /dev/null
@@ -1 +0,0 @@
-content to lose
)";
        file.close();
    }

    {
        Patch::File file("gone.txt", std::ios_base::out);
        file << "content to lose\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p0", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file gone.txt\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::exists("gone.txt"));
    EXPECT_FALSE(Patch::filesystem::exists("gone.txt.orig"));
}

COMPAT_TEST(no_backup_of_removed_file_for_dry_run)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- gone.txt
+++ /dev/null
@@ -1 +0,0 @@
-content to lose
)";
        file.close();
    }

    const std::string to_patch = "content to lose\n";
    {
        Patch::File file("gone.txt", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "--batch", "-p0", "-b", "--dry-run", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("gone.txt", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("gone.txt.orig"));
}

COMPAT_TEST(backup_on_top_of_existing_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-06-19 13:12:11.460782360 +1200
+++ b	2022-06-19 13:12:19.472676546 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)";
        file.close();
    }

    {
        Patch::File file("a.orig", std::ios_base::out);
        file << "some file content!\n";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
}
)";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-b", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("a", R"(int main()
{
    return 0;
}
)");
    EXPECT_FILE_EQ("a.orig", to_patch);
}

static void backup_if_mismatch(const char* patch_path, const std::vector<const char*>& extra_args)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2023-01-22 11:11:17.631295409 +1300
+++ b	2023-01-22 11:11:22.183419472 +1300
@@ -1,3 +1,2 @@
 1
-2
 3
)";
        file.close();
    }

    // extra newline at start of file
    const std::string to_patch = "\n1\n2\n3\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    // Instert extra commandline arguments (if any).
    std::vector<const char*> args { patch_path, "-i", "diff.patch" };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.emplace_back(nullptr);

    Process process(patch_path, args);
    EXPECT_EQ(process.stdout_data(), "patching file a\nHunk #1 succeeded at 2 (offset 1 line).\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("a", "\n1\n3\n");
    EXPECT_FILE_EQ("a.orig", to_patch);
}

COMPAT_TEST(backup_if_mismatch_done_by_default)
{
    backup_if_mismatch(patch_path, {});
}

COMPAT_TEST(backup_if_mismatch_done_with_flag)
{
    backup_if_mismatch(patch_path, { "--backup-if-mismatch" });
}

COMPAT_TEST(backup_if_mismatch_done_posix_and_done_with_flag)
{
    backup_if_mismatch(patch_path, { "--backup-if-mismatch", "--posix" });
}

COMPAT_TEST(backup_if_mismatch_done_posix_and_backup_given)
{
    backup_if_mismatch(patch_path, { "-b", "--posix" });
}

static void no_backup_if_mismatch(const char* patch_path, const std::vector<const char*>& extra_args)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2023-01-22 11:11:17.631295409 +1300
+++ b	2023-01-22 11:11:22.183419472 +1300
@@ -1,3 +1,2 @@
 1
-2
 3
)";
        file.close();
    }

    // extra newline at start of file
    const std::string to_patch = "\n1\n2\n3\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    // Instert extra commandline arguments (if any).
    std::vector<const char*> args { patch_path, "-i", "diff.patch" };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.emplace_back(nullptr);

    Process process(patch_path, args);
    EXPECT_EQ(process.stdout_data(), "patching file a\nHunk #1 succeeded at 2 (offset 1 line).\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("a", "\n1\n3\n");
    EXPECT_FALSE(Patch::filesystem::exists("a.orig"));
}

COMPAT_TEST(backup_if_mismatch_not_done_with_flag)
{
    no_backup_if_mismatch(patch_path, { "--no-backup-if-mismatch" });
}

COMPAT_TEST(COMPAT_XFAIL_backup_if_mismatch_not_done_posix)
{
    // NOTE: This appears to be a bug with GNU patch.
    //       Setting POSIXLY_CORRECT appears to work (see below), but --posix flag does not!
    no_backup_if_mismatch(patch_path, { "--posix" });
}

COMPAT_TEST(backup_if_mismatch_not_done_posix_env)
{
    Patch::set_env("POSIXLY_CORRECT", "1");
    no_backup_if_mismatch(patch_path, {});
}

COMPAT_TEST(backup_multiple_files_only_backs_up_first)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/main.cpp	2022-06-08 20:59:48.119369201 +1200
+++ b/main.cpp	2022-06-08 21:00:29.848423061 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
--- b/main.cpp	2022-06-08 21:00:29.848423061 +1200
+++ c/main.cpp	2022-06-08 21:00:59.885147433 +1200
@@ -1,3 +1,4 @@
-int main()
+int main(int argc, char** argv)
 {
+	return 5; // and comment
 }
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
    return 0;
}
)";
    {
        Patch::File file("main.cpp", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-b", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file main.cpp\npatching file main.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("main.cpp", R"(int main(int argc, char** argv)
{
	return 5; // and comment
}
)");
    EXPECT_FILE_EQ("main.cpp.orig", to_patch);
}

COMPAT_TEST(backup_when_patch_is_adding_a_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null	2022-06-08 13:09:24.217949672 +1200
+++ f	2022-06-08 20:44:55.455275623 +1200
@@ -0,0 +1 @@
+a line
)";
        file.close();
    }

    EXPECT_FALSE(Patch::filesystem::exists("f"));
    EXPECT_FALSE(Patch::filesystem::exists("f.orig"));

    Process process(patch_path, { patch_path, "-b", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file f\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("f", "a line\n");
    EXPECT_FILE_EQ("f.orig", "");
}

COMPAT_TEST(backup_creates_parent_directories)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- /dev/null
+++ nested/file
@@ -0,0 +1 @@
+a line
)";
    }

    Process process(patch_path, { patch_path, "-p0", "--backup", "--prefix", "backups/", "--suffix", ".orig", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file nested/file\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("nested/file", "a line\n");
    EXPECT_FILE_EQ("backups/nested/file.orig", "");
}

COMPAT_TEST(backup_prefix_only)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- x	2022-07-27 21:07:04.624795529 +1200
+++ y	2022-07-27 21:07:08.304813552 +1200
@@ -1 +1 @@
-abc
+xyz
)";
        file.close();
    }

    const std::string to_patch = "abc\n";
    {
        Patch::File file("x", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "--backup", "--prefix", "pre.", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file x\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("pre.x", to_patch);
    EXPECT_FILE_EQ("x", "xyz\n");
}

COMPAT_TEST(backup_suffix_only)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- x	2022-07-27 21:07:04.624795529 +1200
+++ y	2022-07-27 21:07:08.304813552 +1200
@@ -1 +1 @@
-abc
+xyz
)";
        file.close();
    }

    const std::string to_patch = "abc\n";
    {
        Patch::File file("x", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "--backup", "--suffix", ".post", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file x\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("x.post", to_patch);
    EXPECT_FILE_EQ("x", "xyz\n");
}

COMPAT_TEST(backup_prefix_and_suffix)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- x	2022-07-27 21:07:04.624795529 +1200
+++ y	2022-07-27 21:07:08.304813552 +1200
@@ -1 +1 @@
-abc
+xyz
)";
        file.close();
    }

    const std::string to_patch = "abc\n";
    {
        Patch::File file("x", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "--backup", "--prefix", "pre.", "--suffix", ".post", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file x\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("pre.x.post", to_patch);
    EXPECT_FILE_EQ("x", "xyz\n");
}

COMPAT_TEST(reverse_still_applies_to_first_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/x.cpp	2022-06-10 19:28:11.018017172 +1200
+++ b/y.cpp	2022-06-10 19:28:21.841903003 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+    return 1;
 }
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
    return 1;
}
)";
    {
        Patch::File file("x.cpp", std::ios_base::out);
        file << to_patch;
        file.close();
    }
    {
        Patch::File file("y.cpp", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    // GNU patch does not seem to also reverse the old and new file
    // names and will still apply the patch to the old file first.
    Process process(patch_path, { patch_path, "-R", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file x.cpp\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("y.cpp", to_patch);
    EXPECT_FILE_EQ("x.cpp", R"(int main()
{
}
)");
}

COMPAT_TEST(reverse_option_when_reversed)
{
    const std::string patch = R"(
)";
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- main.cpp	2022-06-11 16:34:12.304745651 +1200
+++ main2.cpp	2022-06-11 16:34:04.240768394 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-	return 0;
 }
)";
        file.close();
    }

    const std::string to_patch = R"(int main()
{
	return 0;
}
)";
    {
        Patch::File file("main.cpp", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-RN", "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(patching file main.cpp
Unreversed patch detected!  Skipping patch.
1 out of 1 hunk ignored -- saving rejects to file main.cpp.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);

    EXPECT_FILE_EQ("main.cpp.rej", R"(--- main2.cpp	2022-06-11 16:34:04.240768394 +1200
+++ main.cpp	2022-06-11 16:34:12.304745651 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
)");
    EXPECT_FILE_EQ("main.cpp", to_patch);
    EXPECT_FALSE(Patch::filesystem::exists("main.cpp.orig"));
}

COMPAT_TEST(write_output_to_some_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-08-21 12:01:05.302352795 +1200
+++ b	2022-08-21 12:01:08.874339091 +1200
@@ -1,3 +1,3 @@
 a
-b
+d
 c
)";
        file.close();
    }

    const std::string to_patch = "a\nb\nc\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-osome-file", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file some-file (read from a)\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", to_patch);
    EXPECT_FILE_EQ("some-file", "a\nd\nc\n");
}

COMPAT_TEST(error_when_invalid_patch_given)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "\n\tsome\n\tgarbage!\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** Only garbage was found in the patch input.\n");
    EXPECT_EQ(process.return_code(), 2);
}

template<typename Function>
static void expect_no_such_file_or_directory(Function function)
{
    bool caught = false;
    const auto expected = std::make_error_code(std::errc::no_such_file_or_directory);
    try {
        function();
    } catch (const std::system_error& error) {
        caught = true;
        EXPECT_EQ(error.code(), expected);
    }
    EXPECT_EQ(caught, true);
}

TEST(missing_patch_file_reports_no_such_file_or_directory)
{
    Patch::Options options;
    options.patch_file_path = "diff.patch";
    expect_no_such_file_or_directory([&] { Patch::process_patch(options); });
}

TEST(bad_patch_directory_reports_no_such_file_or_directory)
{
    expect_no_such_file_or_directory([] { Patch::chdir("bad_directory"); });
}

static void expect_fatal_error_with_prefix(const std::string& output, const std::string& prefix)
{
    EXPECT_EQ(output.substr(0, prefix.size()), prefix);
    EXPECT_NE(output.size(), prefix.size());
    EXPECT_NE(output.size(), prefix.size() + 1);
    EXPECT_EQ(output.back(), '\n');
}

COMPAT_TEST(error_when_nonexistent_patch_file_given)
{
    EXPECT_FALSE(Patch::filesystem::exists("diff.patch"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "");
    expect_fatal_error_with_prefix(process.stderr_data(),
        std::string(patch_path) + ": **** Can't open patch file diff.patch");
    EXPECT_EQ(process.return_code(), 2);
}

COMPAT_TEST(error_on_chdir_to_bad_directory)
{
    Process process(patch_path, { patch_path, "-i", "diff.patch", "-d", "bad_directory", nullptr });
    EXPECT_EQ(process.stdout_data(), "");
    expect_fatal_error_with_prefix(process.stderr_data(),
        std::string(patch_path) + ": **** Can't change to directory bad_directory");
    EXPECT_EQ(process.return_code(), 2);
}

static void test_basic_bad_mode_given(const char* patch_path, const char* new_mode_str)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << "diff --git a/file b/file\n"
             << "old mode 100644\n"
             << "new mode " << new_mode_str << '\n';
    }

    const std::string to_patch = "1\n2\n3\n";
    {
        Patch::File file("file", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    const auto old_perms = Patch::filesystem::get_permissions("file");

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-u", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file file\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("file", to_patch);

    const auto new_perms = Patch::filesystem::get_permissions("file");

    EXPECT_EQ(old_perms, new_perms);
}

COMPAT_TEST(test_basic_bad_mode_given_too_long)
{
    test_basic_bad_mode_given(patch_path, "1007555");
}

COMPAT_TEST(test_basic_bad_mode_given_bad_character_ending)
{
    test_basic_bad_mode_given(patch_path, "10075a");
}

COMPAT_TEST(test_basic_bad_mode_given_bad_character_beginning)
{
    test_basic_bad_mode_given(patch_path, ";0075a");
}

COMPAT_TEST(add_executable_bit)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
diff --git a/file b/file
old mode 100644
new mode 100755
)";
        file.close();
    }

    const std::string to_patch = "1\n2\n3\n";
    {
        Patch::File file("file", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file file\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("file", to_patch);

#ifndef _WIN32
    const auto perms = Patch::filesystem::get_permissions("file");
    EXPECT_TRUE((perms & Patch::filesystem::perms::owner_exec) != Patch::filesystem::perms::none);
    EXPECT_TRUE((perms & Patch::filesystem::perms::group_exec) != Patch::filesystem::perms::none);
    EXPECT_TRUE((perms & Patch::filesystem::perms::others_exec) != Patch::filesystem::perms::none);
#endif
}

static void test_read_only_file(const char* patch_path, const std::vector<const char*>& extra_args = {})
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/a	2022-09-18 09:59:59.586887443 +1200
+++ b/a	2022-09-18 10:00:04.410912780 +1200
@@ -1,4 +1,3 @@
 1
 2
-3
 4
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n4\n";
        file.close();
    }

    auto mode = Patch::filesystem::get_permissions("a");
    const auto ro_mask = Patch::filesystem::perms::all
        ^ (Patch::filesystem::perms::owner_write
            | Patch::filesystem::perms::group_write
            | Patch::filesystem::perms::others_write);

    // Make file read-only. Keep track of old mode.
    Patch::filesystem::permissions("a", mode & ro_mask);
    const auto old_mode = Patch::filesystem::get_permissions("a");

    std::vector<const char*> args { patch_path, "-i", "diff.patch" };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.push_back(nullptr);

    Process process(patch_path, args);
    EXPECT_EQ(process.stdout_data(), "File a is read-only; trying to patch anyway\npatching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n2\n4\n");

    const auto new_mode = Patch::filesystem::get_permissions("a");
    EXPECT_TRUE(old_mode == new_mode);
}

COMPAT_TEST(read_only_file_no_arguments)
{
    test_read_only_file(patch_path);
}

COMPAT_TEST(read_only_file_warn)
{
    test_read_only_file(patch_path, { "--read-only=warn" });
}

COMPAT_TEST(read_only_file_ignore)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a/a	2022-09-18 09:59:59.586887443 +1200
+++ b/a	2022-09-18 10:00:04.410912780 +1200
@@ -1,4 +1,3 @@
 1
 2
-3
 4
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "1\n2\n3\n4\n";
        file.close();
    }

    auto mode = Patch::filesystem::get_permissions("a");
    const auto ro_mask = Patch::filesystem::perms::all
        ^ (Patch::filesystem::perms::owner_write
            | Patch::filesystem::perms::group_write
            | Patch::filesystem::perms::others_write);

    // Make file read-only. Keep track of old mode.
    Patch::filesystem::permissions("a", mode & ro_mask);
    const auto old_mode = Patch::filesystem::get_permissions("a");

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--read-only=ignore", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "1\n2\n4\n");

    const auto new_mode = Patch::filesystem::get_permissions("a");
    EXPECT_TRUE(old_mode == new_mode);
}

COMPAT_TEST(read_only_file_fail)
{
    const std::string patch = R"(--- a	2022-09-18 09:59:59.586887443 +1200
+++ a	2022-09-18 10:00:04.410912780 +1200
@@ -1,4 +1,3 @@
 1
 2
-3
 4
)";
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << patch;
        file.close();
    }

    const std::string to_patch = "1\n2\n3\n4\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << to_patch;
        file.close();
    }

    auto mode = Patch::filesystem::get_permissions("a");
    const auto ro_mask = Patch::filesystem::perms::all
        ^ (Patch::filesystem::perms::owner_write
            | Patch::filesystem::perms::group_write
            | Patch::filesystem::perms::others_write);

    // Make file read-only. Keep track of old mode.
    Patch::filesystem::permissions("a", mode & ro_mask);
    const auto old_mode = Patch::filesystem::get_permissions("a");

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--read-only", "fail", nullptr });
    EXPECT_EQ(process.stdout_data(), R"(File a is read-only; refusing to patch
1 out of 1 hunk ignored -- saving rejects to file a.rej
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("a", to_patch);
    EXPECT_FILE_EQ("a.rej", patch);

    const auto new_mode = Patch::filesystem::get_permissions("a");
    EXPECT_TRUE(old_mode == new_mode);
}

static Patch::filesystem::perms make_file_read_only(const std::string& path)
{
    const auto write_permissions = Patch::filesystem::perms::owner_write
        | Patch::filesystem::perms::group_write
        | Patch::filesystem::perms::others_write;
    const auto permissions = Patch::filesystem::get_permissions(path) & (Patch::filesystem::perms::all ^ write_permissions);
    Patch::filesystem::permissions(path, permissions);
    return permissions;
}

COMPAT_TEST(malformed_patch_does_not_modify_read_only_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- a
+++ a
@@ -1,4 +1,3 @@
 1
 2
-3
d4
 4
)";
    }

    const std::string original = "1\n2\n3\n4\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << original;
    }
    const auto original_permissions = make_file_read_only("a");

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("a", original);
    EXPECT_EQ(Patch::filesystem::get_permissions("a"), original_permissions);
}

COMPAT_TEST(backup_failure_does_not_modify_read_only_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- a
+++ a
@@ -1 +1 @@
-old
+new
)";
    }

    const std::string original = "old\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << original;
    }
    const auto original_permissions = make_file_read_only("a");
    Patch::File::touch("not-a-directory");

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--backup", "--prefix", "not-a-directory/", nullptr });

    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("a", original);
    EXPECT_EQ(Patch::filesystem::get_permissions("a"), original_permissions);
}

COMPAT_TEST(replacement_preserves_file_mode)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- a
+++ a
@@ -1 +1 @@
-old
+new
)";
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "old\n";
    }

    const auto mode = Patch::filesystem::perms::owner_read
        | Patch::filesystem::perms::owner_write
        | Patch::filesystem::perms::group_read;
    Patch::filesystem::permissions("a", mode);
    const auto original_mode = Patch::filesystem::get_permissions("a");

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("a", "new\n");
    EXPECT_EQ(Patch::filesystem::get_permissions("a"), original_mode);
}

COMPAT_TEST(late_parse_failure_keeps_completed_files)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(diff --git a/a b/a
--- a/a
+++ b/a
@@ -1 +1 @@
-old a
+new a
diff --git a/b b/b
--- a/b
+++ b/b
@@ -1 +1 @@
-old b
dnew b
)";
    }

    {
        Patch::File file("a", std::ios_base::out);
        file << "old a\n";
    }
    {
        Patch::File file("b", std::ios_base::out);
        file << "old b\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("a", "new a\n");
    EXPECT_FILE_EQ("b", "old b\n");
}

COMPAT_TEST(git_swap_files)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
diff --git a/a b/b
rename from a
rename to b
diff --git a/b b/a
rename from b
rename to a
)";
        file.close();
    }

    const std::string a_content = "a\nb\nc\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << a_content;
        file.close();
    }

    const std::string b_content = "1\n2\n3\n";
    {
        Patch::File file("b", std::ios_base::out);
        file << b_content;
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(patching file b (renamed from a)
patching file a (renamed from b)
)");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("a", b_content);
    EXPECT_FILE_EQ("b", a_content);
}

COMPAT_TEST(unified_patch_with_corrupted_operation_line)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(--- a	2023-01-06 17:48:48.711995171 +1300
+++ b	2023-01-06 17:49:12.471983923 +1300
@@ -1,5 +1,4 @@
 1
 2
-3
d4
 4
)";
        file.close();
    }

    {
        Patch::File file("a", std::ios_base::out);

        file << "1\n2\n3\n4\n";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.stdout_data(), "patching file a\n");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** malformed patch at line 7: d4\n\n");
    EXPECT_EQ(process.return_code(), 2);
}

COMPAT_TEST(reversed_patch_batch)
{
    {
        Patch::File file("a", std::ios_base::out);
        file << "1\nb\n3\n";
        file.close();
    }

    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
--- a	2022-09-27 19:32:58.112960376 +1300
+++ b	2022-09-27 19:33:04.088209645 +1300
@@ -1,3 +1,3 @@
 1
-2
+b
 3
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "--batch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file a\nReversed (or previously applied) patch detected!  Assuming -R.\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("a", "1\n2\n3\n");
}

COMPAT_TEST(basic_add_symlink_file)
{
#ifndef _WIN32
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
commit 00072ad856ca2fa4fe34b4c79bd656a4cfaec81e (HEAD -> master)
Author: Shannon Booth <shannon.ml.booth@gmail.com>
Date:   Sun Apr 9 12:44:55 2023 +1200

    add symlink

diff --git a/active b/active
new file mode 120000
index 0000000..2e65efe
--- /dev/null
+++ b/active
@@ -0,0 +1 @@
+a
\ No newline at end of file
)";
        file.close();
    }

    const std::string content = "some file content that the symlink is pointing to!\n";
    {
        Patch::File file("a", std::ios_base::out);
        file << content;
    }

    // Sanity check
    EXPECT_FALSE(Patch::filesystem::exists("active"));
    EXPECT_FALSE(Patch::filesystem::is_symlink("active"));

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching symbolic link active\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    // Symlink exists and is pointing to valid file that has our contents.
    EXPECT_TRUE(Patch::filesystem::is_symlink("active"));
    EXPECT_TRUE(Patch::filesystem::exists("active"));
    EXPECT_FILE_EQ("active", content);
#endif
}

TEST(file_mode_symlink_predicate)
{
    EXPECT_TRUE(Patch::filesystem::is_symlink(0120000)); // symbolic link

    EXPECT_FALSE(Patch::filesystem::is_symlink(0160000)); // git submodule
    EXPECT_FALSE(Patch::filesystem::is_symlink(0100644)); // regular file
    EXPECT_FALSE(Patch::filesystem::is_symlink(0100755)); // executable file
    EXPECT_FALSE(Patch::filesystem::is_symlink(0040000)); // directory
    EXPECT_FALSE(Patch::filesystem::is_symlink(0140000)); // socket
    EXPECT_FALSE(Patch::filesystem::is_symlink(0170000)); // all type bits set
    EXPECT_FALSE(Patch::filesystem::is_symlink(0));       // unspecified

    // The permission bits must not influence the type.
    EXPECT_TRUE(Patch::filesystem::is_symlink(0120777));
    EXPECT_FALSE(Patch::filesystem::is_symlink(0160777));
}

static void write_add_submodule_patch()
{
    Patch::File file("diff.patch", std::ios_base::out);

    file << R"(diff --git a/sub b/sub
new file mode 160000
index 0000000..2ab5a7b
--- /dev/null
+++ b/sub
@@ -0,0 +1 @@
+Subproject commit 2ab5a7b1234567890123456789012345678901234
)";
    file.close();
}

static void write_escaping_patch(const std::string& name)
{
    Patch::File file("diff.patch", std::ios_base::out);
    file << "--- " << name << "\n+++ " << name << "\n@@ -1 +1 @@\n-ORIGINAL\n+PATCHED\n";
    file.close();
}

static void write_outside_file()
{
    Patch::filesystem::create_directory("work");
    Patch::File file("outside.txt", std::ios_base::out);
    file << "ORIGINAL\n";
    file.close();
}

// NOTE: not a COMPAT_TEST. GNU patch also refuses this name, but says nothing about why.
PATCH_TEST(patch_refuses_name_escaping_the_directory)
{
    write_outside_file();
    Patch::chdir("work");
    write_escaping_patch("a/../../outside.txt");

    Process process(patch_path, { patch_path, "--batch", "-p1", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(Ignoring potentially dangerous file name ../../outside.txt
can't find file to patch at input line 3
Perhaps you used the wrong -p or --strip option?
The text leading up to this was:
--------------------------
|--- a/../../outside.txt
|+++ a/../../outside.txt
--------------------------
No file to patch.  Skipping patch.
1 out of 1 hunk ignored
)");
    EXPECT_EQ(process.return_code(), 1);

    Patch::chdir("..");
    EXPECT_FILE_EQ("outside.txt", "ORIGINAL\n");
}

COMPAT_TEST(patch_refuses_name_with_dot_dot_inside_the_directory)
{
    Patch::filesystem::create_directory("sub");
    {
        Patch::File file("target.txt", std::ios_base::out);
        file << "ORIGINAL\n";
        file.close();
    }
    write_escaping_patch("sub/../target.txt");

    Process process(patch_path, { patch_path, "--batch", "-p0", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), R"(Ignoring potentially dangerous file name sub/../target.txt
can't find file to patch at input line 3
Perhaps you used the wrong -p or --strip option?
The text leading up to this was:
--------------------------
|--- sub/../target.txt
|+++ sub/../target.txt
--------------------------
No file to patch.  Skipping patch.
1 out of 1 hunk ignored
)");
    EXPECT_EQ(process.return_code(), 1);
    EXPECT_FILE_EQ("target.txt", "ORIGINAL\n");
}

COMPAT_TEST(patch_allows_strip_to_remove_dot_dot)
{
    Patch::filesystem::create_directory("foo");
    {
        Patch::File file("foo/bar.txt", std::ios_base::out);
        file << "ORIGINAL\n";
        file.close();
    }
    write_escaping_patch("../../foo/bar.txt");

    Process process(patch_path, { patch_path, "--batch", "-p2", "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file foo/bar.txt\n");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("foo/bar.txt", "PATCHED\n");
}

COMPAT_TEST(patch_allows_explicit_path_outside_the_directory)
{
    write_outside_file();
    Patch::chdir("work");
    write_escaping_patch("whatever");

    Process process(patch_path, { patch_path, "--batch", "-p0", "-i", "diff.patch", "../outside.txt", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file ../outside.txt\n");
    EXPECT_EQ(process.return_code(), 0);

    Patch::chdir("..");
    EXPECT_FILE_EQ("outside.txt", "PATCHED\n");
}

COMPAT_TEST(git_add_submodule_is_not_a_symlink)
{
    write_add_submodule_patch();

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file sub\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FALSE(Patch::filesystem::is_symlink("sub"));
    EXPECT_TRUE(Patch::filesystem::is_regular_file("sub"));
}

// Not a COMPAT_TEST: GNU patch seems to be unable to reverse this.
PATCH_TEST(git_add_submodule_is_readable_and_reversible)
{
    write_add_submodule_patch();

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });
    EXPECT_EQ(process.return_code(), 0);

    EXPECT_FILE_EQ("sub", "Subproject commit 2ab5a7b1234567890123456789012345678901234\n");

    const auto permissions = Patch::filesystem::get_permissions("sub");
    EXPECT_TRUE((permissions & Patch::filesystem::perms::owner_read) != Patch::filesystem::perms::none);
    EXPECT_TRUE((permissions & Patch::filesystem::perms::owner_write) != Patch::filesystem::perms::none);

    Process reverse(patch_path, { patch_path, "-R", "-i", "diff.patch", nullptr });
    EXPECT_EQ(reverse.stdout_data(), "patching file sub\n");
    EXPECT_EQ(reverse.return_code(), 0);
    EXPECT_FALSE(Patch::filesystem::exists("sub"));
}

COMPAT_TEST(basic_add_symlink_file_to_stdout)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);

        file << R"(
diff --git a/b b/b
new file mode 120000
index 0000000..2e65efe
--- /dev/null
+++ b/b
@@ -0,0 +1 @@
+a
\ No newline at end of file
)";
        file.close();
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "-o-", nullptr });

    EXPECT_EQ(process.stdout_data(), "a");
    EXPECT_EQ(process.stderr_data(), "patching symbolic link - (read from b)\n");
    EXPECT_EQ(process.return_code(), 0);
}

COMPAT_TEST(regular_patch_refuses_to_follow_symlink)
{
    Patch::skip_without_symlink_support();

    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(--- a
+++ a
@@ -1 +1 @@
-old
+new
)";
    }

    const std::string content = "old\n";
    {
        Patch::File file("real", std::ios_base::out);
        file << content;
    }

    Patch::filesystem::symlink("real", "a");

    Process process(patch_path, { patch_path, "-i", "diff.patch", nullptr });

    EXPECT_EQ(process.return_code(), 1);
    EXPECT_EQ(process.stdout_data(),
        "File a is not a regular file -- refusing to patch\n"
        "1 out of 1 hunk ignored -- saving rejects to file a.rej\n");

    EXPECT_TRUE(Patch::filesystem::is_symlink("a"));
    EXPECT_FILE_EQ("real", content);
}
