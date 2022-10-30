#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

import argparse
import os
import subprocess
import sys
import tempfile
import stat
import unittest

PATCH_PROGRAM = 'sb_patch'

def run(cmd, input=None):
    ''' run patch with some options '''
    try:
        return subprocess.run(cmd, text=True, capture_output=True, timeout=1, input=input, encoding='utf8')
    except subprocess.TimeoutExpired as e:
        print(e.stderr)
        print('------------------------------')
        print(e.stdout)
        raise

def run_patch(cmd, input=None):
    ''' run patch with some options '''
    return run([PATCH_PROGRAM] + cmd.split(' ')[1:], input)

class TestPatch(unittest.TestCase):


    def setUp(self):
        ''' set up the patch test with a temporary directory to manipulate patches inside of '''
        self.old_cwd = os.getcwd()
        self.tmp_dir = tempfile.TemporaryDirectory()
        os.chdir(self.tmp_dir.name)

    def tearDown(self):
        ''' clean up the patch test temporary directory and return to the old cwd '''
        os.chdir(self.old_cwd)
        self.tmp_dir.cleanup()

    def assertFileEqual(self, path, content):
        ''' assert that the given path has the expected file content '''
        with open(path, 'r', encoding='utf8') as file:
            file_content = file.read()
        self.assertEqual(file_content, content)

    def assertFileBinaryEqual(self, path, content):
        ''' assert that the given path has the expected file content '''
        with open(path, 'rb') as file:
            file_content = file.read()
        self.assertEqual(file_content, content)

    def test_patch_dash_filename_patch(self):
        ''' test that patch still applies a patch to a file named '-' '''
        patch = '''
diff --git a/- b/-
new file mode 100644
index 0000000..7a1c613
--- /dev/null
+++ b/-
@@ -0,0 +1 @@
+some file
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file -\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('-', 'some file\n')


    def test_ed_patches_not_supported(self):
        ''' test that when we try patch an ed patch file, we fail with an appropriate error '''
        patch = '3d\n'
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch --ed')
        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')
        self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** ed format patches are not supported by this version of patch\n')

    def test_backup_when_patch_is_adding_file(self):
        ''' test that when a patch creating a file has the backup option, a empty backup is made '''
        patch = '''
--- /dev/null	2022-06-08 13:09:24.217949672 +1200
+++ f	2022-06-08 20:44:55.455275623 +1200
@@ -0,0 +1 @@
+a line
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        self.assertFalse(os.path.exists('f')) # sanity check
        self.assertFalse(os.path.exists('f.orig')) # sanity check
        ret = run_patch('patch -b -i diff.patch')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file f\n')
        self.assertFileEqual('f', 'a line\n')
        self.assertFileEqual('f.orig', '')


    def test_backup_rename_patch(self):
        ''' test that no backup is created for a rename patch '''

        # NOTE: this might very well be a bug. However, GNU patch
        # also has this behavior, so this test is to ensure any
        # change here is made deliberately.
        patch = '''
commit 61593eeba9cf1663927cbccec2a15a987b6f9e53
Author: Shannon Booth <shannon.ml.booth@gmail.com>
Date:   Sun Sep 4 11:27:52 2022 +1200

    rename

diff --git a/a b/b
similarity index 100%
rename from a
rename to b
'''

        # FIXME: add a test for no newline patch...
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = 'ab'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -b -i diff.patch')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file b (renamed from a)\n')
        self.assertFileEqual('b', to_patch)
        self.assertFileEqual('b.orig', '')
        self.assertEqual(set(os.listdir()), {'b', 'b.orig', 'diff.patch'})


    def test_backup_on_top_of_existing_file(self):
        ''' test that when a patch creating a file has the backup option, a empty backup is made '''
        patch = '''
--- a	2022-06-19 13:12:11.460782360 +1200
+++ b	2022-06-19 13:12:19.472676546 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+    return 0;
 }
'''

        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        existing = 'some file content!\n'
        with open('a.orig', 'w', encoding='utf8') as existing_file:
            existing_file.write(existing)

        to_patch = '''int main()
{
}
'''
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -b -i diff.patch')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file a\n')
        self.assertFileEqual('a', '''int main()
{
    return 0;
}
''')
        self.assertFileEqual('a.orig', to_patch)

    def test_backup_multiple_files_only_backs_up_first(self):
        ''' test that if a patch changes same file twice, only the original is backed up '''
        patch = '''
--- a/main.cpp	2022-06-08 20:59:48.119369201 +1200
+++ b/main.cpp	2022-06-08 21:00:29.848423061 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-    return 0;
 }
--- b/main.cpp	2022-06-08 21:00:29.848423061 +1200
+++ c/main.cpp	2022-06-08 21:00:59.885147433 +1200
@@ -1,3 +1,4 @@
-int main()
+int main(int argc, char** argv)
 {
+	return 5; // and comment
 }
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 0;
}
'''
        with open('main.cpp', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -b -i diff.patch')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file main.cpp\npatching file main.cpp\n')
        self.assertFileEqual('main.cpp.orig', to_patch)
        self.assertFileEqual('main.cpp', '''int main(int argc, char** argv)
{
	return 5; // and comment
}
''')


    def test_backup_prefix_only(self):
        ''' test applying a patch with backup prefix '''
        patch = '''
--- x	2022-07-27 21:07:04.624795529 +1200
+++ y	2022-07-27 21:07:08.304813552 +1200
@@ -1 +1 @@
-abc
+xyz
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = 'abc\n'
        with open('x', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch --backup --prefix pre. -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file x\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('pre.x', 'abc\n')
        self.assertFileEqual('x', 'xyz\n')


    def test_backup_suffix_only(self):
        ''' test applying a patch with backup suffix '''
        patch = '''
