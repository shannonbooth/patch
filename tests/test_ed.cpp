// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/parser.h>
#include <patch/process.h>
#include <patch/system.h>
#include <patch/test.h>

TEST(parser_ed_commands)
{
    Patch::EdCommand command;

    EXPECT_TRUE(Patch::parse_ed_command(command, "0a"));
    EXPECT_EQ(command.operation, Patch::EdOperation::Append);
    EXPECT_EQ(command.range.start_line, 0);
    EXPECT_EQ(command.range.number_of_lines, 1);

    EXPECT_TRUE(Patch::parse_ed_command(command, "3,5c"));
    EXPECT_EQ(command.operation, Patch::EdOperation::Change);
    EXPECT_EQ(command.range.start_line, 3);
    EXPECT_EQ(command.range.number_of_lines, 3);

    EXPECT_TRUE(Patch::parse_ed_command(command, "7d"));
    EXPECT_EQ(command.operation, Patch::EdOperation::Delete);

    EXPECT_TRUE(Patch::parse_ed_command(command, "4s/.//"));
    EXPECT_EQ(command.operation, Patch::EdOperation::SubstituteFirstCharacter);
}

TEST(parser_ed_rejects_invalid_commands)
{
    Patch::EdCommand command;

    EXPECT_FALSE(Patch::parse_ed_command(command, "0c"));
    EXPECT_FALSE(Patch::parse_ed_command(command, "0d"));
    EXPECT_FALSE(Patch::parse_ed_command(command, "3,2c"));
    EXPECT_FALSE(Patch::parse_ed_command(command, "1,2a"));
    EXPECT_FALSE(Patch::parse_ed_command(command, "1s/a/b/"));
    EXPECT_FALSE(Patch::parse_ed_command(command, "9223372036854775808d"));
    EXPECT_FALSE(Patch::parse_ed_command(command, "0,9223372036854775807c"));
}

TEST(parser_ed_patch_and_format_detection)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(3d
2c
two
.
0a
zero
.
)");

    auto patch = Patch::parse_patch(patch_file);
    EXPECT_EQ(patch.format, Patch::Format::Ed);
    EXPECT_EQ(patch.operation, Patch::Operation::Change);
    EXPECT_EQ(patch.ed_commands.size(), 3);
    EXPECT_EQ(patch.ed_commands[0].operation, Patch::EdOperation::Delete);
    EXPECT_EQ(patch.ed_commands[1].operation, Patch::EdOperation::Change);
    EXPECT_EQ(patch.ed_commands[1].lines.size(), 1);
    EXPECT_EQ(patch.ed_commands[1].lines[0].content, "two");
    EXPECT_EQ(patch.ed_commands[2].operation, Patch::EdOperation::Append);
}

PATCH_TEST(ed_patch_mixed_commands)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(6a
g
.
4,5c
E
.
1c
A
.
)";
    }
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "a\nb\nc\nd\ne\nf\n";
    }

    Process process(patch_path, { patch_path, "--ed", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), "");
    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", "A\nb\nc\nE\nf\ng\n");
}

PATCH_TEST(ed_patch_is_detected_automatically)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "2a\nc\nd\n.\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "a\nb\n";
    }

    Process process(patch_path, { patch_path, "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", "a\nb\nc\nd\n");
}

PATCH_TEST(ed_patch_handles_dot_lines)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << R"(0a
..
.
1s/.//
1a
..
...
.
)";
    }
    Patch::File("to_patch", std::ios_base::out);

    Process process(patch_path, { patch_path, "--ed", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("to_patch", ".\n..\n...\n");
}

PATCH_TEST(ed_patch_creates_missing_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "0a\ncreated\n.\n";
    }

    Process process(patch_path, { patch_path, "--ed", "-i", "diff.patch", "created-file", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_EQ("created-file", "created\n");
}

PATCH_TEST(ed_patch_preserves_inserted_line_endings)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);
        file << "1a\r\ninserted\r\n.\r\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "existing\n";
    }

    Process process(patch_path, { patch_path, "--ed", "--newline-output=preserve", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_BINARY_EQ("to_patch", "existing\ninserted\r\n");
}

PATCH_TEST(ed_patch_appends_after_missing_final_newline)
{
    {
        Patch::File file("diff.patch", std::ios_base::out | std::ios_base::binary);
        file << "1a\ninserted\n.\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out | std::ios_base::binary);
        file << "existing";
    }

    Process process(patch_path, { patch_path, "--ed", "--newline-output=lf", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FILE_BINARY_EQ("to_patch", "existing\ninserted\n");
}

PATCH_TEST(ed_patch_removes_empty_file)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "1,2d\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "a\nb\n";
    }

    Process process(patch_path, { patch_path, "--ed", "-E", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.return_code(), 0);
    EXPECT_FALSE(Patch::filesystem::exists("to_patch"));
}

PATCH_TEST(ed_patch_invalid_address_is_atomic)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "9d\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "unchanged\n";
    }

    Process process(patch_path, { patch_path, "--ed", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.stdout_data(), "patching file to_patch\n");
    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** ed command #1 has an invalid address: 9\n");
    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("to_patch", "unchanged\n");
}

PATCH_TEST(ed_patch_missing_terminator_is_atomic)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "1c\nreplacement\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "unchanged\n";
    }

    Process process(patch_path, { patch_path, "--ed", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("to_patch", "unchanged\n");
}

PATCH_TEST(ed_patch_cannot_be_reversed)
{
    {
        Patch::File file("diff.patch", std::ios_base::out);
        file << "1c\nchanged\n.\n";
    }
    {
        Patch::File file("to_patch", std::ios_base::out);
        file << "unchanged\n";
    }

    Process process(patch_path, { patch_path, "--ed", "--reverse", "-i", "diff.patch", "to_patch", nullptr });

    EXPECT_EQ(process.stderr_data(), std::string(patch_path) + ": **** ed patches cannot be reversed\n");
    EXPECT_EQ(process.return_code(), 2);
    EXPECT_FILE_EQ("to_patch", "unchanged\n");
}
