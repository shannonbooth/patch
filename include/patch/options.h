// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <string>

namespace Patch {

struct Options {
    enum class NewlineOutput {
        Native,
        LF,
        CRLF,
        Keep,
    };

    enum class RejectFormat {
        Context,
        Unified,
        Default,
    };

    enum class ReadOnlyHandling {
        Warn,
        Ignore,
        Fail,
    };

    // posix defined options
    bool save_backup { false };
    bool interpret_as_context { false };
    std::string patch_directory_path;
    std::string define_macro;
    bool interpret_as_ed { false };
    std::string patch_file_path;
    bool ignore_whitespace { false };
    bool interpret_as_normal { false };
    bool ignore_reversed { false };
    std::string out_file_path;
    int strip_size { -1 };
    int max_fuzz { 2 };
    bool reverse_patch { false };
    std::string file_to_patch;
    std::string reject_file_path;

    // non posix defined
    bool force { false };
    bool batch { false };
    bool show_help { false };
    bool show_version { false };
    bool interpret_as_unified { false };
    bool verbose { false };
    bool dry_run { false };
    NewlineOutput newline_output { NewlineOutput::Native };
    RejectFormat reject_format { RejectFormat::Default };
    ReadOnlyHandling read_only_handling { ReadOnlyHandling::Warn };
    std::string backup_suffix;
    std::string backup_prefix;
};

} // namespace Patch
