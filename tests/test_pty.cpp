// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <iostream>
#include <patch/file.h>
#include <patch/system.h>
#include <pty_spawn.h>
#include <stdexcept>
#include <test.h>

static void test(const char* patch_path)
{
    std::array<const char*, 4> cmd { patch_path, "-i", "diff.patch", nullptr };

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

    PtySpawn term(static_cast<int>(cmd.size()), cmd.data());

    term.write("y\n");

    const auto out = term.read();

    EXPECT_EQ(out, R"(patching file a
Reversed (or previously applied) patch detected! Assume -R? [n] )");
}

int main(int argc, const char* const* argv)
{
    if (argc != 2) {
        std::cerr << argc << " - Wrong number of arguments given\n";
        return 1;
    }

    std::string tmp_dir = Patch::filesystem::make_temp_directory();
    Patch::chdir(tmp_dir);

    try {
        test(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';

        std::string command = "rm -rf " + tmp_dir;
        std::system(command.c_str());
        return 1;
    }
}
