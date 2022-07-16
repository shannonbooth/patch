#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

import argparse
import os
import subprocess
import sys
import tempfile
import unittest

PATCH_PROGRAM = 'sb_patch'

def run(cmd, input=None):
    ''' run patch with some options '''
    try:
        return subprocess.run(cmd, text=True, capture_output=True, timeout=1, input=input)
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
        with open(path, 'r') as file:
            file_content = file.read()
        self.assertEqual(file_content, content)

    def assertFileBinaryEqual(self, path, content):
        ''' assert that the given path has the expected file content '''
        with open(path, 'rb') as file:
            file_content = file.read()
        self.assertEqual(file_content, content)


    def test_basic_patch(self):
        ''' the most basic of patch tests - hopefully should never fail! '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('to_patch', '''int main()
{
	return 0;
}
''')

    def test_basic_context_patch(self):
        ''' the most basic of context patch tests - hopefully should never fail! '''
        patch = '''
*** a	2022-06-19 20:26:52.280538615 +1200
--- b	2022-06-19 20:26:59.968648316 +1200
***************
*** 1,3 ****
--- 1,4 ----
  int main()
  {
+     return 0;
  }
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
'''
        with open('a', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.stdout, 'patching file a\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFileEqual('a', '''int main()
{
    return 0;
}
''')

    def test_less_basic_patch(self):
        ''' test a slightly more complex patch '''
        patch = '''
--- x   2022-10-10 20:45:19.577907405 +1300
+++ x   2022-10-10 13:03:25.792099411 +1300
@@ -1,9 +1,8 @@
 1
 2
-3
+3a
 4
 5
 6
-7
 8
 9
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''1
2
3
4
5
6
7
8
9
'''
        with open('x', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file x\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('x', '''1
