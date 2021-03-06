// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/formatter.h>
#include <patch/hunk.h>
#include <patch/patch.h>
#include <patch/system.h>

TEST(Formatter, ChangeLines)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '-', "    return 0;" },
        { '+', "    int x = 4;" },
        { '+', "    return x;" },
        { ' ', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 4;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 5;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.str(), R"(@@ -1,4 +1,5 @@
 int main()
 {
-    return 0;
+    int x = 4;
+    return x;
 }
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);

    EXPECT_EQ(ss2.str(), R"(*** 1,4 ****
  int main()
  {
!     return 0;
  }
--- 1,5 ----
  int main()
  {
!     int x = 4;
!     return x;
  }
)");
}

TEST(Formatter, AddLine)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '+', "    return 0;" },
        { ' ', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 3;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 4;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.str(), R"(@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.str(), R"(*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
)");
}

TEST(Formatter, RemoveLine)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '-', "    return 0;" },
        { ' ', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 4;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 3;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.str(), R"(@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.str(), R"(*** 1,4 ****
  int main()
  {
-     return 0;
  }
--- 1,3 ----
)");
}

TEST(Formatter, MoreComplexPatch)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { '-', "A line that needs to be changed!" },
        { '-', "A similar line that needs to be changed is this." },
        { ' ', "some words..." },
        { '-', "xxx" },
        { '+', "yyy" },
        { '+', "" },
        { ' ', "123456" },
        { ' ', ")))" },
        { ' ', "key blob" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 7;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 6;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.str(), R"(@@ -1,7 +1,6 @@
-A line that needs to be changed!
-A similar line that needs to be changed is this.
 some words...
-xxx
+yyy
+
 123456
 )))
 key blob
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);

    EXPECT_EQ(ss2.str(), R"(*** 1,7 ****
- A line that needs to be changed!
- A similar line that needs to be changed is this.
  some words...
! xxx
  123456
  )))
  key blob
--- 1,6 ----
  some words...
! yyy
! 
  123456
  )))
  key blob
)");
}

TEST(Formatter, OnlyOneLineInFromFiles)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { '-', "2" },
    };

    hunk.old_file_range.start_line = 2;
    hunk.old_file_range.number_of_lines = 1;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 0;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.str(), R"(@@ -2 +1,0 @@
-2
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.str(), R"(*** 2 ****
- 2
--- 1 ----
)");
}

TEST(Formatter, NoNewLineAtEndOfFileBothSides)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '+', "    return 0;" },
        { ' ', { "}", Patch::NewLine::None } },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 3;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 4;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);

    EXPECT_EQ(ss1.str(), R"(@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
\ No newline at end of file
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.str(), R"(*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
\ No newline at end of file
)");
}

TEST(Formatter, NoNewLineAtEndOfFileForToFile)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '-', "}" },
        { '+', "    return 0;" },
        { '+', { "}", Patch::NewLine::None } },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 3;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 4;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);

    EXPECT_EQ(ss1.str(), R"(@@ -1,3 +1,4 @@
 int main()
 {
-}
+    return 0;
+}
\ No newline at end of file
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.str(), R"(*** 1,3 ****
  int main()
  {
! }
--- 1,4 ----
  int main()
  {
!     return 0;
! }
\ No newline at end of file
)");
}

TEST(Formatter, NoNewLineAtEndOfFileForOldFile)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { ' ', "int main()" },
        { ' ', "{" },
        { '-', "    return 0;" },
        { '-', { "}", Patch::NewLine::None } },
        { '+', "}" },
    };

    hunk.old_file_range.start_line = 1;
    hunk.old_file_range.number_of_lines = 4;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 3;

    std::stringstream ss1;
    Patch::write_hunk_as_unified(hunk, ss1);

    EXPECT_EQ(ss1.str(), R"(@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
-}
\ No newline at end of file
+}
)");

    std::stringstream ss2;
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.str(), R"(*** 1,4 ****
  int main()
  {
!     return 0;
! }
\ No newline at end of file
--- 1,3 ----
  int main()
  {
! }
)");
}
