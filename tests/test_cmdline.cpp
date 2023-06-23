// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <array>
#include <patch/cmdline.h>
#include <patch/options.h>
#include <patch/test.h>
#include <stdexcept>

static Patch::Options parse_cmdline(size_t argc, const char* const* argv)
{
    Patch::OptionHandler handler;
    Patch::CmdLineParser cmdline(static_cast<int>(argc), argv);
    cmdline.parse(handler);
    handler.apply_defaults();
    return handler.options();
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

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "unrecognized option '--unknown-flag'");
}

TEST(cmdline_flag_with_missing_operand)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--input",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "option missing operand for --input");
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

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "strip count q is not a number");
}

TEST(cmdline_with_invalid_fuzz_option_set)
{
    const std::vector<const char*> dummy_args {
        "./patch",
        "--fuzz=thingy",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "fuzz factor thingy is not a number");
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

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());
    EXPECT_EQ(options.patch_file_path, "");
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
        "-ye",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "invalid option -- 'y'");
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

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "unrecognized option '--def=thing'");
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

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "third-positional-arg: extra operand");
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

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "strip count 3p is not a number");
}

TEST(cmdline_boolean_followed_by_integer_then_alpha_in_single_arg)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "-cp0b",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "strip count 0b is not a number");
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

TEST(cmdline_boolean_followed_by_equal)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--help=",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "option '--help' doesn't allow an argument");
}

TEST(cmdline_long_option_only_partially_specified_unambiguous)
{
    const std::vector<const char*> version_options {
        "--help",
        "--hel",
        "--he",
        "--h",
    };

    for (const char* option : version_options) {
        const std::vector<const char*> dummy_args {
            "patch",
            option,
            nullptr,
        };

        auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

        EXPECT_TRUE(options.show_help);
    }
}

TEST(cmdline_long_option_only_partially_specified_ambiguous)
{
    struct TestData {
        const char* input;
        const char* error;
    };
    const std::vector<TestData> test_data {
        { "--reject-f", "option '--reject-f' is ambiguous; possibilities: '--reject-file' '--reject-format'" },
        { "--reject-", "option '--reject-' is ambiguous; possibilities: '--reject-file' '--reject-format'" },
        { "--reject", "option '--reject' is ambiguous; possibilities: '--reject-file' '--reject-format'" },
        { "--rejec", "option '--rejec' is ambiguous; possibilities: '--reject-file' '--reject-format'" },
        { "--reje", "option '--reje' is ambiguous; possibilities: '--reject-file' '--reject-format'" },
        { "--rej", "option '--rej' is ambiguous; possibilities: '--reject-file' '--reject-format'" },
        { "--re", "option '--re' is ambiguous; possibilities: '--remove-empty-files' '--reverse' '--reject-file' '--read-only' '--reject-format'" },
        { "--r", "option '--r' is ambiguous; possibilities: '--remove-empty-files' '--reverse' '--reject-file' '--read-only' '--reject-format'" },
    };

    for (const auto& data : test_data) {
        const std::vector<const char*> dummy_args {
            "patch",
            data.input,
            "context",
            nullptr,
        };

        EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error, data.error);
    }
}

TEST(cmdline_bad_quote_style_cmdline_throws)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--quoting-style=bad",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "unrecognized quoting style bad");
}

TEST(cmdline_bad_quote_style_env_is_defaulted)
{
    const std::vector<const char*> dummy_args {
        "patch",
        nullptr,
    };

    Patch::set_env("QUOTING_STYLE", "bad");

    auto options = parse_cmdline(dummy_args.size() - 1, dummy_args.data());

    EXPECT_EQ(options.quoting_style, Patch::Options::QuotingStyle::Shell);
}

TEST(cmdline_bad_reject_format)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--reject-format=unknown!!",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "unrecognized reject format unknown!!");
}

TEST(cmdline_bad_read_only_handling)
{
    const std::vector<const char*> dummy_args {
        "patch",
        "--read-only=another-bad-option",
        nullptr,
    };

    EXPECT_THROW_WITH_MSG(parse_cmdline(dummy_args.size() - 1, dummy_args.data()), Patch::cmdline_parse_error,
        "unrecognized read-only handling another-bad-option");
}
