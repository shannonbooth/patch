// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/system.h>
#include <test.h>

class Test {
public:
    explicit Test(std::string name, std::function<void(const char*)> test_function)
        : m_name(std::move(name))
        , m_test_function(std::move(test_function))
        , m_tmp_dir(Patch::filesystem::make_temp_directory())
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

private:
    std::function<void(const char*)> m_test_function;

    std::string m_name;
    std::string m_tmp_dir;
    std::string m_start_dir;
    std::string m_test;
};

void Test::setup()
{
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

class TestRunner {
public:
    void register_test(std::string name, std::function<void(const char*)> test);

    bool run_all_tests(const char* patch_path);

private:
    std::vector<Test> m_tests;
};

void TestRunner::register_test(std::string name, std::function<void(const char*)> test_function)
{
    m_tests.emplace_back(std::move(name), std::move(test_function));
}

bool TestRunner::run_all_tests(const char* patch_path)
{
    bool success = true;

    for (Test& test : m_tests) {

        test.setup();

        try {
            test.test(patch_path);
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::cerr << test.name() << " FAILED!\n";
            success = false;
        }

        test.tear_down();
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

int main(int argc, const char* const* argv)
{
    if (argc != 2) {
        std::cerr << argc << " - Wrong number of arguments given\n";
        return 1;
    }

    return runner().run_all_tests(argv[1]) ? 0 : 1;
}
