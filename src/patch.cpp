// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include <limits>
#include <patch/applier.h>
#include <patch/cmdline.h>
#include <patch/file.h>
#include <patch/hunk.h>
#include <patch/locator.h>
#include <patch/options.h>
#include <patch/parser.h>
#include <patch/patch.h>
#include <patch/system.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace Patch {
namespace {

// Owns a uniquely named temporary file staged beside its destination along with
// the pathnames and cleanup responsibility. The staged file is removed on
// destruction unless commit() has published it over the destination.
class StagedReplacement {
public:
    // Stage source's contents into a fresh temporary beside destination. When the
    // final permissions are known the temporary is created privately (0600) and
    // tightened to those permissions at commit time; a genuinely new file whose
    // mode is unknown keeps the umask-filtered default applied at creation.
    static StagedReplacement stage_regular_file(const std::string& destination, File& source,
        bool binary, const filesystem::file_metadata& metadata)
    {
        const unsigned create_permissions = metadata.permissions == filesystem::perms::unknown ? 0666 : 0600;
        StagedReplacement replacement(destination, metadata);
        replacement.m_file = File::create_temporary_near(destination, replacement.m_temporary_path, binary, create_permissions);
        source.write_entire_contents_to(replacement.m_file);
        return replacement;
    }

    ~StagedReplacement()
    {
        cleanup();
    }

    StagedReplacement(const StagedReplacement&) = delete;
    StagedReplacement& operator=(const StagedReplacement&) = delete;

    StagedReplacement(StagedReplacement&& other) noexcept
        : m_file(std::move(other.m_file))
        , m_destination(std::move(other.m_destination))
        , m_temporary_path(std::move(other.m_temporary_path))
        , m_metadata(other.m_metadata)
    {
        other.m_temporary_path.clear();
    }

    StagedReplacement& operator=(StagedReplacement&& other) noexcept
    {
        if (this != &other) {
            cleanup();
            m_file = std::move(other.m_file);
            m_destination = std::move(other.m_destination);
            m_temporary_path = std::move(other.m_temporary_path);
            m_metadata = other.m_metadata;
            other.m_temporary_path.clear();
        }
        return *this;
    }

    void commit()
    {
        // Copy the original's metadata onto the staged file, close it with error
        // checking, then rename it over the destination. Ownership is applied
        // before permissions because chown may clear the setuid/setgid bits.
        // Retain ownership of the staged file until the rename succeeds so a
        // failure still cleans up.
        m_file.set_ownership(m_metadata);
        m_file.set_permissions(m_metadata.permissions);
        m_file.close();
        filesystem::rename(m_temporary_path, m_destination);
        m_temporary_path.clear();
    }

private:
    StagedReplacement(std::string destination, filesystem::file_metadata metadata)
        : m_destination(std::move(destination))
        , m_metadata(metadata)
    {
    }

    void cleanup() noexcept
    {
        if (m_temporary_path.empty())
            return;

        std::error_code ignored;
        filesystem::remove(m_temporary_path, ignored);
        m_temporary_path.clear();
    }

    File m_file;
    std::string m_destination;
    std::string m_temporary_path;
    filesystem::file_metadata m_metadata;
};

// Create a symlink to target and rename it over destination. symlink() will not
// overwrite an existing path, so an existing regular file or symlink is replaced
// by staging the link beside the destination and renaming it into place.
static void replace_with_symlink(const std::string& target, const std::string& destination)
{
    constexpr int max_attempts = 256;

    for (int i = 0; i < max_attempts; ++i) {
        auto temporary_path = destination + ".patch-" + generate_random_alphanumeric_string(6);

        std::error_code ec;
        filesystem::symlink(target, temporary_path, ec);
        if (ec == std::errc::file_exists)
            continue;
        if (ec)
            throw std::system_error(ec, "Unable to create symbolic link " + temporary_path);

        try {
            filesystem::rename(temporary_path, destination);
        } catch (...) {
            std::error_code ignored;
            filesystem::remove(temporary_path, ignored);
            throw;
        }
        return;
    }

    throw std::system_error(EEXIST, std::generic_category(), "Failed creating temporary symbolic link near " + destination);
}

} // namespace

