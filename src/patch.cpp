// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <patch/applier.h>
#include <patch/cmdline.h>
#include <patch/file.h>
#include <patch/hunk.h>
#include <patch/options.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <patch/system.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>

namespace Patch {

void print_header_info(File& patch, const PatchHeaderInfo& header_info, std::ostream& out)
{
    patch.seekg(header_info.patch_start);
    out << "Hmm...  Looks like a " << to_string(header_info.format) << " diff to me...\n";

    if (header_info.lines_till_first_hunk > 1) {
        out << "The text leading up to this was:\n"
            << "--------------------------\n";
        size_t my_lines = header_info.lines_till_first_hunk;
        std::string line;
        while (my_lines > 1) {
            --my_lines;
            if (!patch.get_line(line))
                throw std::runtime_error("Failure reading line from patch outputting header info");

            out << '|' << line << '\n';
        }
        out << "--------------------------\n";
    }
}

std::string to_string(Format format)
{
    switch (format) {
    case Format::Context:
        return "context";
    case Format::Ed:
        return "ed";
    case Format::Normal:
        return "normal";
    case Format::Unified:
        return "unified";
    case Format::Unknown:
        return "unknown";
    }

    throw std::invalid_argument("Unknown diff format");
}

bool check_with_user(const std::string& question, std::ostream& out, Default default_response)
{
    char default_char = default_response == Default::True ? 'y' : 'n';
    out << question << " [" << default_char << "] " << std::flush;

    auto buffer = read_tty_until_enter();
    bool is_truthy = buffer.empty() || !(buffer[0] != default_char);
    if (default_response == Default::False)
        is_truthy = !is_truthy;

    return is_truthy;
}

static std::string guess_filepath(const Patch& patch)
{
    // POSIX specifies that after stripping using the '-p' option then the existence of both the old
    // and new files are tested. If both paths exist then patch should not be able to determine
    // any paths from this step.
    //
    // However, it seems from my testing that this behaviour is not followed by GNU patch, even when
    // the --posix argument is specified. In the GNU documentation they state when following posix,
    // they say that the order of 'old', 'new' then 'index' when trying to determine the file name
    // to patch.
    //
    // This means that they do not throw any error when both files exist (which aligns with my testing).
    // For now, this implementation matches the GNU behaviour when the --posix flag is specified. In
    // the future, we may want to make our implementation match whatever the behaviour of GNU patch
    // is for this path determination.
    if (patch.old_file_path != "/dev/null" && filesystem::exists(patch.old_file_path))
        return patch.old_file_path;

    if (patch.new_file_path != "/dev/null" && filesystem::exists(patch.new_file_path))
        return patch.new_file_path;

    if (patch.index_file_path != "/dev/null" && filesystem::exists(patch.index_file_path))
        return patch.index_file_path;

    return {};
}

static std::string prompt_for_filepath(std::ostream& out)
{
    while (true) {
        out << "File to patch: " << std::flush;

        auto buffer = read_tty_until_enter();

        if (!buffer.empty()) {
            errno = 0;
            if (filesystem::is_regular_file(buffer))
                return buffer;
            auto saved_errno = errno;

            out << buffer;
            if (saved_errno != 0)
                out << ": " << std::strerror(saved_errno);
            out << '\n';
        }

        if (check_with_user("Skip this patch", out, Default::True))
            return "";
    }
}

static Format diff_format_from_options(const Options& options)
{
    auto format = Format::Unknown;
    if (options.interpret_as_context)
        format = Format::Context;
    else if (options.interpret_as_normal)
        format = Format::Normal;
    else if (options.interpret_as_unified)
        format = Format::Unified;
    else if (options.interpret_as_ed)
        format = Format::Ed;
    return format;
}

class PatchFile {
public:
    explicit PatchFile(const Options& options)
    {
        if (options.patch_file_path.empty() || options.patch_file_path == "-") {
            m_patch_file = File::create_temporary(stdin);
        } else {
            std::ios::openmode mode = std::ios::in | std::ios::out;
            if (options.newline_output != Options::NewlineOutput::Native)
                mode |= std::ios::binary;

            m_patch_file.open(options.patch_file_path, mode);
            if (!m_patch_file)
                throw std::system_error(errno, std::generic_category(), "Unable to open patch file " + options.patch_file_path);
        }
    }