2
3a
4
5
6
8
9
''')

    def test_basic_patch_with_dryrun(self):
        ''' basic patch applied with dry run '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch --dry-run')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'checking file to_patch\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('to_patch', to_patch)
        self.assertFalse(os.path.exists('to_patch.orig'))


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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file -\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('-', 'some file\n')


    def test_basic_patch_with_dryrun_to_stdout(self):
        ''' basic patch applied with dry run to stdout '''
        patch = '''
--- to_patch	2022-06-19 16:56:12.974516527 +1200
+++ to_patch	2022-06-19 16:56:24.666877199 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 0;
 }
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
'''
        with open('to_patch', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch --dry-run -o -')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stderr, 'checking file - (read from to_patch)\n')
        self.assertEqual(ret.stdout, '''\
int main()
{
	return 0;
}
''')
        self.assertFileEqual('to_patch', to_patch)
        self.assertFalse(os.path.exists('to_patch.orig'))


    def test_failed_patch_dry_run(self):
        ''' test a bad patch with dry run does not write rejects '''
        patch = '''
--- 1	2022-06-26 12:22:22.161398905 +1200
+++ 2	2022-06-26 12:22:44.105278030 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''a
b
c
'''
        with open('1', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch --dry-run --force')
        self.assertEqual(ret.returncode, 1)
        self.assertEqual(ret.stdout, '''checking file 1
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED
''')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('1', to_patch)
        self.assertFalse(os.path.exists('1.rej'))
        self.assertFalse(os.path.exists('1.orig'))

    def test_ed_patches_not_supported(self):
        ''' test that when we try patch an ed patch file, we fail with an appropriate error '''
        patch = '3d\n'
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch --ed')
        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')
        self.assertEqual(ret.stderr, 'patch: **** ed format patches are not supported by this version of patch\n')

    def test_add_file(self):
        ''' test that a patch which adds a file actually ends up adding said file '''
        patch = '''
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ add	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        self.assertFalse(os.path.exists('add')) # sanity check
        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file add\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('add', '''int main()
{
}
''')

    def test_add_file_using_basename(self):
        ''' test a patch with default strip '''
        patch = '''
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ a/b/c/d/e	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        self.assertFalse(os.path.exists('e')) # sanity check
        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file e\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('e', '''int main()
{
}
''')

    def test_add_file_missing_folders(self):
        ''' test a patch adding a file with containing folders missing '''
        patch = '''
--- /dev/null	2022-05-27 08:55:08.788091961 +1200
+++ a/b/c/d/e	2022-05-28 17:03:10.882372978 +1200
@@ -0,0 +1,3 @@
+int main()
+{
+}
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        self.assertFalse(os.path.exists('e')) # sanity check
        ret = run_patch('patch -i diff.patch -p0')
        self.assertEqual(ret.stdout, 'patching file a/b/c/d/e\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFileEqual('a/b/c/d/e', '''int main()
{
}
''')

    def test_override_reject_file(self):
        ''' applying a rejected patch with overridden reject file writes to correct location '''
        patch = '''\
--- a	2022-07-08 10:34:03.860546761 +1200
+++ b	2022-07-08 10:34:20.096714313 +1200
@@ -1,3 +1,3 @@
-a
-b
-c
+1
+2
+3
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n2\n3\n'
        with open('a', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch --force -r override.rej')
        self.assertEqual(ret.stdout, '''patching file a
Hunk #1 FAILED at 1.
1 out of 1 hunk FAILED -- saving rejects to file override.rej
''')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 1)
        self.assertFileEqual('override.rej', patch)


    def test_remove_file_in_folders(self):
        ''' test a patch removing a file removes all containing empty folders '''
        patch = '''
--- a/b/c/d/e	2022-07-03 14:32:47.081054897 +1200
+++ /dev/null	2022-06-30 20:33:13.470250591 +1200
@@ -1 +0,0 @@
-1
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        path = os.path.join('a', 'b', 'c', 'd')
        os.makedirs(path)
        to_patch = '1\n'
        with open(os.path.join(path, 'e'), 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        with open(os.path.join('a', '1'), 'w') as another_file:
            another_file.write('some stuff!\n')

        # Test that the file and empty directory are removed, but the
        # non empty directory is not removed.
        ret = run_patch('patch -i diff.patch -p0')
        self.assertEqual(ret.stdout, 'patching file a/b/c/d/e\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFalse(os.path.exists(os.path.join('a', 'b', 'c', 'd')))
        self.assertFalse(os.path.exists(os.path.join('a', 'b', 'c')))
        self.assertFalse(os.path.exists(os.path.join('a', 'b')))
        self.assertFileEqual(os.path.join('a', '1'), 'some stuff!\n')

    def test_remove_file_successfully(self):
        ''' test removal patch that has some garbage not found in patch file does not clean file '''
        patch = '''
--- to_remove	2022-06-06 14:40:51.547454892 +1200
+++ /dev/null	2022-06-01 16:08:26.275921370 +1200
@@ -1,3 +0,0 @@
-int main()
-{
-}
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
'''
        with open('to_remove', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        self.assertTrue(os.path.exists('to_remove'))
        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.stdout, 'patching file to_remove\n')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertFalse(os.path.exists('to_remove'))


    def test_remove_file_that_has_trailing_garbage(self):
        ''' test removal patch that has some garbage not found in patch file does not clean file '''
        patch = '''
--- remove	2022-06-06 14:40:51.547454892 +1200
+++ /dev/null	2022-06-01 16:08:26.275921370 +1200
@@ -1,3 +0,0 @@
-int main()
-{
-}
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
// some trailing garbage
'''
        with open('remove', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.stdout, '''patching file remove
Not deleting file remove as content differs from patch
''')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 1)
        self.assertFileEqual('remove', '// some trailing garbage\n')


    def test_backup_when_patch_is_adding_file(self):
        ''' test that when a patch creating a file has the backup option, a empty backup is made '''
        patch = '''
--- /dev/null	2022-06-08 13:09:24.217949672 +1200
+++ f	2022-06-08 20:44:55.455275623 +1200
@@ -0,0 +1 @@
+a line
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        self.assertFalse(os.path.exists('f')) # sanity check
        self.assertFalse(os.path.exists('f.orig')) # sanity check
        ret = run_patch('patch -b -i diff.patch')
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file f\n')
        self.assertFileEqual('f', 'a line\n')
        self.assertFalse(os.path.exists('f.orig'))

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

        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        existing = 'some file content!\n'
        with open('a.orig', 'w') as existing_file:
            existing_file.write(existing)

        to_patch = '''int main()
{
}
'''
        with open('a', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 0;
}
'''
        with open('main.cpp', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 1;
}
'''
        with open('x.cpp', 'w') as to_patch_file:
            to_patch_file.write(to_patch)
        with open('y.cpp', 'w') as to_patch_file:
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
            with open('a', 'w') as to_patch_file:
                to_patch_file.write(to_patch)
            ret = run_patch('patch' + extra_arg, input=patch)
            self.assertEqual(ret.stderr, '')
            self.assertEqual(ret.stdout, 'patching file a\n')
            self.assertEqual(ret.returncode, 0)
            self.assertFileEqual('a', '''1
2

4
''')

    def test_write_output_to_stdout(self):
        ''' test that setting -o to - writes the patched file to stdout '''
        patch = '''
--- a/x.cpp	2022-06-10 19:28:11.018017172 +1200
+++ b/y.cpp	2022-06-10 19:28:21.841903003 +1200
@@ -1,3 +1,4 @@
 int main()
 {
+	return 1;
 }
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
}
'''
        with open('x.cpp', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch -o -')
        self.assertEqual(ret.stderr, 'patching file - (read from x.cpp)\n')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, '''\
int main()
{
	return 1;
}
''')

    def test_write_empty_output_to_stdout(self):
        ''' test that setting -o to - writes the patched file to stdout for an empty file '''
        patch = '''
--- a	2022-07-02 15:23:07.929349813 +1200
+++ /dev/null	2022-06-30 20:33:13.470250591 +1200
@@ -1 +0,0 @@
-1
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n'
        with open('a', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch -o -')
        self.assertEqual(ret.stderr, 'patching file - (read from a)\n')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, '')

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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
	return 0;
}
'''
        with open('main.cpp', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -RN -i diff.patch')
        self.assertEqual(ret.stderr, '')
        # perhaps we should have a better error here?
        self.assertEqual(ret.stdout, '''patching file main.cpp
Unreversed patch detected! Skipping patch.
Hunk #1 skipped at 1 with fuzz 2.
1 out of 1 hunk ignored -- saving rejects to file main.cpp.rej
''')
        self.assertFileEqual('main.cpp.orig', '''int main()
{
	return 0;
}
''')

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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        with open('orig_file', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = 'a\nb\nc\nd\n'
        with open('y', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        with open('thing', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 0;
}
'''
        with open('x', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''int main()
{
    return 0;
}
'''
        with open('x', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        existing_b = '1\n2\n3\n'
        with open('b', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        existing_a = 'a\nb\nc\n'
        with open('a', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        existing_a = 'a\n2\nc\n'
        with open('a', 'w') as to_patch_file:
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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch')
        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')
        self.assertEqual(ret.stderr, 'patch: **** Unable to determine patch format\n')

    def test_error_when_non_existent_patch_file_given(self):
        ''' test that when patch is given a non-existent patch file is given an error is raised '''
        self.assertFalse(os.path.exists('diff.patch')) # sanity check
        ret = run_patch('patch -b -i diff.patch')

        # windows returns in different case, we don't care.
        if os.name == 'nt':
            self.assertEqual(ret.stderr, 'patch: **** Unable to open patch file diff.patch: no such file or directory\n')
        else:
            self.assertEqual(ret.stderr, 'patch: **** Unable to open patch file diff.patch: No such file or directory\n')

        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')

    def test_chdir_good_case(self):
        ''' test using chdir allows relative path to work '''

        os.mkdir('folder')
        patch = '''
--- 1	2022-06-26 11:17:58.948060133 +1200
+++ 2	2022-06-26 11:18:03.500001858 +1200
@@ -1,3 +1,2 @@
 1
-2
 3
'''
        with open(os.path.join('folder', 'diff.patch'), 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '''1
2
3
'''
        with open(os.path.join('folder', '1'), 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch -d folder')

        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file 1\n')
        self.assertEqual(ret.stderr, '')

    def test_add_executable_bit(self):
        ''' test that a patch changing mode applies '''

        patch = '''
diff --git a/file b/file
old mode 100644
new mode 100755
'''
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        to_patch = '1\n2\n3\n'
        with open('file', 'w') as to_patch_file:
            to_patch_file.write(to_patch)

        ret = run_patch('patch -i diff.patch')

        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stdout, 'patching file file\n')
        self.assertEqual(ret.stderr, '')
        self.assertFileEqual('file', to_patch)
        self.assertTrue(os.access('file', os.X_OK))

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
        with open('diff.patch', 'w') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch -d bad_directory')

        # windows returns in different case, we don't care.
        if os.name == 'nt':
            self.assertEqual(ret.stderr, 'patch: **** Unable to change to directory bad_directory: no such file or directory\n')
        else:
            self.assertEqual(ret.stderr, 'patch: **** Unable to change to directory bad_directory: No such file or directory\n')

        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')


    def test_version_message(self):
        ''' test the version message is as expected '''
        ret = run_patch('patch --version')
        self.assertEqual(ret.returncode, 0)
        self.assertEqual(ret.stderr, '')
        self.assertEqual(ret.stdout, '''patch 0.0.1
Copyright (C) 2022 Shannon Booth
''')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument('patch_program', nargs='?', default='sb_patch', help='patch program to use for testing')
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args()
    PATCH_PROGRAM = args.patch_program
    sys.argv[1:] = args.unittest_args
    unittest.main()