--- x	2022-07-27 21:07:04.624795529 +1200
+++ y	2022-07-27 21:07:08.304813552 +1200
@@ -1 +1 @@
-abc
+xyz
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = 'abc\n'
        with open('x', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch --backup --suffix .post -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file x\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('x.post', 'abc\n')
        self.assertFileEqual('x', 'xyz\n')


    def test_backup_prefix_and_suffix(self):
        ''' test applying a patch with backup prefix and suffix '''
        patch = '''
--- x	2022-07-27 21:07:04.624795529 +1200
+++ y	2022-07-27 21:07:08.304813552 +1200
@@ -1 +1 @@
-abc
+xyz
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = 'abc\n'
        with open('x', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch --backup --prefix pre. --suffix .post -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file x\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('pre.x.post', 'abc\n')
        self.assertFileEqual('x', 'xyz\n')


    def test_reverse_still_applies_to_first_file(self):
        ''' test that reversing a file will still apply to first the reverse file '''
        patch = '''
--- a/x.cpp	2022-06-10 19:28:11.018017172 +1200
+++ b/y.cpp	2022-06-10 19:28:21.841903003 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+    return 1;
 }
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 1;
}
'''
        with open('x.cpp', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)
        with open('y.cpp', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        # GNU patch does not seem to also reverse the old and new file
        # names and will still apply the patch to the old file first.
        ret = run_patch('patch -R -i diff.patch')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file x.cpp\n')
        self.assertFileEqual('y.cpp', to_patch)
        self.assertFileEqual('x.cpp', '''int main()
{
}
''')

    def test_read_patch_from_stdin(self):
        ''' test reading a patch from stdin works as intended '''
        patch = '''--- a
+++ b
@@ -1,5 +1,4 @@
 1
 2
-3
-4
 
+4
'''
        to_patch = '''1
2
3
4

'''
        for extra_arg in [' -i -', '']:
            with open('a', 'w', encoding='utf8') as to_patch_file:
                to_patch_file.write(to_patch)
            ret = run_patch('patch' + extra_arg, input=patch)
            self.assertEqual(ret.stderr, '')
            self.assertEqual(ret.stdout, 'patching file a\n')
            self.assertEqual(ret.returncode, 0)
            self.assertFileEqual('a', '''1
2

4
''')


    def test_write_output_to_some_file(self):
        ''' test overriding the file path to patch '''
        patch = '''