    File& file() { return m_patch_file; }

private:
    File m_patch_file;
    std::stringstream m_in_memory_buffer;
};

static std::string output_path(const Options& options, const Patch& patch, const std::string& file_to_patch)
{
    if (!options.out_file_path.empty())
        return options.out_file_path;

    if (patch.operation == Operation::Rename || patch.operation == Operation::Copy)
        return options.reverse_patch ? patch.old_file_path : patch.new_file_path;

    return file_to_patch;
}

static std::string backup_name(const Options& options, const std::string& output_file)
{
    if (!options.backup_prefix.empty() && !options.backup_suffix.empty())
        return options.backup_prefix + output_file + options.backup_suffix;

    if (!options.backup_prefix.empty())
        return options.backup_prefix + "orig";

    if (!options.backup_suffix.empty())
        return output_file + options.backup_suffix;

    return options.backup_prefix + output_file + options.backup_suffix + ".orig";
}

int process_patch(const Options& options)
{
    if (options.show_help) {
        show_usage(std::cout);
        return 0;
    }

    if (options.show_version) {
        show_version(std::cout);
        return 0;
    }

    if (!options.patch_directory_path.empty())
        chdir(options.patch_directory_path);

    // When writing the patched file to cout - write any prompts to cerr instead.
    const bool output_to_stdout = options.out_file_path == "-";
    auto& out = output_to_stdout ? std::cerr : std::cout;

    PatchFile patch_file(options);

    const auto format = diff_format_from_options(options);

    if (format == Format::Ed)
        throw std::invalid_argument("ed format patches are not supported by this version of patch");

    std::unordered_set<std::string> backed_up_files; // to keep track in case there are multiple patches for the same file.

    bool had_failure = false;
    bool first_patch = true;

    // Continue parsing patches from the input file and applying them.
    while (true) {
        if (patch_file.file().eof()) {
            if (options.verbose)
                out << "done\n";
            break;
        }

        Patch patch(format);
        PatchHeaderInfo info;
        bool should_parse_body = parse_patch_header(patch, patch_file.file(), info, options.strip_size);

        if (patch.format == Format::Unknown) {
            if (first_patch)
                throw std::invalid_argument("Unable to determine patch format");
            break;
        }

        first_patch = false;

        std::string file_to_patch;
        if (!options.file_to_patch.empty())
            file_to_patch = options.file_to_patch;
        else
            file_to_patch = guess_filepath(patch);

        bool looks_like_adding_file = false;

        if (patch.old_file_path == "/dev/null") {
            // This should only happen if the file has already been patched!
            if (!file_to_patch.empty()) {
                out << "The next patch would create the file " << file_to_patch << ",\n"
                    << "which already exists!\n";
            } else {
                file_to_patch = patch.new_file_path;
                looks_like_adding_file = true;
            }
        }

        if (patch.operation == Operation::Binary) {
            out << "File " << (options.reverse_patch ? patch.new_file_path : patch.old_file_path) << ": git binary diffs are not supported.\n";
            had_failure = true;
            continue;
        }

        if (options.verbose || file_to_patch.empty())
            print_header_info(patch_file.file(), info, out);

        if (file_to_patch.empty())
            file_to_patch = prompt_for_filepath(out);

        if (file_to_patch.empty()) {
            out << "Skipping patch.\n";
            continue;
        }

        if (should_parse_body)
            parse_patch_body(patch, patch_file.file());

        const auto output_file = output_path(options, patch, file_to_patch);

        std::ios::openmode mode = std::ios::out;
        if (options.newline_output != Options::NewlineOutput::Native)
            mode |= std::ios::binary;

        File tmp_reject_file = File::create_temporary();
        RejectWriter reject_writer(patch, tmp_reject_file, options.reject_format);

        if (!looks_like_adding_file && !filesystem::is_regular_file(file_to_patch)) {
            // FIXME: Figure out a nice way of reducing duplication with the failure case below.
            out << "File " << file_to_patch << " is not a regular file -- refusing to patch\n";
            for (const auto& hunk : patch.hunks)
                reject_writer.write_reject_file(hunk);
            out << reject_writer.rejected_hunks() << " out of " << patch.hunks.size() << " hunk";
            if (patch.hunks.size() > 1)
                out << 's';
            std::cout << " ignored";
            if (!options.dry_run) {
                const auto reject_path = options.reject_file_path.empty() ? output_file + ".rej" : options.reject_file_path;
                out << " -- saving rejects to file " << reject_path;
                File file(reject_path, mode | std::ios::trunc);
                tmp_reject_file.write_entire_contents_to(file);
            }
            out << '\n';
            had_failure = true;
            continue;
        }

        if (options.dry_run)
            out << "checking";
        else
            out << "patching";

        out << " file " << output_file;
        if (patch.operation == Operation::Rename) {
            if (file_to_patch == output_file) {
                out << " (already renamed from " << (options.reverse_patch ? patch.new_file_path : patch.old_file_path) << ")";
                patch.operation = Operation::Change;
            } else {
                out << " (renamed from " << file_to_patch << ")";
            }
        } else if (patch.operation == Operation::Copy) {
            out << " (copied from " << file_to_patch << ")";
        } else if (!options.out_file_path.empty()) {
            out << " (read from " << file_to_patch << ")";
        }
        out << '\n';

        File input_file;
        if (!looks_like_adding_file || filesystem::exists(file_to_patch)) {
            input_file.open(file_to_patch, looks_like_adding_file ? mode : mode | std::ios_base::in);
            if (!input_file)
                throw std::system_error(errno, std::generic_category(), "Unable to open input file " + file_to_patch);
        }

        File tmp_out_file = File::create_temporary();

        Result result = apply_patch(tmp_out_file, reject_writer, input_file, patch, options, out);

        input_file.close();

        if (output_to_stdout) {
            // Nothing else to do other than write to stdout :^)
            tmp_out_file.write_entire_contents_to(stdout);
        } else {
            if (!options.dry_run) {
                if (options.save_backup || !result.all_hunks_applied_perfectly) {
                    const auto backup_file = backup_name(options, output_file);

                    // Per POSIX:
                    // > if multiple patches are applied to the same file, the .orig file will be written only for the first patch
                    if (filesystem::exists(output_file) && backed_up_files.emplace(backup_file).second)
                        filesystem::rename(output_file, backup_file);
                }

                // Ensure that parent directories exist if we are adding a file.
                if (looks_like_adding_file)
                    ensure_parent_directories(output_file);

                File file(output_file, mode | std::ios::trunc);
                tmp_out_file.write_entire_contents_to(file);

                if (patch.new_file_mode != 0) {
                    auto perms = static_cast<filesystem::perms>(patch.new_file_mode) & filesystem::perms::mask;
                    filesystem::permissions(output_file, perms);
                }
            }

            if (result.failed_hunks != 0) {
                had_failure = true;
                const char* reason = result.was_skipped ? " ignored" : " FAILED";
                out << result.failed_hunks << " out of " << patch.hunks.size() << " hunk";
                if (patch.hunks.size() > 1)
                    out << 's';
                std::cout << reason;
                if (!options.dry_run) {
                    const auto reject_path = options.reject_file_path.empty() ? output_file + ".rej" : options.reject_file_path;
                    out << " -- saving rejects to file " << reject_path;

                    File file(reject_path, mode | std::ios::trunc);
                    tmp_reject_file.write_entire_contents_to(file);
                }
                out << '\n';
            } else {
                if (!options.dry_run && patch.operation == Operation::Rename)
                    remove_file_and_empty_parent_folders(file_to_patch);

                // Clean up the file if it looks like it was removed.
                // NOTE: we check for file size for the degenerate case that the file is a removal, but has nothing left.
                if (patch.new_file_path == "/dev/null") {
                    if (filesystem::file_size(output_file) == 0) {
                        if (!options.dry_run)
                            remove_file_and_empty_parent_folders(output_file);
                    } else {
                        out << "Not deleting file " << output_file << " as content differs from patch\n";
                        had_failure = true;
                    }
                }
            }
        }
    }

    return had_failure ? 1 : 0;
}

} // namespace Patch
