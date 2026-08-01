// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/directory.h>
#include <patch/file.h>
#include <patch/system.h>
#include <patch/test.h>
#include <system_error>

// Whether resolution refuses the path for leaving the directory being patched.
// Any other failure propagates, so a test cannot mistake a broken resolution
// for either answer.
static bool escapes(const std::string& path, Patch::PathOrigin origin = Patch::PathOrigin::Patch)
{
    try {
        Patch::PathResolver(".").resolve(path, origin);
    } catch (const Patch::UnsafePatchPath&) {
        return true;
    }

    return false;
}

static void create_link(const std::string& target, const std::string& path)
{
    Patch::PathResolver(".").resolve(path, Patch::PathOrigin::User).create_symlink(target);
}

TEST(directory_names_the_final_component)
{
    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("f.txt", Patch::PathOrigin::Patch);
    EXPECT_EQ(resolved.name(), "f.txt");

    Patch::create_directory("a");
    Patch::create_directory("a/b");
    auto nested = resolver.resolve("a/b/f.txt", Patch::PathOrigin::Patch);
    EXPECT_EQ(nested.name(), "f.txt");

    // A repeated separator and '.' name the same directory.
    auto same = resolver.resolve("a//./b/f.txt", Patch::PathOrigin::Patch);
    EXPECT_EQ(same.name(), "f.txt");
}

TEST(directory_refuses_to_leave_the_directory_being_patched)
{
    Patch::create_directory("a");
    Patch::create_directory("b");

    EXPECT_TRUE(escapes("../f.txt"));
    EXPECT_TRUE(escapes("a/../../f.txt"));

    // Rising and returning within the directory is fine.
    EXPECT_FALSE(escapes("a/../b/f.txt"));

    // The user may name whatever they like on the command line.
    EXPECT_FALSE(escapes("../f.txt", Patch::PathOrigin::User));
}

TEST(directory_follows_a_link_which_stays_inside)
{
    Patch::skip_without_symlink_support();
    Patch::create_directory("real");
    create_link("real", "link");

    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("link/f.txt", Patch::PathOrigin::Patch);
    EXPECT_EQ(resolved.name(), "f.txt");

    // Writing through the link must land in the real directory.
    auto* stream = resolved.create_exclusive_sibling("f.txt", true,
        Patch::filesystem::perms::owner_read | Patch::filesystem::perms::owner_write);
    EXPECT_TRUE(stream != nullptr);
    std::fclose(stream);
    EXPECT_TRUE(Patch::file_exists("real/f.txt"));
}

TEST(directory_refuses_a_link_which_leaves)
{
    Patch::skip_without_symlink_support();
    Patch::create_directory("inside");
    create_link("..", "escape");
    create_link("/tmp", "absolute");

    EXPECT_TRUE(escapes("escape/f.txt"));
    EXPECT_TRUE(escapes("absolute/f.txt"));

    // A link which rises and comes back stays inside.
    create_link("../inside", "inside/back");
    EXPECT_FALSE(escapes("inside/back/f.txt"));
}

TEST(directory_refuses_a_link_loop)
{
    Patch::skip_without_symlink_support();
    create_link("loop_b", "loop_a");
    create_link("loop_a", "loop_b");

    EXPECT_TRUE(escapes("loop_a/f.txt"));
}

TEST(directory_reports_a_component_which_is_not_a_directory)
{
    {
        Patch::File file("plain", std::ios_base::out);
        file << "content\n";
        file.close();
    }

    // Not an escape - it is simply not a directory.
    bool escaped = false;
    bool not_a_directory = false;
    try {
        Patch::PathResolver(".").resolve("plain/f.txt", Patch::PathOrigin::Patch);
    } catch (const Patch::UnsafePatchPath&) {
        escaped = true;
    } catch (const std::system_error& error) {
        not_a_directory = error.code() == std::errc::not_a_directory;
    }
    EXPECT_FALSE(escaped);
    EXPECT_TRUE(not_a_directory);
}

