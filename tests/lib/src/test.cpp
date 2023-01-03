// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/system.h>
#include <patch/test.h>
#include <patch/utils.h>

class Test {
public:
    explicit Test(std::string name, std::function<void(const char*)> test_function)
        : m_name(std::move(name))
        , m_test_function(std::move(test_function))
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

    bool run(const char* patch_path);

    enum class ExpectedResult {
        Pass,
        Disabled,
        ExpectedFail,
    };

    ExpectedResult expected_result() const;

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
}

bool Test::run(const char* patch_path)
{
    const auto expected = expected_result();
    if (expected == ExpectedResult::Disabled) {
        std::cout << m_name << " DISABLED\n";
        return true;
    }

    bool success = true;

    setup();

    try {
        test(patch_path);
    } catch (const std::exception& e) {
        if (expected == ExpectedResult::ExpectedFail) {
            std::cout << m_name << " XFAIL\n";
        } else {
            std::cerr << e.what() << '\n';
            std::cerr << m_name << " FAILED!\n";
            success = false;
        }
    }

    tear_down();

    return success;
}

Test::ExpectedResult Test::expected_result() const
{
    if (Patch::starts_with(m_name, "DISABLED_"))
        return ExpectedResult::Disabled;

    if (Patch::starts_with(m_name, "XFAIL_"))
        return ExpectedResult::ExpectedFail;

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
    for (Test& test : m_tests) {
        if (test.name() == test_name)
            return test.run(patch_path);
    }

    std::cerr << "Unable to find test " << test_name << '\n';
    return false;
}

bool TestRunner::run_all_tests(const char* patch_path)
{
    bool success = true;

    for (Test& test : m_tests) {
        if (!test.run(patch_path))
            success = false;
    }

    return success;
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

int main(int argc, const char* const* argv)
{
    auto& r = runner();

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
