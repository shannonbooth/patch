# Patch

![Build](https://github.com/shannonbooth/patch/actions/workflows/cmake.yml/badge.svg)
[![codecov](https://codecov.io/gh/shannonbooth/patch/branch/main/graph/badge.svg?token=MWQ2WETJVB)](https://codecov.io/gh/shannonbooth/patch)

Patch is an implementation of the Unix tool [patch](https://pubs.opengroup.org/onlinepubs/7908799/xcu/patch.html).

## Goals

* Cross-platform
* Compatible with existing patch implementations
* Compatible with POSIX

## How to use

### Generate a patch

Generate a patch file using a tool such as `git` or the `diff` utility:

```sh
# Generate a file 'file-orig.txt' with contents:
# > a
# > b
# > c
echo 'a\nb\nc\n' > file-orig.txt

# Generate a file 'file-new.txt' with contents:
# > a
# > c
echo 'a\nc\n' > file-new.txt

# Generate a unified patch to a file named 'diff.patch' showing 'file-orig.txt'
# has the line 'b' removed when compared to 'file-new.txt'.
diff -u file-orig.txt file-new.txt > diff.patch
```

Supported patch formats are "normal", "context" and "unified".

### Applying the patch

Run `patch` using the generated patch file:

```sh
patch --input diff.patch
```

`file-orig.txt` should have been patched, removing the line `b` to match the
contents of `file-new.txt`.

## Cross platform behavior

Patch is written to support Windows, Linux, BSD's and other Unix like systems.

### Newline handling

A line that has a different line ending to another (CRLF vs LF) will be considered
to not match. The flag `--ignore-whitespace` can be used so that lines with different
line endings but the same content will be considered equal.

The flag `--newline-output` can be used to change how newlines will be output
in patched files.

* `native`: will write the entire contents of the file in native line endings
            (CRLF `\r\n` on Windows, LF `\n` on Unix).
* `lf`: will write the file with LF `\n` line endings.
* `crlf`: will write the file with CRLF `\r\n` line endings.
* `preserve`: will attempt to use same line endings as lines in the file being patched.

By default, patch will select the `preserve` option.

### Path handling

Patch will parse and apply patches using the host systems path separation
semantics. For example, `\` characters are treated as path separators on
Windows, but as part of the file name on Unix-like systems.

### Symlinks

Symlinks in extended-format patches are supported on Unix like systems. Symlinks
are not supported on Windows, and result in the patch failing.

## Build & test

Patch is built with `cmake`. It can be built with the following commands:

```sh
cmake -S . -B build
cmake --build build
```

Tests can be enabled with the `-DBUILD_TESTING=On`, with coverage information
reported with the `coverage` target if `-DPATCH_ENABLE_COVERAGE=On` is enabled.
