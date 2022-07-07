// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <patch/applier.h>
#include <patch/cmdline.h>
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

void print_header_info(std::istream& patch, PatchHeaderInfo& header_info, std::ostream& out)
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
            if (!get_line(patch, line))
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

bool check_with_user(const std::string& question, Default default_response)
{
    char default_char = default_response == Default::True ? 'y' : 'n';
    std::cout << question << " [" << default_char << "] " << std::flush;

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
    if (std::filesystem::is_regular_file(patch.old_file_path))
        return patch.old_file_path;

    if (std::filesystem::is_regular_file(patch.new_file_path))
        return patch.new_file_path;

    if (std::filesystem::is_regular_file(patch.index_file_path))
        return patch.index_file_path;

    return {};
}

static std::string prompt_for_filepath()
{
    while (true) {
        std::cout << "File to patch: " << std::flush;

        auto buffer = read_tty_until_enter();

        if (!buffer.empty()) {
            errno = 0;
            if (std::filesystem::is_regular_file(buffer.c_str()))
                return buffer;
            auto saved_errno = errno;

            std::cout << buffer;
            if (saved_errno != 0)
                std::cout << ": " << std::strerror(saved_errno);
            std::cout << '\n';
        }

        if (check_with_user("Skip this patch", Default::True))
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
            m_in_memory_buffer << std::cin.rdbuf();
            if (!m_in_memory_buffer)
                throw std::system_error(errno, std::generic_category(), "Unable to read patch from stdin");
            m_patch_istream = &m_in_memory_buffer;
        } else {
            std::ios::openmode mode = std::ios::in | std::ios::out;
            if (options.newline_output != Options::NewlineOutput::Native)
                mode |= std::ios::binary;
            m_patch_file.open(options.patch_file_path, mode);
            if (!m_patch_file)
                throw std::system_error(errno, std::generic_category(), "Unable to open patch file " + options.patch_file_path);
        }
    }

    std::istream& istream() { return *m_patch_istream; }

private:
    std::fstream m_patch_file;
    std::stringstream m_in_memory_buffer;
    std::istream* m_patch_istream { &m_patch_file };
};

static void write_to_file(const std::string& path, std::ios::openmode mode, std::stringstream& content)
{
    std::ofstream file(path, mode | std::ios::trunc);
    if (!file)
        throw std::system_error(errno, std::generic_category(), "Unable to open file " + path);

    if (content.rdbuf()->in_avail() == 0)
        return;

    file << content.rdbuf();
    if (!file)
        throw std::system_error(errno, std::generic_category(), "Failed writing to file " + path);
}

static void remove_file_and_empty_parent_folders(std::filesystem::path path)
{
    std::filesystem::remove(path);
    while (path.has_parent_path()) {
        path = path.parent_path();
        if (!remove_empty_directory(path.string()))
            break;
    }
}

