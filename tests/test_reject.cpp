// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/applier.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <patch/test.h>
#include <sstream>

TEST(reject_unified_remove_line_no_offset)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- 1	2022-04-24 12:58:33.100280281 +1200
+++ 2	2022-04-24 12:58:31.560276720 +1200
@@ -2,7 +2,6 @@
 2
 3
 4
-5
 6
 7
 8
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(1
2
3
5
5
5
7
8
9
10
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File output = Patch::File::create_temporary();

    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(output.read_all_as_string(), input_file.read_all_as_string());

    EXPECT_EQ(reject.read_all_as_string(),
        R"(--- 1	2022-04-24 12:58:33.100280281 +1200
+++ 2	2022-04-24 12:58:31.560276720 +1200
@@ -2,7 +2,6 @@
 2
 3
 4
-5
 6
 7
 8
)");
}

TEST(reject_context_remove_line)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
*** a.cpp	2022-04-25 10:47:18.388073392 +1200
--- b.cpp	2022-04-25 10:04:06.012170915 +1200
***************
*** 1,4 ****
  int main()
  {
- 	return 0;
  }
--- 1,3 ----
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(int main()
{
/	return 0;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File output = Patch::File::create_temporary();
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::Options options;
    options.force = true;
    Patch::apply_patch(output, reject_writer, input_file, patch, options);

    EXPECT_EQ(output.read_all_as_string(), input_file.read_all_as_string());

    EXPECT_EQ(reject.read_all_as_string(),
        R"(*** a.cpp	2022-04-25 10:47:18.388073392 +1200
--- b.cpp	2022-04-25 10:04:06.012170915 +1200
***************
*** 1,4 ****
  int main()
  {
- 	return 0;
  }
--- 1,3 ----
)");
}

TEST(reject_offset_in_previous_hunk_applies_to_reject)
{
    Patch::File patch_file = Patch::File::create_temporary_with_content(R"(
--- main.cpp	2022-06-07 20:08:07.722685716 +1200
+++ second_main.cpp	2022-06-07 20:08:22.863130397 +1200
@@ -1,5 +1,6 @@
 int main()
 {
+	return 0;
 }
 //
 int another()
@@ -9,5 +10,5 @@
 //
 int one_more()
 {
-	return 0;
+	return 1;
 }
)");

    Patch::File input_file = Patch::File::create_temporary_with_content(R"(// newly added line
// ... and another
int main()
{
}
//
int another()
{
	return 1 - 2;
}
// some
int one_more() // comments to
{ // cause
	return 0;
} // reject
)");

    auto patch = Patch::parse_patch(patch_file);
    Patch::File output = Patch::File::create_temporary();
    Patch::File reject = Patch::File::create_temporary();
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::apply_patch(output, reject_writer, input_file, patch);

    EXPECT_EQ(output.read_all_as_string(), R"(// newly added line
// ... and another
int main()
{
	return 0;
}
//
int another()
{
	return 1 - 2;
}
// some
int one_more() // comments to
{ // cause
	return 0;
} // reject
)");

    EXPECT_EQ(reject.read_all_as_string(), R"(--- main.cpp	2022-06-07 20:08:07.722685716 +1200
+++ second_main.cpp	2022-06-07 20:08:22.863130397 +1200
@@ -10,5 +11,5 @@
 //
 int one_more()
 {
-	return 0;
+	return 1;
 }
)");
}