TEST(directory_refuses_to_open_a_link_as_a_file)
{
    Patch::skip_without_symlink_support();
    {
        Patch::File file("real.txt", std::ios_base::out);
        file << "content\n";
        file.close();
    }
    create_link("real.txt", "link.txt");

    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("link.txt", Patch::PathOrigin::Patch);
    EXPECT_EQ(resolved.name(), "link.txt");

    // The final component is opened with O_NOFOLLOW, so a link is refused
    // rather than followed through to its target.
    EXPECT_THROW(resolved.open_read(true), std::system_error);

    auto real = resolver.resolve("real.txt", Patch::PathOrigin::Patch);
    auto* stream = real.open_read(true);
    EXPECT_TRUE(stream != nullptr);
    if (stream)
        std::fclose(stream);
}

TEST(directory_type_and_permissions_do_not_follow_a_link)
{
    Patch::skip_without_symlink_support();
    {
        Patch::File file("target.txt", std::ios_base::out);
        file << "content\n";
        file.close();
    }
    create_link("target.txt", "pointer.txt");

    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("pointer.txt", Patch::PathOrigin::Patch);

    EXPECT_EQ(resolved.type(), Patch::FileType::Symlink);
    EXPECT_EQ(resolver.resolve("target.txt", Patch::PathOrigin::Patch).type(), Patch::FileType::Regular);
    EXPECT_EQ(resolver.resolve("does-not-exist", Patch::PathOrigin::Patch).type(), Patch::FileType::None);
}

TEST(directory_creates_missing_directories)
{
    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("made/up/f.txt", Patch::PathOrigin::Patch, true);
    EXPECT_EQ(resolved.name(), "f.txt");
    EXPECT_TRUE(Patch::file_exists("made/up"));
}

TEST(directory_preserves_an_absolute_user_path)
{
    Patch::create_directory("absolute");
    const auto path = Patch::current_path() + "/absolute/f.txt";

    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve(path, Patch::PathOrigin::User);
    auto file = Patch::File::open_write(resolved, true);
    EXPECT_TRUE(static_cast<bool>(file));
    file << "content\n";
    file.close();

    EXPECT_FILE_EQ("absolute/f.txt", "content\n");
}

TEST(directory_resolves_a_relative_user_path_from_its_pinned_root)
{
    Patch::File::touch("f.txt");
    Patch::create_directory("other");

    Patch::PathResolver resolver(".");
    Patch::chdir("other");
    auto resolved = resolver.resolve("f.txt", Patch::PathOrigin::User);
    EXPECT_EQ(resolved.type(), Patch::FileType::Regular);
    Patch::chdir("..");
}

TEST(directory_uses_the_requested_root)
{
    Patch::create_directory("root");
    Patch::File::touch("root/f.txt");

    Patch::PathResolver resolver("root");
    EXPECT_EQ(resolver.resolve("f.txt", Patch::PathOrigin::Patch).type(), Patch::FileType::Regular);
    EXPECT_EQ(resolver.resolve("missing.txt", Patch::PathOrigin::Patch).type(), Patch::FileType::None);
}

TEST(directory_sibling_apis_accept_only_one_component)
{
    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("f.txt", Patch::PathOrigin::Patch);
    const auto permissions = Patch::filesystem::perms::owner_read
        | Patch::filesystem::perms::owner_write;

    EXPECT_THROW(resolved.create_exclusive_sibling("../outside", true, permissions),
        std::system_error);
    EXPECT_THROW(resolved.create_exclusive_sibling("nested/file", true, permissions),
        std::system_error);
    EXPECT_THROW(resolved.with_suffix("/outside"), std::system_error);
}

TEST(directory_renames_and_removes_within_itself)
{
    Patch::PathResolver resolver(".");
    auto source = resolver.resolve("from.txt", Patch::PathOrigin::Patch);

    auto* stream = source.create_exclusive_sibling("from.txt", true,
        Patch::filesystem::perms::owner_read | Patch::filesystem::perms::owner_write);
    EXPECT_TRUE(stream != nullptr);
    std::fclose(stream);

    auto destination = resolver.resolve("to.txt", Patch::PathOrigin::Patch);
    destination.replace_with_sibling("from.txt");
    EXPECT_FALSE(Patch::file_exists("from.txt"));
    EXPECT_TRUE(Patch::file_exists("to.txt"));

    destination.remove();
    EXPECT_FALSE(Patch::file_exists("to.txt"));
}

