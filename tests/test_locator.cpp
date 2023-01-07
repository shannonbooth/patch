// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/hunk.h>
#include <patch/locator.h>
#include <patch/patch.h>
#include <patch/system.h>
#include <patch/test.h>
#include <sstream>

TEST(locator_matches_ignoring_whitespace)
{
    EXPECT_FALSE(Patch::matches_ignoring_whitespace("c", " c"));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace(" c", "c"));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace("c ", " c"));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace(" c", "c "));

    EXPECT_TRUE(Patch::matches_ignoring_whitespace("\tc", " c"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace(" c", "\tc"));

    EXPECT_TRUE(Patch::matches_ignoring_whitespace("a", "a"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("with tab", "with\ttab"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("trailing whitespace   ", "trailing whitespace"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("trailing whitespace", "trailing whitespace   "));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("tabbed  trailing\t", "tabbed  trailing"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("different in-between-spacing", "different  \tin-between-spacing"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("trailing\t whitespace   ", "trailing\t whitespace"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("a b c d", "a\tb\tc\td"));

    EXPECT_TRUE(Patch::matches_ignoring_whitespace("c", "c "));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("c ", "c"));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace(" c ", " c "));

    EXPECT_TRUE(Patch::matches_ignoring_whitespace(" ", ""));
    EXPECT_TRUE(Patch::matches_ignoring_whitespace("", " "));

    EXPECT_FALSE(Patch::matches_ignoring_whitespace("   a", ""));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace("", "   a"));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace("", "a"));

    EXPECT_FALSE(Patch::matches_ignoring_whitespace("a", "b"));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace("ab", "a b"));
    EXPECT_FALSE(Patch::matches_ignoring_whitespace("a b c d", "abcd"));
}

TEST(locator_matches)
{
    // Matching content, changing newlines.
    EXPECT_TRUE(Patch::matches(Patch::Line("some content", Patch::NewLine::LF), Patch::Line("some content", Patch::NewLine::LF), false));
    EXPECT_TRUE(Patch::matches(Patch::Line("some content", Patch::NewLine::CRLF), Patch::Line("some content", Patch::NewLine::CRLF), false));
    EXPECT_TRUE(Patch::matches(Patch::Line("some content", Patch::NewLine::None), Patch::Line("some content", Patch::NewLine::None), false));
    EXPECT_FALSE(Patch::matches(Patch::Line("some content", Patch::NewLine::CRLF), Patch::Line("some content", Patch::NewLine::LF), false));
    EXPECT_TRUE(Patch::matches(Patch::Line("some content", Patch::NewLine::CRLF), Patch::Line("some content", Patch::NewLine::None), true));
    EXPECT_TRUE(Patch::matches(Patch::Line("some content", Patch::NewLine::CRLF), Patch::Line("some content", Patch::NewLine::LF), true));
    EXPECT_TRUE(Patch::matches(Patch::Line("some content", Patch::NewLine::CRLF), Patch::Line("some content", Patch::NewLine::None), true));

    // Different content
    EXPECT_FALSE(Patch::matches(Patch::Line("some content1", Patch::NewLine::LF), Patch::Line("some content2", Patch::NewLine::LF), true));
    EXPECT_FALSE(Patch::matches(Patch::Line("some content1", Patch::NewLine::LF), Patch::Line("some content2", Patch::NewLine::None), true));

    // Similar
    EXPECT_TRUE(Patch::matches(Patch::Line("some\tcontent", Patch::NewLine::LF), Patch::Line("some content", Patch::NewLine::None), true));
    EXPECT_FALSE(Patch::matches(Patch::Line("some\tcontent", Patch::NewLine::LF), Patch::Line("some content", Patch::NewLine::None), false));
    EXPECT_FALSE(Patch::matches(Patch::Line("some\tcontent", Patch::NewLine::LF), Patch::Line("some content", Patch::NewLine::LF), false));
    EXPECT_TRUE(Patch::matches(Patch::Line("some\tcontent", Patch::NewLine::LF), Patch::Line("some content", Patch::NewLine::LF), true));
}

