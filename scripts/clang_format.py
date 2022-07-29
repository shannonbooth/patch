#!/usr/bin/python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>
'''
clang_format.py

Wrapper script around clang-format intended to be used for reformatting an
entire git repository of code, or for use in CI, for checking that changes meet
the formatting outlined by .clang-format

There is also support for an ignore file which can be used for not formatting
or checking formatting for files that are not owned by this repository, or should
not have formatting done on them.

Color diff support for clang-format is also implemented through these scripts.
'''

import sys
import argparse
from difflib import unified_diff
from subprocess import check_output
import pathspec

RED = '\033[0;31m'
GREEN = '\033[92m'
BLUE = '\033[0;34m'
BOLD = '\033[1m'
ENDC = '\033[0m'

def list_from_lines(lines):
    ''' Returns a list from each line '''
    return [line.rstrip() for line in lines.splitlines()]

def tracked_files(branch='HEAD'):
    ''' Returns a list of all tracked files for a branch '''
    return list_from_lines(run_cmd(['git', 'ls-tree', '-r', branch, '--name-only']))

def run_cmd(cmd):
    ''' Run a shell command and return the output as a string '''
    return check_output(cmd).decode('utf-8')

def color_diff(diff):
    ''' Colors a diff using ANSI color control characters '''
    for line in diff:
        if line.startswith('+'):
            yield GREEN + line + ENDC
        elif line.startswith('-'):
            yield RED + line + ENDC
        elif line.startswith('^'):
            yield BLUE + line + ENDC
        else:
            yield line

def format_file(path_to_file, quiet=False, no_write=False):
    ''' Format a specific file using clang-format '''
    with open(path_to_file, 'r') as original_text:
        original_file = original_text.read()

    cmd = ['clang-format', '--style=file', path_to_file]
    if not no_write:
        cmd += ['-i']

    # We get the formatted file from stdout if it is not being written to.
    formatted_file = run_cmd(cmd)
    if not no_write:
        with open(path_to_file, 'r') as formated_text:
            formatted_file = formated_text.read()

    if original_file == formatted_file:
        return 0

    print('{}NOTE{}: Found diff for {}'.format(BOLD, ENDC, path_to_file))
    if not quiet:
        diff = color_diff(unified_diff(original_file.splitlines(),
                                       formatted_file.splitlines(),
                                       fromfile=path_to_file,
                                       tofile=path_to_file,
                                       lineterm=''))
        for line in diff:
            print(line.rstrip())
    return 1

def format_all(quiet=False, no_write=False):
    ''' Format all files in a git repository using clang-format '''
    extensions_to_format = ('.cpp', '.cc', '.h', '.hpp')
    files_to_format = [file for file in tracked_files() if file.endswith(extensions_to_format)]
    try:
        with open('.clang-format.ignore', 'r') as ignore_file:
            files_to_ignore = ignore_file.readlines()
    except FileNotFoundError:
        files_to_ignore = []

    ignore = pathspec.PathSpec.from_lines(pathspec.patterns.GitWildMatchPattern, files_to_ignore)
    ret = 0
    for i, file in enumerate(files_to_format):
        # Nicely align the count of files that we have formatted
        print('{0:>{align}}/{1:<{align}}: '.format(i + 1, len(files_to_format),
                                                   align=len(str(len(files_to_format)))), end='')
        if ignore.match_file(file):
            print('Skipping  ' + file)
        else:
            print('Formating ' + file)
            ret |= format_file(file, quiet=quiet, no_write=no_write)

    return ret

def main():
    ''' Wraps all of this '''
    parser = argparse.ArgumentParser(description='run clang format over a selection of files')
    parser.add_argument('files', nargs='*', help='path to files to format. if none given, '
                                                 ' defaults to all files in the repository.')
    parser.add_argument('--quiet', action='store_true',
                        help='do not output the diff of formatted files')
    parser.add_argument('--no-write', action='store_true',
                        help='do not write the formatted files to disk')

    args = parser.parse_args()

    if not args.files:
        ret = format_all(quiet=args.quiet, no_write=args.no_write)
        sys.exit(ret)

    ret = 0
    for file_to_format in args.files:
        ret |= format_file(file_to_format, quiet=args.quiet, no_write=False)
    sys.exit(ret)

if __name__ == '__main__':
    main()
