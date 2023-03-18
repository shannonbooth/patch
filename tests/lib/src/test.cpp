// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#include <chrono>
#include <patch/system.h>
#include <patch/test.h>
#include <patch/utils.h>
#include <system_error>

#ifdef _WIN32
#    include <cstdlib>
#else
#    include <unistd.h>
#endif

namespace Patch {

void unset_env(const char* name)
{
#ifdef _WIN32
    int ret = _putenv_s(name, "");
#else
    int ret = unsetenv(name);
#endif
    if (ret != 0)
        throw std::system_error(errno, std::generic_category(), "Failed to unset environment variable " + std::string(name));
}

void set_env(const char* name, const char* value)
{
#ifdef _WIN32
    int ret = _putenv_s(name, value);
#else
    int ret = setenv(name, value, 1);
#endif
    if (ret != 0)
        throw std::system_error(errno, std::generic_category(), "Failed to set environment variable " + std::string(name));
}

class Test {
public:
    explicit Test(std::string name, std::function<void(const char*)> test_function)
        : m_test_function(std::move(test_function))
        , m_name(std::move(name))
        , m_start_dir(Patch::current_path())
    {
    }

    void setup();

    void tear_down();

    void test(const char* patch_path)
    {
        m_test_function(patch_path);
    }

    const std::string& name() const { return m_name; }

    bool run(const char* patch_path, bool is_compat = false);

    enum class ExpectedResult {
        Pass,
        Disabled,
        ExpectedFail,
    };

    ExpectedResult expected_result(bool is_compat) const;

private:
    std::function<void(const char*)> m_test_function;
    std::string m_name;
    std::string m_tmp_dir;
    std::string m_start_dir;
    std::string m_test;
};

void Test::setup()
{
    m_tmp_dir = Patch::filesystem::make_temp_directory();
    Patch::chdir(m_tmp_dir);
}

void Test::tear_down()
{
    Patch::chdir(m_start_dir);

#ifdef _WIN32
    std::string command = "rmdir /s /q " + m_tmp_dir;
#else
    std::string command = "rm -rf " + m_tmp_dir;
#endif

    std::system(command.c_str());

    unset_env("POSIXLY_CORRECT");
    unset_env("QUOTING_STYLE");
}

bool Test::run(const char* patch_path, bool is_compat)
{
    const auto expected = expected_result(is_compat);
    if (expected == ExpectedResult::Disabled) {
        std::cout << "[ DISABLED ] " << m_name << '\n';
        return true;
    }

    bool success = true;

    setup();

    std::cout << "[ RUN      ] " << m_name << '\n';

    const auto test_start = std::chrono::high_resolution_clock::now();

    try {
        test(patch_path);
        if (expected == ExpectedResult::ExpectedFail)
            success = false;
    } catch (const std::exception& e) {
        if (expected != ExpectedResult::ExpectedFail) {
            std::cerr << e.what() << '\n';
            std::cerr << m_name << " FAILED!\n";
            success = false;
        } else {
            success = true;
        }
    }

    const auto test_finish = std::chrono::high_resolution_clock::now();
    const auto test_time = std::chrono::duration_cast<std::chrono::milliseconds>(test_finish - test_start);

    if (success) {
        if (expected == ExpectedResult::Pass) {
            std::cout << "[       OK ] " << m_name;
        } else {
            std::cout << "[    XFAIL ] " << m_name;
        }
    } else {
        std::cout << "[  FAILED  ] " << m_name;
    }

    std::cout << " (" << test_time.count() << " ms)\n";

    tear_down();

    return success;
}

Test::ExpectedResult Test::expected_result(bool is_compat) const
{
    if (Patch::starts_with(m_name, "DISABLED_"))
        return ExpectedResult::Disabled;

    if (Patch::starts_with(m_name, "XFAIL_"))
        return ExpectedResult::ExpectedFail;

    if (is_compat) {
        if (Patch::starts_with(m_name, "COMPAT_XFAIL_"))
            return ExpectedResult::ExpectedFail;
    } else {
        if (Patch::starts_with(m_name, "PATCH_XFAIL_"))
            return ExpectedResult::ExpectedFail;
    }

    return ExpectedResult::Pass;
}

class TestRunner {
public:
    void register_test(std::string name, std::function<void(const char*)> test);

    bool run_all_tests(const char* patch_path);

    bool run_test(const char* patch_path, const char* test_name);

    const std::vector<Test>& tests() const { return m_tests; }

private:
    std::vector<Test> m_tests;
};

void TestRunner::register_test(std::string name, std::function<void(const char*)> test_function)
{
    m_tests.emplace_back(std::move(name), std::move(test_function));
}

bool TestRunner::run_test(const char* patch_path, const char* test_name)
{
    // Determine if this is a compatability test if it begins with 'compat.'
    // When looking for the test to run, strip this prefix when performing a name lookup.
    std::string normalized_test_name = test_name;
    bool is_compat = Patch::starts_with(normalized_test_name, "compat.");
    if (is_compat)
        normalized_test_name.erase(0, 7);

    for (Test& test : m_tests) {
        if (test.name() == normalized_test_name)
            return test.run(patch_path, is_compat);
    }

    std::cerr << "Unable to find test " << test_name << '\n';
    return false;
}

bool TestRunner::run_all_tests(const char* patch_path)
{
    std::cout << "[==========] Running " << m_tests.size() << " tests.\n";

    size_t passed_tests = 0;
    std::vector<std::string> failed_test_names;

    const auto overall_start = std::chrono::high_resolution_clock::now();

    for (Test& test : m_tests) {
        if (test.run(patch_path))
            ++passed_tests;
        else
            failed_test_names.emplace_back(test.name());
    }

    const auto overall_finish = std::chrono::high_resolution_clock::now();
    const auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>(overall_finish - overall_start);

    std::cout << "\n[==========] " << m_tests.size() << " tests ran. (" << overall_time.count() << " ms total)\n"
              << "[  PASSED  ] " << passed_tests << " tests.\n";

    if (!failed_test_names.empty()) {
        std::cout << "[  FAILED  ] " << failed_test_names.size() << " test";
        if (failed_test_names.size() != 1)
            std::cout << 's';
        std::cout << ", listed below:\n";
        for (const auto& test_name : failed_test_names)
            std::cout << "[  FAILED  ] " << test_name << '\n';
    }

    return failed_test_names.empty();
}

static TestRunner& runner()
{
    static TestRunner r;
    return r;
}

void register_test(std::string name, std::function<void(const char*)> test_function)
{
    runner().register_test(std::move(name), std::move(test_function));
}

void register_test(std::string name, const std::function<void()>& test_function)
{
    runner().register_test(std::move(name), [=](const char*) {
        test_function();
    });
}

} // namespace Patch

int main(int argc, const char* const* argv)
{
    auto& r = Patch::runner();

    // List tests
    if (argc == 1) {
        for (const auto& test : r.tests())
            std::cout << test.name() << '\n';
        return 0;
    }

    // Run all tests
    if (argc == 2) {
        return r.run_all_tests(argv[1]) ? 0 : 1;
    }

    // Run specific test
    if (argc == 3) {
        return r.run_test(argv[1], argv[2]) ? 0 : 1;
    }

    std::cerr << argc << " - Wrong number of arguments given\n";

    return 1;
}