--- a	2022-08-21 12:01:05.302352795 +1200
+++ b	2022-08-21 12:01:08.874339091 +1200
@@ -1,3 +1,3 @@
 a
-b
+d
 c
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = 'a\nb\nc\n'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch -osome-file')
        self.assertEqual(ret.stdout, 'patching file some-file (read from a)\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFileEqual('a', 'a\nb\nc\n')
        self.assertFileEqual('some-file', 'a\nd\nc\n')


    def test_reverse_option_when_reversed(self):
        ''' test that we still check for reversed when reverse option specified '''
        patch = '''
--- main.cpp	2022-06-11 16:34:12.304745651 +1200
+++ main2.cpp	2022-06-11 16:34:04.240768394 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-	return 0;
 }
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
	return 0;
}
'''
        with open('main.cpp', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -RN -i diff.patch')
        self.assertEqual(ret.stderr, '')
        # perhaps we should have a better error here?
        self.assertEqual(ret.stdout, '''patching file main.cpp
Unreversed patch detected!  Skipping patch.
1 out of 1 hunk ignored -- saving rejects to file main.cpp.rej
''')
        self.assertFalse(os.path.exists('main.cpp.orig'))

        self.assertFileEqual('main.cpp.rej', '''--- main2.cpp	2022-06-11 16:34:04.240768394 +1200
+++ main.cpp	2022-06-11 16:34:12.304745651 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
''')

        self.assertEqual(ret.returncode, 1)

    def test_both_patch_and_input_as_crlf_output_keep(self):
        ''' test handling when input is CRLF '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()\r
 {\r
+	return 0;\r
 }\r
'''
        with open('diff.patch', 'wb') as patch_file:
            patch_file.write(patch.encode('utf-8'))

        to_patch = '''int main()\r
{\r
}\r
'''
        with open('to_patch', 'wb') as to_patch_file:
            to_patch_file.write(to_patch.encode('utf-8'))

        ret = run_patch('patch -i diff.patch --newline-output preserve')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileBinaryEqual('to_patch',  '''int main()\r
{\r
	return 0;\r
}\r
'''.encode('utf-8'))

    def test_mix_patch_and_input_as_crlf_with_preserve(self):
        ''' test handling when input is CRLF and newlines preserved '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()\r
 {\r
+	return 0;\r
 }\r
'''
        with open('diff.patch', 'wb') as patch_file:
            patch_file.write(patch.encode('utf-8'))

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'wb') as to_patch_file:
            to_patch_file.write(to_patch.encode('utf-8'))

        ret = run_patch('patch -i diff.patch --newline-output preserve')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, '''patching file to_patch
Hunk #1 succeeded at 1 with fuzz 2.
''')
        self.assertEqual(ret.stderr, '')
        self.assertFileBinaryEqual('to_patch',  '''int main()
{
	return 0;\r
}
'''.encode('utf-8'))

    def test_mix_patch_and_input_as_crlf_newlines_crlf(self):
        ''' test handling when input is CRLF and output is CRLF '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()\r
 {\r
+	return 0;\r
 }\r
'''
        with open('diff.patch', 'wb') as patch_file:
            patch_file.write(patch.encode('utf-8'))

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'wb') as to_patch_file:
            to_patch_file.write(to_patch.encode('utf-8'))

        ret = run_patch('patch -l -i diff.patch --newline-output crlf')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileBinaryEqual('to_patch',  '''int main()\r
{\r
	return 0;\r
}\r
'''.encode('utf-8'))

    def test_mix_patch_and_input_as_crlf_newlines_native(self):
        ''' test that mixed newline output is written correctly as native '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()\r
 {
+	return 0;\r
 }\r
'''
        with open('diff.patch', 'wb') as patch_file:
            patch_file.write(patch.encode('utf-8'))

        to_patch = '''int main()
{\r
}
'''
        with open('to_patch', 'wb') as to_patch_file:
            to_patch_file.write(to_patch.encode('utf-8'))

        ret = run_patch('patch -l -i diff.patch --newline-output native')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('to_patch',  '''int main()
{
	return 0;
}
''')

    def test_mix_patch_and_input_as_crlf_newlines_lf(self):
        ''' test handling when input is CRLF writing as LF '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()\r
 {\r
+	return 0;\r
 }\r
'''
        with open('diff.patch', 'wb') as patch_file:
            patch_file.write(patch.encode('utf-8'))

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'wb') as to_patch_file:
            to_patch_file.write(to_patch.encode('utf-8'))

        ret = run_patch('patch -l -i diff.patch --newline-output crlf')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileBinaryEqual('to_patch',  '''int main()\r
{\r
	return 0;\r
}\r
'''.encode('utf-8'))


    def test_mix_patch_and_input_as_crlf_ignore_whitespace(self):
        ''' test handling when input is CRLF, but ignoring whitespace '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()\r
 {\r
+	return 0;\r
 }\r
'''
        with open('diff.patch', 'wb') as patch_file:
            patch_file.write(patch.encode('utf-8'))

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'wb') as to_patch_file:
            to_patch_file.write(to_patch.encode('utf-8'))

        ret = run_patch('patch --ignore-whitespace -i diff.patch --newline-output preserve')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileBinaryEqual('to_patch',  '''int main()
{
	return 0;\r
}
'''.encode('utf-8'))

    def test_patch_rename_no_change(self):
        ''' test that patch correctly applies a rename '''
        patch = '''diff --git a/orig_file b/another_new
similarity index 100%
rename from orig_file
rename to another_new
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        with open('orig_file', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write('a\nb\nc\nd\n')

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file another_new (renamed from orig_file)\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('another_new', 'a\nb\nc\nd\n')
        self.assertFalse(os.path.exists('orig_file'))

    def test_patch_reverse_rename_no_change(self):
        ''' test that patch with reverse option correctly applies a rename '''
        patch = '''diff --git a/x b/y