static std::string output_path(const Options& options, const Patch& patch, const std::string& file_to_patch)
{
    if (!options.out_file_path.empty())
        return options.out_file_path;

    if (patch.operation == Operation::Rename || patch.operation == Operation::Copy)
        return patch.new_file_path;

    return file_to_patch;
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

    PatchFile patch_file(options);

    const auto format = diff_format_from_options(options);

    if (format == Format::Ed)
        throw std::invalid_argument("ed format patches are not supported by this version of patch");

    std::unordered_set<std::string> backed_up_files; // to keep track in case there are multiple patches for the same file.

    bool had_failure = false;
    bool first_patch = true;

    // Continue parsing patches from the input file and applying them.
    while (true) {
        if (patch_file.istream().eof()) {
            if (options.verbose)
                std::cout << "done\n";
            break;
        }

        Patch patch(format);
        PatchHeaderInfo info;
        parse_patch_header(patch, patch_file.istream(), info, options.strip_size);

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
                std::cout << "The next patch would create the file " << file_to_patch << ",\n"
                          << "which already exists!\n";
            } else {
                file_to_patch = patch.new_file_path;
                looks_like_adding_file = true;
            }
        }

        if (options.verbose || file_to_patch.empty())
            print_header_info(patch_file.istream(), info, std::cout);

        if (file_to_patch.empty())
            file_to_patch = prompt_for_filepath();

        if (file_to_patch.empty()) {
            std::cout << "Skipping patch.\n";
            continue;
        }

        parse_patch_body(patch, patch_file.istream());

        const auto output_file = output_path(options, patch, file_to_patch);

        if (options.dry_run)
            std::cout << "checking";
        else
            std::cout << "patching";

        std::cout << " file " << output_file;
        if (patch.operation == Operation::Rename) {
            if (file_to_patch == output_file) {
                std::cout << " (already renamed from " << patch.old_file_path << ")";
                patch.operation = Operation::Change;
            } else {
                std::cout << " (rename from " << file_to_patch << ")";
            }
        } else if (patch.operation == Operation::Copy) {
            std::cout << " (copied from " << file_to_patch << ")";
        } else if (output_file == "-") {
            std::cout << " (read from " << file_to_patch << ")";
        }
        std::cout << '\n';

        std::ios::openmode mode = std::ios::out;
        if (options.newline_output != Options::NewlineOutput::Native)
            mode |= std::ios::binary;

        std::fstream input_file;
        if (!looks_like_adding_file || std::filesystem::exists(file_to_patch)) {
            input_file.open(file_to_patch, looks_like_adding_file ? mode : mode | std::fstream::in);
            if (!input_file)
                throw std::system_error(errno, std::generic_category(), "Unable to open input file " + file_to_patch);
        }

        std::stringstream tmp_out_file;
        std::stringstream tmp_reject_file;

        Result result = apply_patch(tmp_out_file, tmp_reject_file, input_file, patch, options);

        input_file.close();

        if (output_file == "-") {
            // Nothing else to do other than write to stdout :^)
            if (tmp_out_file.rdbuf()->in_avail() > 0 && !(std::cout << tmp_out_file.rdbuf()))
                throw std::system_error(errno, std::generic_category(), "Failure writing to stdout");
        } else {
            if (!options.dry_run) {
                if (options.save_backup || !result.all_hunks_applied_perfectly) {
                    const auto backup_file = output_file + ".orig";

                    // Per POSIX:
                    // > if multiple patches are applied to the same file, the .orig file will be written only for the first patch
                    if (std::filesystem::exists(output_file) && backed_up_files.emplace(backup_file).second)
                        std::filesystem::rename(output_file, backup_file);
                }

                // Ensure that parent directories exist if we are adding a file.
                if (looks_like_adding_file) {
                    std::filesystem::path path(output_file);
                    if (path.has_parent_path())
                        std::filesystem::create_directories(path.parent_path());
                }

                write_to_file(output_file, mode, tmp_out_file);
            }

            if (result.failed_hunks != 0) {
                had_failure = true;
                const char* reason = result.was_skipped ? "ignored" : "FAILED";
                std::cout << result.failed_hunks << " out of " << patch.hunks.size() << " hunks " << reason;
                if (!options.dry_run) {
                    const auto reject_path = options.reject_file_path.empty() ? output_file + ".rej" : options.reject_file_path;
                    std::cout << " -- saving rejects to file " << reject_path;
                    write_to_file(reject_path, mode, tmp_reject_file);
                }
                std::cout << '\n';
            } else {
                if (!options.dry_run && patch.operation == Operation::Rename)
                    remove_file_and_empty_parent_folders(file_to_patch);

                // Clean up the file if it looks like it was removed.
                // NOTE: we check for file size for the degenerate case that the file is a removal, but has nothing left.
                if (patch.new_file_path == "/dev/null") {
                    if (std::filesystem::file_size(output_file) == 0) {
                        if (!options.dry_run)
                            remove_file_and_empty_parent_folders(output_file);
                    } else {
                        std::cout << "Not deleting file " << output_file << " as content differs from patch\n";
                    }
                }
            }
        }
    }

    return had_failure ? 1 : 0;
}

} // namespace Patch