TEST(locator_finds_hunk_perfect_match)
{
    const std::vector<Patch::Line> file_content = {
        { "int add(int a, int b)", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return a + b;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "int subtract(int a, int b)", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return a - b;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "int main()", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return 0;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "" },
        { ' ', "int subtract(int a, int b)" },
        { ' ', "{" },
        { '-', "    return a - b;" },
        { '+', "    return a + b;" },
        { ' ', "}" },
        { ' ', "" },
        { ' ', "int main()" },
    };

    hunk.old_file_range.start_line = 5;
    hunk.old_file_range.number_of_lines = 7;
    hunk.new_file_range.start_line = 5;
    hunk.new_file_range.number_of_lines = 7;

    auto location = Patch::locate_hunk(file_content, hunk);

    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.line_number, 4);
    EXPECT_EQ(location.fuzz, 0);
    EXPECT_EQ(location.offset, 0);
}

TEST(locator_finds_hunk_offset_one_increase)
{
    const std::vector<Patch::Line> file_content = {
        { "int add(int a, int b)", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return a + b;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "", Patch::NewLine::LF }, // Extra line added here, hunk location should be offset by one.
        { "int subtract(int a, int b)", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return a - b;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "int main()", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return 0;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "" },
        { ' ', "int subtract(int a, int b)" },
        { ' ', "{" },
        { '-', "    return a - b;" },
        { '+', "    return a + b;" },
        { ' ', "}" },
        { ' ', "" },
        { ' ', "int main()" },
    };

    hunk.old_file_range.start_line = 5;
    hunk.old_file_range.number_of_lines = 7;
    hunk.new_file_range.start_line = 5;
    hunk.new_file_range.number_of_lines = 7;

    auto location = Patch::locate_hunk(file_content, hunk);

    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.line_number, 5);
    EXPECT_EQ(location.fuzz, 0);
    EXPECT_EQ(location.offset, 1);
}

TEST(locator_finds_hunk_offset_one_decrease)
{
    const std::vector<Patch::Line> file_content = {
        { "int add(int a, int b)", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        //  The following line is removed here, causing an offset of -1 "    return a + b;"
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "int subtract(int a, int b)", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return a - b;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "int main()", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return 0;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "" },
        { ' ', "int subtract(int a, int b)" },
        { ' ', "{" },
        { '-', "    return a - b;" },
        { '+', "    return a + b;" },
        { ' ', "}" },
        { ' ', "" },
        { ' ', "int main()" },
    };

    hunk.old_file_range.start_line = 5;
    hunk.old_file_range.number_of_lines = 7;
    hunk.new_file_range.start_line = 5;
    hunk.new_file_range.number_of_lines = 7;

    auto location = Patch::locate_hunk(file_content, hunk);

    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.line_number, 3);
    EXPECT_EQ(location.fuzz, 0);
    EXPECT_EQ(location.offset, -1);
}