similarity index 100%
rename from x
rename to y
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = 'a\nb\nc\nd\n'
        with open('y', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch -R')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file x (renamed from y)\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('x', to_patch)
        self.assertFalse(os.path.exists('y'))

    def test_patch_rename_with_change(self):
        ''' test that patch correctly applies a rename with hunk content '''
        patch = '''diff --git a/file b/test
similarity index 87%
rename from thing
rename to test
index 71ac1b5..fc3102f 100644
--- a/thing
+++ b/test
@@ -2,7 +2,6 @@ a
 b
 c
 d
-e
 f
 g
 h
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        with open('thing', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write('''a
b
c
d
e
f
g
h
''')

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file test (renamed from thing)\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('test', '''a
b
c
d
f
g
h
''')
        self.assertFalse(os.path.exists('thing'))

    def test_patch_copy_no_change(self):
        ''' test that patch correctly applies a copy with no hunk content '''
        patch = '''diff --git a/x b/y
similarity index 100%
copy from x
copy to y
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 0;
}
'''
        with open('x', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.stdout, 'patching file y (copied from x)\n')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('x', to_patch)
        self.assertFileEqual('y', to_patch)


    def test_patch_copy_with_change(self):
        ''' test that patch correctly applies a copy with hunk content '''
        patch = '''diff --git a/x b/y
similarity index 51%
copy from x
copy to y
index 905869d..2227c3a 100644
--- a/x
+++ b/y
@@ -1,4 +1,4 @@
 int main()
 {
-    return 0;
+    return 1;
 }
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 0;
}
'''
        with open('x', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file y (copied from x)\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('x', to_patch)
        self.assertFileEqual('y', '''int main()
{
    return 1;
}
''')


    def test_patch_rename_already_exists_no_content(self):
        ''' patch is not applied for rename only '''
        patch = '''
From 89629b257f091dd0ff78509ca0ad626089defaa7 Mon Sep 17 00:00:00 2001
From: Shannon Booth <shannon.ml.booth@gmail.com>
Date: Tue, 5 Jul 2022 18:53:32 +1200
Subject: [PATCH] move a to b

---
 a => b | 0
 1 file changed, 0 insertions(+), 0 deletions(-)
 rename a => b (100%)

diff --git a/a b/b
similarity index 100%
rename from a
rename to b
--
2.25.1

