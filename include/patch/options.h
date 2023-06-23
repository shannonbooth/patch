// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <patch/cmdline.h>
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

    enum class OptionalBool {
        Unset,
        Yes,
        No,
    };

    enum class QuotingStyle {
        Unset,
        Literal,
        Shell,
        ShellAlways,
        C,
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
    bool posix { false };
    OptionalBool backup_if_mismatch { OptionalBool::Unset };
    OptionalBool remove_empty_files { OptionalBool::Unset };
    NewlineOutput newline_output { NewlineOutput::Native };
    RejectFormat reject_format { RejectFormat::Default };
    ReadOnlyHandling read_only_handling { ReadOnlyHandling::Warn };
    QuotingStyle quoting_style { QuotingStyle::Unset };
    std::string backup_suffix;
    std::string backup_prefix;
};

class OptionHandler : public CmdLineParser::Handler {
public:
    OptionHandler();

    void process_option(int short_name, const std::string& option = "") override;

    const Options& options() const { return m_options; }

    void apply_defaults()
    {
        apply_environment_defaults();
        apply_posix_defaults();
    }

private:
    void process_operand(const std::string& value);
    void handle_newline_strategy(const std::string& strategy);
    void handle_read_only(const std::string& handling);
    void handle_reject_format(const std::string& format);
    void handle_quoting_style(const std::string& style, const Options::QuotingStyle* default_quote_style = nullptr);

    void apply_posix_defaults();
    void apply_environment_defaults();

    static int stoi(const std::string& str, const std::string& description);

    int m_positional_arguments_found { 0 };
    Options m_options;
};

void show_usage(std::ostream& out);
void show_version(std::ostream& out);

} // namespace Patch