TEST(locator_finds_hunk_using_fuzz_one)
{
    const std::vector<Patch::Line> file_content = {
        { "int main()", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return 0;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main(int argc, char** argv)" },
        { ' ', "{" },
        { '-', "    return 0;" },
        { '+', "    return 1;" },
        { ' ', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 4;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 4;

    auto location = Patch::locate_hunk(file_content, hunk);

    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.line_number, 0);
    EXPECT_EQ(location.fuzz, 1);
    EXPECT_EQ(location.offset, 0);
}

TEST(locator_finds_hunk_using_fuzz_two)
{
    const std::vector<Patch::Line> file_content = {
        { "int main() // some comment for first fuzz", Patch::NewLine::LF },
        { "{ // some comment for second fuzz", Patch::NewLine::LF },
        { "    return 0;", Patch::NewLine::LF },
        { "} // some comment for last second fuzz", Patch::NewLine::LF },
        { "// comment to make whitespace around patch even", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '-', "    return 0;" },
        { '+', "    return 1;" },
        { ' ', "}" },
        { ' ', "// comment so that hunk is even" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 4;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 4;

    auto location = Patch::locate_hunk(file_content, hunk);
    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.line_number, 0);
    EXPECT_EQ(location.fuzz, 2);
    EXPECT_EQ(location.offset, 0);
}

TEST(locator_asymmetric_hunk_less_suffix)
{
    // Test a behavioral difference found by testing against GNU patch
    // where a patch with less trailing context was only resulting in a
    // fuzz of '1' instead of '2' because there was less trailing
    // contextual information in the patch file, and we were not taking
    // that into account.
    const std::vector<Patch::Line> file_content = {
        { "int main()", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "    return 1;", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '+', "    return 1;" },
        { ' ', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 3;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 4;

    auto location = Patch::locate_hunk(file_content, hunk);
    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.line_number, 0);
    EXPECT_EQ(location.fuzz, 2);
    EXPECT_EQ(location.offset, 0);
}

TEST(locator_remove_file_does_not_apply)
{
    // Test a regression where a removal patch was always returning
    // successfully located due to the locator incorrectly assuming
    // that patches with no context would always succeed (and instead
    // should have only be checking if the from file content contains
    // no lines).

    const std::vector<Patch::Line> file_content = {
        { "int", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { '-', "int main()" },
        { '-', "{" },
        { '-', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 3;
    hunk.new_file_range.start_line = 0;
    hunk.new_file_range.number_of_lines = 0;

    auto location = Patch::locate_hunk(file_content, hunk);
    EXPECT_FALSE(location.is_found());
}

TEST(DISABLED_addition_patch_is_locating_against_preexisting_file)
{
    // Test a behavioral difference found by testing against GNU patch
    // for the situation that a patch that is creating a file already
    // exists and has some content that does not match what the patch
    // specifies. We always apply the patch without even any fuzz!

    const std::vector<Patch::Line> file_content = {
        { "x", Patch::NewLine::LF },
        { "y", Patch::NewLine::LF },
        { "z", Patch::NewLine::LF },
    };

    Patch::Hunk hunk;
    hunk.lines = {
        { '+', "int main()" },
        { '+', "{" },
        { '+', "}" },
    };

    hunk.old_file_range.start_line = 0;
    hunk.old_file_range.number_of_lines = 0;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 3;

    auto location = Patch::locate_hunk(file_content, hunk);
    EXPECT_FALSE(location.is_found());
}

TEST(locator_hunk_at_beginning_of_file_is_offset)
{
    // Test a behavioral difference that was noticed between this implementation
    // and GNU patch where GNU patch seems to be saying that fuzz needs to be
    // applied when a hunk which was at the beginning of a file is now no longer
    // at the beginning of a file.
    //
    // I'm not sure what the requirement here is actually from the specification,
    // or if it is a bug - or if this results in any undesirable side effects.
    // Instead of blindly adjusting our behaviour to match, write this test
    // for the future if our behaviour ever adjusts to match what GNU patch does
    // (intended or not).
    //
    // Change this test case if this is behaviour that we want!

    const std::vector<Patch::Line> file_content = {
        { "// newly added line", Patch::NewLine::LF },
        { "// ... and another", Patch::NewLine::LF },
        { "int main()", Patch::NewLine::LF },
        { "{", Patch::NewLine::LF },
        { "}", Patch::NewLine::LF },
        { "", Patch::NewLine::LF },
        { "int another()", Patch::NewLine::LF },
    };

    // Theory for difference - GNU patch may be considering before the start of the file
    // as part of the context, whereas in our implementation we set the context for both
    // pre and post as max(thing, 0) to prevent negative contexts. Perhaps we should
    // consider negative contexts as a 'fail' to be stripped by fuzz until it is not
    // negative?
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '+', "	return 0;" },
        { ' ', "}" },
        { ' ', "" },
        { ' ', "int another()" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 5;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 6;

    auto location = Patch::locate_hunk(file_content, hunk);
    EXPECT_TRUE(location.is_found());
    EXPECT_EQ(location.offset, 2);
    EXPECT_EQ(location.line_number, 2);
    EXPECT_EQ(location.fuzz, 0); // GNU patch seems to get 2 here
}