'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        existing_b = '1\n2\n3\n'
        with open('b', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(existing_b)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file b (already renamed from a)\n')
        self.assertEqual(ret.stderr, '')
        self.assertFalse(os.path.exists('a'))
        self.assertFileEqual('b', existing_b)


    def test_patch_rename_already_exists_with_content(self):
        ''' test rename patch where rename is made, but content of patch is still applied '''
        patch = '''
diff --git a/b b/a
similarity index 66%
rename from b
rename to a
index de98044..0f673f8 100644
--- a/b
+++ b/a
@@ -1,3 +1,3 @@
 a
-b
+2
 c
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        existing_a = 'a\nb\nc\n'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(existing_a)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.stdout, 'patching file a (already renamed from b)\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFalse(os.path.exists('b'))
        self.assertFileEqual('a', 'a\n2\nc\n')


    def test_patch_revsered_rename_already_exists_with_content(self):
        ''' test reversed rename patch where rename is made, but content of patch is still applied '''
        patch = '''
diff --git a/a b/b
similarity index 66%
rename from a
rename to b
index de98044..0f673f8 100644
--- a/a
+++ b/b
@@ -1,3 +1,3 @@
 a
-b
+2
 c
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        existing_a = 'a\n2\nc\n'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(existing_a)

        ret = run_patch('patch -i diff.patch --reverse')
        self.assertEqual(ret.stdout, 'patching file a (already renamed from b)\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFalse(os.path.exists('b'))
        self.assertFileEqual('a', 'a\nb\nc\n')


    def test_error_when_invalid_patch_given(self):
        ''' test proper error is output when an invalid patch file is supplied '''
        patch = '''
        some
        garbage!
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')
        self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** Only garbage was found in the patch input.\n')

    def test_error_when_non_existent_patch_file_given(self):
        ''' test that when patch is given a non-existent patch file is given an error is raised '''
        self.assertFalse(os.path.exists('diff.patch')) # sanity check
        ret = run_patch('patch -b -i diff.patch')

        # windows returns in different case, we don't care.
        if os.name == 'nt':
            self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** Can\'t open patch file diff.patch : no such file or directory\n')
        else:
            self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** Can\'t open patch file diff.patch : No such file or directory\n')

        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')

    def test_add_executable_bit(self):
        ''' test that a patch changing mode applies '''

        patch = '''
diff --git a/file b/file
old mode 100644
new mode 100755
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n2\n3\n'
        with open('file', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')

        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file file\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('file', to_patch)
        self.assertTrue(os.access('file', os.X_OK))


    def test_read_only_file_default(self):
        ''' test patching a file which is read only still applies with a warning by default '''

        patch = '''
--- a/a	2022-09-18 09:59:59.586887443 +1200
+++ b/a	2022-09-18 10:00:04.410912780 +1200
@@ -1,4 +1,3 @@
 1
 2
-3
 4
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n2\n3\n4\n'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        # Make file read-only. Keep track of old mode.
        mode = os.stat('a').st_mode
        ro_mask = 0o777 ^ (stat.S_IWRITE | stat.S_IWGRP | stat.S_IWOTH)
        os.chmod('a', mode & ro_mask)
        old_mode = os.stat('a').st_mode

        ret = run_patch('patch -i diff.patch')

        self.assertEqual(ret.stdout, 'File a is read-only; trying to patch anyway\npatching file a\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFileEqual('a', '1\n2\n4\n')

        # Ensure file permissions restored to original
        new_mode = os.stat('a').st_mode
        self.assertEqual(new_mode, old_mode)


    def test_read_only_file_ignore(self):
        ''' test patching a file which is read only still applies with no warning if ignore flag set '''

        patch = '''
--- a/a	2022-09-18 09:59:59.586887443 +1200
+++ b/a	2022-09-18 10:00:04.410912780 +1200
@@ -1,4 +1,3 @@
 1
 2
-3
 4
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n2\n3\n4\n'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        # Make file read-only. Keep track of old mode.
        mode = os.stat('a').st_mode
        ro_mask = 0o777 ^ (stat.S_IWRITE | stat.S_IWGRP | stat.S_IWOTH)
        os.chmod('a', mode & ro_mask)
        old_mode = os.stat('a').st_mode

        ret = run_patch('patch -i diff.patch --read-only ignore')

        self.assertEqual(ret.stdout, 'patching file a\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFileEqual('a', '1\n2\n4\n')

        # Ensure file permissions restored to original
        new_mode = os.stat('a').st_mode
        self.assertEqual(new_mode, old_mode)


    def test_read_only_file_fail(self):
        ''' test patching a file which is read only fails if fail option is set '''

        patch = '''\
--- a	2022-09-18 09:59:59.586887443 +1200
+++ a	2022-09-18 10:00:04.410912780 +1200
@@ -1,4 +1,3 @@
 1
 2
-3
 4
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n2\n3\n4\n'
        with open('a', 'w', encoding='utf8') as to_patch_file:
            to_patch_file.write(to_patch)

        # Make file read-only. Keep track of old mode.
        mode = os.stat('a').st_mode
        ro_mask = 0o777 ^ (stat.S_IWRITE | stat.S_IWGRP | stat.S_IWOTH)
        os.chmod('a', mode & ro_mask)
        old_mode = os.stat('a').st_mode

        ret = run_patch('patch -i diff.patch --read-only fail')

        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.stdout, '''File a is read-only; refusing to patch
1 out of 1 hunk ignored -- saving rejects to file a.rej
''')
        self.assertEqual(ret.returncode, 1)
        self.assertFileEqual('a', to_patch)
        self.assertFileEqual('a.rej', patch)

        # Ensure file permissions restored to original
        new_mode = os.stat('a').st_mode
        self.assertEqual(new_mode, old_mode)


    def test_error_on_chdir_to_bad_directory(self):
        ''' test that an appropriate error is shown on chdir to a non existent directory '''
        patch = '''
--- main.cpp	2022-06-11 16:34:12.304745651 +1200
+++ main2.cpp	2022-06-11 16:34:04.240768394 +1200
@@ -1,4 +1,3 @@
 int main()
 {
-	return 0;
 }
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch -d bad_directory')

        # windows returns in different case, we don't care.
        if os.name == 'nt':
            self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** Unable to change to directory bad_directory: no such file or directory\n')
        else:
            self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** Unable to change to directory bad_directory: No such file or directory\n')

        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')


    @unittest.expectedFailure
    def test_git_swap_files(self):
        ''' test that git patch swapping files is correctly applied '''
        patch = '''\
diff --git a/a b/b
rename from a
rename to b
diff --git a/b b/a
rename from b
rename to a
'''
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        a = '1\n2\n3\n'
        with open('a', 'w', encoding='utf8') as a_file:
            a_file.write(a)

        b = 'a\nb\nc\n'
        with open('b', 'w', encoding='utf8') as b_file:
            b_file.write(b)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.stdout, '''\
patching file b (renamed from a)
patching file a (renamed from b)
''')
        self.assertFileEqual('a', b)
        self.assertFileEqual('b', a)


    def test_unknown_commandline(self):
        ''' test error on unknown command line argument '''
        ret = run_patch('patch --garbage')
        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stderr, f'''{PATCH_PROGRAM}: **** unknown commandline argument --garbage
Try '{PATCH_PROGRAM} --help' for more information.
''')
        self.assertEqual(ret.stdout, '')


    def test_version_message(self):
        ''' test the version message is as expected '''
        ret = run_patch('patch --version')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.stdout, '''patch 0.0.1
Copyright (C) 2022 Shannon Booth
''')

    def test_help_message(self):
        ''' test the help message is as expected '''
        ret = run_patch('patch --help')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stderr, '')
        # Don't really care about the entire thing, just that we're giving it at all.
        self.assertTrue(ret.stdout.startswith('''patch - (C) 2022 Shannon Booth

patch reads a patch file containing a difference (diff) and applies it to files.
'''))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument('patch_program', nargs='?', default='sb_patch', help='patch program to use for testing')
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args()
    PATCH_PROGRAM = args.patch_program
    sys.argv[1:] = args.unittest_args
    unittest.main()