static std::vector<Line> file_as_lines(File& input_file)
{
    std::vector<Line> lines;
    NewLine newline;
    std::string line;
    while (input_file.get_line(line, &newline))
        lines.emplace_back(line, newline);

    return lines;
}

std::string to_string(Format format)
{
    switch (format) {
    case Format::Context:
        return "new-style context";
    case Format::Ed:
        return "ed";
    case Format::Normal:
        return "normal";
    case Format::Git:
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

    if (patch.operation == Operation::Add)
        return patch.new_file_path;

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

        if (check_with_user("Skip this patch?", out, Default::True))
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
                throw std::system_error(errno, std::generic_category(), "Can't open patch file " + options.patch_file_path + " ");
        }
    }

    File& file() { return m_patch_file; }

private:
    File m_patch_file;
};

static std::string output_path(const Options& options, const Patch& patch, const std::string& file_to_patch)
{
    if (!options.out_file_path.empty())
        return options.out_file_path;

    if (patch.operation == Operation::Rename || patch.operation == Operation::Copy)
        return options.reverse_patch ? patch.old_file_path : patch.new_file_path;

    return file_to_patch;
}

static std::string reject_path(const Options& options, const std::string& output_file)
{
    return options.reject_file_path.empty() ? output_file + ".rej" : options.reject_file_path;
}

static void inform_hunks_failed(std::ostream& out, const char* reason, const std::vector<Hunk>& hunks, size_t number_of_hunks_failed)
{
    out << number_of_hunks_failed << " out of " << hunks.size() << " hunk";
    if (hunks.size() > 1)
        out << 's';
    out << ' ' << reason;
}

static void refuse_to_patch(std::ostream& out, std::ios_base::openmode mode, const std::string& output_file, const Patch& patch, const Options& options)
{
    out << " refusing to patch\n";
    inform_hunks_failed(out, "ignored", patch.hunks, patch.hunks.size());

    if (!options.dry_run) {
        const auto reject_file = reject_path(options, output_file);
        out << " -- saving rejects to file " << reject_file;
        File file(reject_file, mode | std::ios::trunc);

        RejectWriter reject_writer(patch, file, options.reject_format);
        for (const auto& hunk : patch.hunks)
            reject_writer.write_reject_file(hunk);
    }
    out << '\n';
}

static bool needs_shell_quoting(const std::string& input)
{
    // FIXME: This list is probably incomplete.
    //        Is based off special characters in shell.
    return std::any_of(input.begin(), input.end(), [](unsigned char c) {
        return c == '!'
            || c == '`'
            || c == '$'
            || c == '('
            || c == ')'
            || c == '>'
            || c == '<'
            || c == '['
            || c == ']'
            || c == '&';
    });
}

static std::string quote_c_style(const std::string& input)
{
    std::string output = "\"";
    for (const char c : input) {
        switch (c) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += c;
            break;
        }
    }
    output += "\"";
    return output;
}

static std::string quote_shell_style(Options::QuotingStyle quote_style, const std::string& input)
{
    const bool needs_quoting = needs_shell_quoting(input);

    // FIXME: This needs to be smarter - we may need to escape characters.

    if (needs_quoting)
        return "'" + input + "'";

    auto pos = input.find('\'');
    if (pos != std::string::npos)
        return "\"" + input + "\"";

    if (quote_style == Options::QuotingStyle::Shell) {
        pos = input.find('"');
        if (pos != std::string::npos)
            return "'" + input + "'";

        return input;
    }

    return "'" + input + "'";
}

static std::string format_filename(Options::QuotingStyle quote_style, const std::string& input)
{
    if (quote_style == Options::QuotingStyle::C)
        return quote_c_style(input);

    if (quote_style == Options::QuotingStyle::ShellAlways || quote_style == Options::QuotingStyle::Shell)
        return quote_shell_style(quote_style, input);

    return input;
}

static const char* patch_operation(const Options& options)
{
    return options.dry_run ? "checking" : "patching";
}

