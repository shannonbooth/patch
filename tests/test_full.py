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

    def test_ed_patches_not_supported(self):
        ''' test that when we try patch an ed patch file, we fail with an appropriate error '''
        patch = '3d\n'
        with open('diff.patch', 'w', encoding='utf8') as patch_file:
            patch_file.write(patch)

        ret = run_patch('patch -i diff.patch --ed')
        self.assertEqual(ret.returncode, 2)
        self.assertEqual(ret.stdout, '')
        self.assertEqual(ret.stderr, f'{PATCH_PROGRAM}: **** ed format patches are not supported by this version of patch\n')


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
