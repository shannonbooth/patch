// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/applier.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <sstream>

TEST(Reject, UnifiedRemoveLineNoOffset)
{
    std::stringstream patch_file(R"(
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

    std::stringstream input_file(R"(1
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

    auto expected_output = input_file.str();
    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;

    std::stringstream reject;
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::apply_patch(output, reject_writer, input_file, patch);
    EXPECT_EQ(output.str(), expected_output);

    EXPECT_EQ(reject.str(),
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

TEST(Reject, ContextRemoveLine)
{
    std::stringstream patch_file(R"(
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

    std::stringstream input_file(R"(int main()
{
/	return 0;
}
)");

    auto expected_output = input_file.str();

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::Options options;
    options.force = true;
    Patch::apply_patch(output, reject_writer, input_file, patch, options);

    EXPECT_EQ(output.str(), expected_output);

    EXPECT_EQ(reject.str(),
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

TEST(Reject, OffsetInPreviousHunkAppliesToReject)
{
    std::stringstream patch_file(R"(
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

    std::stringstream input_file(R"(// newly added line
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

    auto expected_output = input_file.str();

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;
    Patch::RejectWriter reject_writer(patch, reject);

    Patch::apply_patch(output, reject_writer, input_file, patch);

    EXPECT_EQ(output.str(), R"(// newly added line
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

    EXPECT_EQ(reject.str(), R"(--- main.cpp	2022-06-07 20:08:07.722685716 +1200
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