void check_prerequisite_handling(std::ostream& out, const Options& options, const std::string& prerequisite)
{
    if (options.batch)
        throw std::runtime_error("This file doesn't appear to be the " + prerequisite + " version -- aborting.");

    if (options.force) {
        out << "Warning: this file doesn't appear to be the " << prerequisite << " version -- patching anyway.\n";
        return;
    }

    out << "This file doesn't appear to be the " << prerequisite << " version -- ";
    if (!check_with_user("patch anyway?", out, Default::False))
        throw std::runtime_error("aborted");
}

class Backup {
public:
    explicit Backup(const Options& options)
        : m_options(options)
    {
    }

    std::string backup_name(const std::string& file_path) const
    {
        if (!m_options.backup_prefix.empty() && !m_options.backup_suffix.empty())
            return m_options.backup_prefix + file_path + m_options.backup_suffix;

        if (!m_options.backup_prefix.empty())
            return m_options.backup_prefix + file_path;

        if (!m_options.backup_suffix.empty())
            return file_path + m_options.backup_suffix;

        return m_options.backup_prefix + file_path + m_options.backup_suffix + ".orig";
    }

    void make_backup_for(const std::string& file_path)
    {
        const auto backup_file = backup_name(file_path);

        // Per POSIX:
        // > if multiple patches are applied to the same file, the .orig file will be written only for the first patch
        if (m_backed_up_files.emplace(backup_file).second) {
            // Attempt to open the original directly rather than probing with a
            // separate exists() check. A missing original is expected and just
            // means the backup source is empty; any other error is fatal.
            File contents;
            filesystem::file_metadata metadata;
            if (contents.open(file_path, std::ios_base::in | std::ios_base::binary)) {
                metadata = contents.get_metadata();
            } else {
                const auto saved_errno = errno;
                if (saved_errno != ENOENT)
                    throw std::system_error(saved_errno, std::generic_category(), "Unable to open input file " + file_path);
                contents = File::create_temporary();
            }

            auto replacement = StagedReplacement::stage_regular_file(backup_file, contents,
                true, metadata);
            replacement.commit();
        }
    }

private:
    // to keep track in case there are multiple patches for the same file.
    std::unordered_set<std::string> m_backed_up_files;
    const Options& m_options;
};

class DeferredWriter {
public:
    void deferred_write(StagedReplacement&& replacement)
    {
        m_deferred_writes.emplace_back(std::move(replacement));
    }

    void finalize()
    {
        for (auto& deferred_write : m_deferred_writes)
            deferred_write.commit();
    }

private:
    std::vector<StagedReplacement> m_deferred_writes;
};

struct PermissionResult {
    filesystem::perms old_permissions { filesystem::perms::unknown };
    bool had_failure { false };
};

static PermissionResult check_read_only_handling(std::ostream& out, const Options& options, const std::string& output_file)
{
    PermissionResult result;
    result.old_permissions = filesystem::get_permissions(output_file);
    const auto write_perm_mask = filesystem::perms::group_write | filesystem::perms::owner_write | filesystem::perms::others_write;
    const bool is_read_only = result.old_permissions != filesystem::perms::unknown
        && (result.old_permissions & write_perm_mask) == filesystem::perms::none;

    // The replacement is staged and renamed into place rather than opened for
    // writing, so a read-only destination no longer needs its permissions widened
    // and restored. Only the read-only policy warning/failure remains.
    if (is_read_only && options.read_only_handling != Options::ReadOnlyHandling::Ignore) {
        out << "File " << output_file << " is read-only;";
        if (options.read_only_handling == Options::ReadOnlyHandling::Fail) {
            result.had_failure = true;
            return result;
        }

        out << " trying to patch anyway\n";
    }

    return result;
}

