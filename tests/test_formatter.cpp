// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/file.h>
#include <patch/formatter.h>
#include <patch/hunk.h>
#include <patch/patch.h>
#include <patch/system.h>
#include <patch/test.h>

TEST(formatter_change_lines)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,4 +1,5 @@
 int main()
 {
-    return 0;
+    int x = 4;
+    return x;
 }
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);

    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,4 ****
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

TEST(formatter_add_line)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
)");
}

TEST(formatter_remove_line)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,4 ****
  int main()
  {
-     return 0;
  }
--- 1,3 ----
)");
}

TEST(formatter_more_complex_patch)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,7 +1,6 @@
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

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);

    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,7 ****
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

TEST(formatter_only_one_line_in_from_files)
{
    Patch::Hunk hunk;
    hunk.lines = {
        { '-', "2" },
    };

    hunk.old_file_range.start_line = 2;
    hunk.old_file_range.number_of_lines = 1;
    hunk.new_file_range.start_line = 1;
    hunk.new_file_range.number_of_lines = 0;

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);
    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -2 +1,0 @@
-2
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 2 ****
- 2
--- 1 ----
)");
}

TEST(formatter_no_new_line_at_end_of_file_both_sides)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);

    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
\ No newline at end of file
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
\ No newline at end of file
)");
}

TEST(formatter_no_new_line_at_end_of_file_for_to_file)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);

    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,3 +1,4 @@
 int main()
 {
-}
+    return 0;
+}
\ No newline at end of file
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,3 ****
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

TEST(formatter_no_new_line_at_end_of_file_for_old_file)
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

    Patch::File ss1 = Patch::File::create_temporary();
    Patch::write_hunk_as_unified(hunk, ss1);

    EXPECT_EQ(ss1.read_all_as_string(), R"(@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
-}
\ No newline at end of file
+}
)");

    Patch::File ss2 = Patch::File::create_temporary();
    Patch::write_hunk_as_context(hunk, ss2);
    EXPECT_EQ(ss2.read_all_as_string(), R"(*** 1,4 ****
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
