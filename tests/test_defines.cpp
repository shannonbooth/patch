// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <gtest/gtest.h>
#include <patch/applier.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <sstream>

TEST(Defines, AddOneLine)
{
    std::stringstream patch_file(R"(
--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,3 +1,4 @@
 int main()
 {
+    return 1;
 }
)");

    std::stringstream input_file(R"(int main()
{
}
)");

    std::stringstream expected_output(R"(int main()
{
#ifdef TEST_PATCH
    return 1;
#endif
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Options options;
    options.define_macro = "TEST_PATCH";

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Defines, RemoveOneLine)
{
    std::stringstream patch_file(R"(
--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
#ifndef TEST_PATCH
    return 0;
#endif
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Options options;
    options.define_macro = "TEST_PATCH";

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Defines, Mix)
{
    std::stringstream patch_file(R"(
--- file.cpp	2022-01-30 13:57:31.173528027 +1300
+++ file.cpp	2022-01-30 13:57:36.321216497 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
+    return 1;
 }
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
#ifndef TEST_PATCH
    return 0;
#else
    return 1;
#endif
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Options options;
    options.define_macro = "TEST_PATCH";

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Defines, ChangesAtEndOfFile)
{
    std::stringstream patch_file(R"(
--- main.cpp	2022-02-20 19:38:33.685996435 +1300
+++ main.cpp	2022-02-20 19:38:48.069436313 +1300
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
-}
+    return 1;
+} // some change to this last line.
)");

    std::stringstream input_file(R"(int main()
{
    return 0;
}
)");

    std::stringstream expected_output(R"(int main()
{
#ifndef TEST_PATCH
    return 0;
}
#else
    return 1;
} // some change to this last line.
#endif
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Options options;
    options.define_macro = "TEST_PATCH";

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}

TEST(Defines, MoreComplexPatch)
{
    std::stringstream patch_file(R"(
--- add.cpp	2022-02-26 13:15:21.624635744 +1300
+++ sub.cpp	2022-02-26 13:15:07.396899118 +1300
@@ -1,17 +1,17 @@
-int add(int a, int b)
+int subtract(int a, int b)
 {
-	return a + b;
+	return a - b;
 }
 
-int subtract(int a, int b)
+int add(int a, int b)
 {
-	return a - b;
+	return a + b;
 }
 
-// To be removed comment
 int main()
 {
 	int x = add(1, 2);
 	int y = subtract(3, 4);
+	// Some comment
 	return x + y;
 }
)");

    std::stringstream input_file(R"(int add(int a, int b)
{
	return a + b;
}

int subtract(int a, int b)
{
	return a - b;
}

// To be removed comment
int main()
{
	int x = add(1, 2);
	int y = subtract(3, 4);
	return x + y;
}
)");

    std::stringstream expected_output(R"(#ifndef MY_PATCH
int add(int a, int b)
#else
int subtract(int a, int b)
#endif
{
#ifndef MY_PATCH
	return a + b;
#else
	return a - b;
#endif
}

#ifndef MY_PATCH
int subtract(int a, int b)
#else
int add(int a, int b)
#endif
{
#ifndef MY_PATCH
	return a - b;
#else
	return a + b;
#endif
}

#ifndef MY_PATCH
// To be removed comment
#endif
int main()
{
	int x = add(1, 2);
	int y = subtract(3, 4);
#ifdef MY_PATCH
	// Some comment
#endif
	return x + y;
}
)");

    auto patch = Patch::parse_patch(patch_file);
    std::stringstream output;
    std::stringstream reject;

    Patch::Options options;
    options.define_macro = "MY_PATCH";

    Patch::Result result = Patch::apply_patch(output, reject, input_file, patch, options);
    EXPECT_EQ(result.failed_hunks, 0);
    EXPECT_FALSE(result.was_skipped);
    EXPECT_EQ(output.str(), expected_output.str());
    EXPECT_EQ(reject.str(), "");
}
