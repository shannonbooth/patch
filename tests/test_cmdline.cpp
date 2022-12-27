// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <patch/cmdline.h>
#include <patch/options.h>
#include <stdexcept>
#include <test.h>

static Patch::Options parse_cmdline(size_t argc, const char* const* argv)
{
    Patch::CmdLineParser cmdline(static_cast<int>(argc), argv);
    return cmdline.parse();
}

TEST(cmdline_input_file_long_arg)
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

TEST(cmdline_input_file_short_arg)
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

TEST(cmdline_input_file_short_arg_combined)
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

TEST(cmdline_show_help)
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

TEST(cmdline_no_input)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_FALSE(options.show_help);
    EXPECT_EQ(options.patch_file_path, "");
}

TEST(cmdline_unknown_input)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--unknown-flag",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_flag_with_missing_operand)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_with_strip_option_set)
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

TEST(cmdline_strip_option_set_combined)
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

TEST(cmdline_with_invalid_strip_option_set)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-p",
        "q",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_with_long_opt_set_with_equal_sign)
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

TEST(cmdline_with_long_opt_set_with_equal_sign_and_no_argument)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input=",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_with_multiple_short_options)
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

TEST(cmdline_unknown_short_argument)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "-te",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_posix_positional_argument)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "/usr/bin/getsubmodules.py",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.file_to_patch, "/usr/bin/getsubmodules.py");
}

TEST(cmdline_gnu_supported_file_to_patch)
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

TEST(cmdline_gnu_extension_support_positional_args_at_beginning)
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

TEST(cmdline_using_delimeter)
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

TEST(cmdline_using_dash_as_stdin)
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

TEST(cmdline_short_attached_option_followed_by_another_option)
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

TEST(cmdline_partial_long_option)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--def=thing",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_newline_handling_lF)
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

TEST(cmdline_too_many_operands)
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

TEST(cmdline_boolean_followed_by_integer_in_single_arg)
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

TEST(cmdline_long_option_integer)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--strip=3",
        nullptr,
    };

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());
    EXPECT_EQ(options.strip_size, 3);
}

TEST(cmdline_long_option_integer_bad_value)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--strip=3p",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_boolean_followed_by_integer_then_alpha_in_single_arg)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "-cp0b",
        nullptr,
    };

    EXPECT_THROW(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error);
}

TEST(cmdline_boolean_followed_by_string_in_single_arg)
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