void write_patched_result_to_file(const Patch& patch, const std::string& output_file_path, const filesystem::file_metadata& input_metadata,
    std::ios::openmode mode, DeferredWriter& deferred_writer, File& patched_file, Backup& backup, bool make_backup)
{
    // Ensure that parent directories exist if we are adding a file.
    if (patch.operation == Operation::Add)
        ensure_parent_directories(output_file_path);

    // The atomic rename creates a new inode, so carry the original's mode and
    // ownership onto the staged replacement rather than leaving it owned by the
    // caller with default permissions. An explicit git file mode wins over the
    // preserved mode.
    auto output_metadata = input_metadata;
    if (patch.new_file_mode != 0)
        output_metadata.permissions = static_cast<filesystem::perms>(patch.new_file_mode) & filesystem::perms::mask;

    const bool binary = (mode & std::ios_base::binary) != 0;

    // A git commit may consist of many patches changing multiple files. Defer
    // publishing each regular-file replacement until every patch has parsed and
    // applied, so a later parse failure leaves the already-staged files unpublished
    // and their destinations untouched. Each replacement is atomic on its own, but
    // the collection is not a transaction: rolling back replacements already
    // renamed into place when a later one fails needs a separate transaction layer.
    //
    // Removals are applied immediately as only a single removal of a file should
    // be present in any git commit - and deferring the write causes issues when
    // checking if we should be removing the file if empty.
    if (patch.format == Format::Git && patch.operation != Operation::Delete) {
        if (filesystem::is_symlink(patch.new_file_mode)) {
            // A symlink patch should contain the filename in the contents of the patched file.
            const auto symlink_target = patched_file.read_all_as_string();
            if (make_backup)
                backup.make_backup_for(output_file_path);
            replace_with_symlink(symlink_target, output_file_path);
        } else {
            auto replacement = StagedReplacement::stage_regular_file(output_file_path, patched_file, binary, output_metadata);
            if (make_backup)
                backup.make_backup_for(output_file_path);
            deferred_writer.deferred_write(std::move(replacement));
        }
    } else {
        auto replacement = StagedReplacement::stage_regular_file(output_file_path, patched_file, binary, output_metadata);
        if (make_backup)
            backup.make_backup_for(output_file_path);
        replacement.commit();
    }
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
    Backup backup(options);

    const auto format = diff_format_from_options(options);

    bool had_failure = false;
    bool first_patch = true;

    DeferredWriter deferred_writer;

    Parser parser(patch_file.file());

    // Continue parsing patches from the input file and applying them.
    while (!parser.is_eof()) {
        Patch patch(format);
        PatchHeaderInfo info;
        bool should_parse_body = parser.parse_patch_header(patch, info, options.strip_size);

        if (patch.format == Format::Unknown) {
            if (first_patch)
                throw std::invalid_argument("Only garbage was found in the patch input.");
            if (options.verbose)
                out << "Hmm...  Ignoring the trailing garbage.\n";
            break;
        }

        first_patch = false;

        if (patch.operation == Operation::Binary) {
            out << "File " << (options.reverse_patch ? patch.new_file_path : patch.old_file_path) << ": git binary diffs are not supported.\n";
            had_failure = true;
            continue;
        }

        if (options.verbose)
            out << "Hmm...  Looks like a " << to_string(info.format) << " diff to me...\n";

        auto file_to_patch = options.file_to_patch.empty() ? guess_filepath(patch) : options.file_to_patch;

        if (file_to_patch.empty()) {
            out << "can't find file to patch at input line " << parser.line_number()
                << "\nPerhaps you "
                << (options.strip_size == -1 ? "should have used the" : "used the wrong")
                << " -p or --strip option?\n";
        }

        if (options.verbose || file_to_patch.empty())
            parser.print_header_info(info, out);

        if (file_to_patch.empty() && !options.force && !options.batch)
            file_to_patch = prompt_for_filepath(out);

        if (file_to_patch.empty()) {
            if (should_parse_body)
                parser.parse_patch_body(patch);

            if (options.force || options.batch)
                out << "No file to patch.  ";
            out << "Skipping patch.\n";
            inform_hunks_failed(out, "ignored", patch.hunks, patch.hunks.size());
            out << '\n';
            had_failure = true;
            continue;
        }

        const auto output_file = output_path(options, patch, file_to_patch);

        std::ios::openmode mode = std::ios::out;
        if (options.newline_output != Options::NewlineOutput::Native)
            mode |= std::ios::binary;

        File tmp_reject_file = File::create_temporary();
        RejectWriter reject_writer(patch, tmp_reject_file, options.reject_format);

        // Refuse to patch anything that is not a plain regular file. is_regular_file
        // does not follow symlinks, so a symbolic link is refused here (matching GNU
        // patch) rather than written through or silently replaced; there is no
        // follow-symlinks policy to opt into that. A patch that itself creates a
        // symlink is handled separately and manages its own link target.
        const bool patch_creates_symlink = filesystem::is_symlink(patch.new_file_mode);
        if (!patch_creates_symlink && filesystem::exists(file_to_patch) && !filesystem::is_regular_file(file_to_patch)) {
            if (should_parse_body)
                parser.parse_patch_body(patch);
            out << "File " << file_to_patch << " is not a regular file --";
            refuse_to_patch(out, mode, output_file, patch, options);
            had_failure = true;
            continue;
        }

        auto permission_result = check_read_only_handling(out, options, output_file);
        if (permission_result.had_failure) {
            if (should_parse_body)
                parser.parse_patch_body(patch);
            refuse_to_patch(out, mode, output_file, patch, options);
            had_failure = true;
            continue;
        }

        // Open the original read-only. The patched result is staged separately and
        // renamed into place, so a read-only destination no longer has to be opened
        // for writing (which would fail without first widening its permissions).
        File input_file;
        auto input_mode = std::ios_base::in;
        if (options.newline_output != Options::NewlineOutput::Native)
            input_mode |= std::ios_base::binary;
        input_file.open(file_to_patch, input_mode);
        if (!input_file && (errno != ENOENT || patch.operation != Operation::Add))
            throw std::system_error(errno, std::generic_category(), "Unable to open input file " + file_to_patch);

        const auto input_lines = file_as_lines(input_file);

        // Capture the original's metadata from the open descriptor so it can be
        // reapplied to the staged replacement after the atomic rename.
        filesystem::file_metadata input_metadata;
        if (input_file)
            input_metadata = input_file.get_metadata();

        input_file.close();

        if (!patch.prerequisite.empty() && !has_prerequisite(input_lines, patch.prerequisite))
            check_prerequisite_handling(out, options, patch.prerequisite);

        out << patch_operation(options) << (filesystem::is_symlink(patch.new_file_mode) ? " symbolic link " : " file ") << format_filename(options.quoting_style, output_file);

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

        if (should_parse_body)
            parser.parse_patch_body(patch);

        if (options.verbose)
            out << "Using Plan A...\n";

        File tmp_out_file = File::create_temporary();

        Result result = apply_patch(tmp_out_file, reject_writer, input_lines, patch, options, out);

        if (result.failed_hunks != 0) {
            had_failure = true;
            const char* reason = result.was_skipped ? "ignored" : "FAILED";
            inform_hunks_failed(out, reason, patch.hunks, result.failed_hunks);
            if (!options.dry_run) {
                const auto reject_file = reject_path(options, output_file);
                out << " -- saving rejects to file " << reject_file;

                File file(reject_file, mode | std::ios::trunc);
                tmp_reject_file.write_entire_contents_to(file);
            }
            out << '\n';
        }

        if (output_to_stdout) {
            // Nothing else to do other than write to stdout :^)
            tmp_out_file.write_entire_contents_to(stdout);
        } else {
            bool write_to_file = !options.dry_run;

            // Clean up the file if it looks like it was removed.
            // NOTE: we check for file size for the degenerate case that the file is a removal, but has nothing left.
            if (options.remove_empty_files == Options::OptionalBool::Yes
                && (patch.operation == Operation::Delete || (patch.format == Format::Ed && tmp_out_file.size() == 0))) {
                if (tmp_out_file.size() == 0) {
                    if (!options.dry_run)
                        remove_file_and_empty_parent_folders(output_file);
                    write_to_file = false;
                } else {
                    out << "Not deleting file " << output_file << " as content differs from patch\n";
                    had_failure = true;
                }
            }

            if (write_to_file) {
                const bool make_backup = options.save_backup
                    || (!result.all_hunks_applied_perfectly && !result.was_skipped && options.backup_if_mismatch == Options::OptionalBool::Yes);
                write_patched_result_to_file(patch, output_file, input_metadata, mode, deferred_writer, tmp_out_file, backup, make_backup);
            }

            if (result.failed_hunks == 0) {
                if (write_to_file && patch.operation == Operation::Rename)
                    remove_file_and_empty_parent_folders(file_to_patch);
            }
        }
    }

    deferred_writer.finalize();

    if (options.verbose)
        out << "done\n";

    return had_failure ? 1 : 0;
}

} // namespace Patch
