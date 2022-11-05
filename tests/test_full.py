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

if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument('patch_program', nargs='?', default='sb_patch', help='patch program to use for testing')
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args()
    PATCH_PROGRAM = args.patch_program
    sys.argv[1:] = args.unittest_args
    unittest.main()