TEST(directory_removes_a_parent_only_when_empty)
{
    Patch::create_directory("outer");
    Patch::create_directory("outer/inner");
    Patch::File::touch("outer/inner/f.txt");
    Patch::File::touch("outer/keep.txt");

    Patch::PathResolver resolver(".");
    auto file = resolver.resolve("outer/inner/f.txt", Patch::PathOrigin::Patch);
    file.remove_and_empty_parents();

    EXPECT_FALSE(Patch::file_exists("outer/inner"));
    EXPECT_TRUE(Patch::file_exists("outer"));
}

TEST(directory_prunes_user_path_parents)
{
    Patch::create_directory("outside");
    Patch::create_directory("outside/deep");
    Patch::File::touch("outside/deep/f.txt");

    Patch::PathResolver resolver(".");
    auto file = resolver.resolve(Patch::current_path() + "/outside/deep/f.txt", Patch::PathOrigin::User);
    file.remove_and_empty_parents();

    EXPECT_FALSE(Patch::file_exists("outside/deep/f.txt"));
    EXPECT_FALSE(Patch::file_exists("outside/deep"));
    EXPECT_FALSE(Patch::file_exists("outside"));
}

TEST(directory_does_not_prune_parents_reached_through_a_link)
{
    Patch::skip_without_symlink_support();
    Patch::create_directory("real");
    Patch::create_directory("real/empty");
    Patch::File::touch("real/empty/f.txt");
    create_link("real", "link");

    Patch::PathResolver resolver(".");
    auto file = resolver.resolve("link/empty/f.txt", Patch::PathOrigin::Patch);
    file.remove_and_empty_parents();

    EXPECT_FALSE(Patch::file_exists("real/empty/f.txt"));
    EXPECT_TRUE(Patch::file_exists("real/empty"));
}

TEST(directory_refuses_a_name_holding_a_nul)
{
    // A name is carried in a std::string, which can hold a NUL that the system
    // call it is handed would silently truncate at.
    const std::string name("f.txt\0.orig", 11);

    Patch::PathResolver resolver(".");
    EXPECT_TRUE(escapes(name));
    EXPECT_THROW(resolver.resolve(name, Patch::PathOrigin::User), std::system_error);
}

TEST(directory_refuses_a_name_which_is_not_one_component)
{
    Patch::PathResolver resolver(".");
    auto resolved = resolver.resolve("f.txt", Patch::PathOrigin::Patch);

    EXPECT_THROW(resolved.with_suffix("/../escape"), std::system_error);
    EXPECT_THROW(resolved.remove_sibling("a/b"), std::system_error);
    EXPECT_THROW(resolved.remove_sibling(".."), std::system_error);
}

TEST(directory_refuses_a_name_windows_cannot_address)
{
#ifdef _WIN32
    // Win32 name rules do not apply to the native API, so a name Win32 would
    // refuse or rewrite must not reach the filesystem.
    EXPECT_TRUE(escapes("bad<name"));
    EXPECT_TRUE(escapes("bad>name"));
    EXPECT_TRUE(escapes("bad\"name"));
    EXPECT_TRUE(escapes("bad|name"));
    EXPECT_TRUE(escapes("what?.txt"));
    EXPECT_TRUE(escapes("star*.txt"));
    EXPECT_TRUE(escapes(std::string("bell\afile", 9)));

    // Win32 strips a trailing space or period from a name.
    EXPECT_TRUE(escapes("trailing."));
    EXPECT_TRUE(escapes("trailing "));

    // A DOS device name refers to the device, whatever the case, extension,
    // or directory, and even spelled with a superscript digit.
    EXPECT_TRUE(escapes("NUL"));
    EXPECT_TRUE(escapes("nul.txt"));
    EXPECT_TRUE(escapes("COM1"));
    EXPECT_TRUE(escapes("lpt9.log"));
    EXPECT_TRUE(escapes("AUX/f.txt"));
    EXPECT_TRUE(escapes("com\xC2\xB9"));

    // Only the device names themselves are reserved.
    EXPECT_FALSE(escapes("CONSOLE"));
    EXPECT_FALSE(escapes("COM10"));
    EXPECT_FALSE(escapes("NULL.txt"));
    EXPECT_FALSE(escapes("dot.in.the.middle"));
#endif
}
