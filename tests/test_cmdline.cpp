// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <gtest/gtest.h>
#include <patch/cmdline.h>
#include <patch/options.h>
#include <stdexcept>

static Patch::Options parse_cmdline(size_t argc, const char* const* argv)
{
    Patch::CmdLineParser cmdline(static_cast<int>(argc), argv);
    return cmdline.parse();
}

TEST(CmdLine, InputFileLongArg)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input",
        "my_file.txt",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.patch_file_path, "my_file.txt");
    EXPECT_FALSE(options.show_help);
}

TEST(CmdLine, InputFileShortArg)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-i",
        "my_file.txt",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.patch_file_path, "my_file.txt");
    EXPECT_FALSE(options.show_help);
}

TEST(CmdLine, InputFileShortArgCombined)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-imy_file.txt",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.patch_file_path, "my_file.txt");
    EXPECT_FALSE(options.show_help);
}

TEST(CmdLine, ShowHelp)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input",
        "my_file.txt",
        "--help",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    // don't care if other options set
    EXPECT_TRUE(options.show_help);
}

TEST(CmdLine, NoInput)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_FALSE(options.show_help);
    EXPECT_EQ(options.patch_file_path, "");
}

TEST(CmdLine, UnknownInput)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--unknown-flag",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, FlagWithMissingOperand)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, WithStripOptionSet)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-p",
        "5",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_FALSE(options.show_help);
    EXPECT_EQ(options.strip_size, 5);
}

TEST(CmdLine, StripOptionSetCombined)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-p2",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_FALSE(options.show_help);
    EXPECT_EQ(options.strip_size, 2);
}

TEST(CmdLine, WithInvalidStripOptionSet)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-p",
        "q",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, WithLongOptSetWithEqualSign)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input=new_file.txt",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_FALSE(options.show_help);
    EXPECT_EQ(options.patch_file_path, "new_file.txt");
}

TEST(CmdLine, WithLongOptSetWithEqualSignAndNoArgument)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input=",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, WithMultipleShortOptions)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-Re",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_TRUE(options.interpret_as_ed);
    EXPECT_TRUE(options.reverse_patch);
    EXPECT_FALSE(options.show_help);
}

TEST(CmdLine, UnknownShortArgument)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-te",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, PosixPositionalArgument)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "/usr/bin/getsubmodules.py",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.file_to_patch, "/usr/bin/getsubmodules.py");
}

TEST(CmdLine, GnuSupportedFileToPatch)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "/usr/bin/getsubmodules.py",
        "fix_get_submodules.patch",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.file_to_patch, "/usr/bin/getsubmodules.py");
    EXPECT_EQ(options.patch_file_path, "fix_get_submodules.patch");
}

TEST(CmdLine, GnuExtensionSupportPositionalArgsAtBeginning)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "a.cpp",
        "diff.patch",
        "--verbose",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.file_to_patch, "a.cpp");
    EXPECT_EQ(options.patch_file_path, "diff.patch");
    EXPECT_TRUE(options.verbose);
}

TEST(CmdLine, UsingDelimeter)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--context",
        "/usr/bin/getsubmodules.py",
        "--",
        "-filename-with-dash-at-start",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.file_to_patch, "/usr/bin/getsubmodules.py");
    EXPECT_EQ(options.patch_file_path, "-filename-with-dash-at-start");
    EXPECT_TRUE(options.interpret_as_context);
}

TEST(CmdLine, UsingDashAsStdin)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--context",
        "some-other_ file",
        "-",
        "--",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.file_to_patch, "some-other_ file");
    EXPECT_EQ(options.patch_file_path, "-");
    EXPECT_TRUE(options.interpret_as_context);
}

TEST(CmdLine, ShortAttachedOptionFollowedByAnotherOption)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "-p1",
        "-i",
        "fadb08e7c7501ed42949e646c6865ba4ec5dd948.patch",
        "-o",
        "-",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.out_file_path, "-");
    EXPECT_EQ(options.patch_file_path, "fadb08e7c7501ed42949e646c6865ba4ec5dd948.patch");
    EXPECT_EQ(options.strip_size, 1);
    EXPECT_EQ(options.newline_output, Patch::Options::NewlineOutput::Native);
}

TEST(CmdLine, PartialLongOption)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--def=thing",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, NewlineHandlingLF)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--newline-output",
        "lf",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.newline_output, Patch::Options::NewlineOutput::LF);
}

TEST(CmdLine, TooManyOperands)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--newline-output",
        "lf",
        "--",
        "first-positional-arg",
        "second-positional-arg",
        "third-positional-arg",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, DISABLED_BooleanFollowedByIntegerInSingleArg)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "-cp0",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_TRUE(options.interpret_as_context);
    EXPECT_EQ(options.strip_size, 0);
}

TEST(CmdLine, LongOptionInteger)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--strip=3",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());
    EXPECT_EQ(options.strip_size, 3);
}

TEST(CmdLine, LongOptionIntegerBadValue)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--strip=3p",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, BooleanFollowedByIntegerThenAlphaInSingleArg)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "-cp0b",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(CmdLine, BooleanFollowedByStringInSingleArg)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "-cisome input",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_TRUE(options.interpret_as_context);
    EXPECT_EQ(options.patch_file_path, "some input");
}
