// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022-2023 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <functional>
#include <iostream>
#include <patch/file.h>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Patch {

void unset_env(const char* name);

void set_env(const char* name, const char* value);

template<class T, typename std::enable_if<!std::is_enum<T> {}>::type* = nullptr>
void test_error_format(const T& a)
{
    std::cerr << a;
}

template<class T, typename std::enable_if<std::is_enum<T> {}>::type* = nullptr>
void test_error_format(const T& a)
{
    std::cerr << static_cast<typename std::underlying_type<T>::type>(a);
}

class test_assertion_failure : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

#define EXPECT_TRUE(condition)                                                                                  \
    do {                                                                                                        \
        if (!(condition)) {                                                                                     \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": 'EXPECT_TRUE(" << #condition ")' failed\n"; \
            throw Patch::test_assertion_failure("Test failed");                                                 \
        }                                                                                                       \
    } while (false)

#define EXPECT_FALSE(condition)                                                                                  \
    do {                                                                                                         \
        if ((condition)) {                                                                                       \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": 'EXPECT_FALSE(" << #condition ")' failed\n"; \
            throw Patch::test_assertion_failure("Test failed");                                                  \
        }                                                                                                        \
    } while (false)

#define EXPECT_NE(lhs, rhs)                                                      \
    do {                                                                         \
        /* NOLINTNEXTLINE(readability-container-size-empty) */                   \
        if ((lhs) == (rhs)) {                                                    \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": ";           \
            std::cerr << "'EXPECT_NE(" << #lhs << ", " << #rhs << ")' failed, "; \
            Patch::test_error_format(lhs);                                       \
            std::cerr << " == ";                                                 \
            Patch::test_error_format(rhs);                                       \
            std::cerr << '\n';                                                   \
            throw Patch::test_assertion_failure("Test failed");                  \
        }                                                                        \
    } while (false)

#define EXPECT_EQ(lhs, rhs)                                                      \
    do {                                                                         \
        /* NOLINTNEXTLINE(readability-container-size-empty) */                   \
        if ((lhs) != (rhs)) {                                                    \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": ";           \
            std::cerr << "'EXPECT_EQ(" << #lhs << ", " << #rhs << ")' failed, "; \
            Patch::test_error_format(lhs);                                       \
            std::cerr << " != ";                                                 \
            Patch::test_error_format(rhs);                                       \
            std::cerr << '\n';                                                   \
            throw Patch::test_assertion_failure("Test failed");                  \
        }                                                                        \
    } while (false)

#define EXPECT_THROW(statement, expected_exception) \
    EXPECT_THROW_WITH_MSG(statement, expected_exception, "")

#define EXPECT_THROW_WITH_MSG(statement, expected_exception, msg)                         \
    do {                                                                                  \
        bool patch_failed_test = false;                                                   \
        std::string patch_error_msg;                                                      \
        std::string expected_msg = msg;                                                   \
        try {                                                                             \
            (statement);                                                                  \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": "                     \
                      << #statement " expected an exception of type "                     \
                      << #expected_exception ", but none was thrown.\n";                  \
            patch_failed_test = true;                                                     \
        } catch (const expected_exception& e) {                                           \
            patch_error_msg = e.what();                                                   \
        } catch (...) {                                                                   \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": "                     \
                      << #statement " throws an exception of type "                       \
                      << #expected_exception ", but threw a different type.\n";           \
            throw Patch::test_assertion_failure("Test failed");                           \
        }                                                                                 \
        if (patch_failed_test)                                                            \
            throw Patch::test_assertion_failure("Test failed");                           \
        if (!expected_msg.empty() && patch_error_msg != (msg)) {                          \
            std::cerr << "FAIL: " __FILE__ << ":" << __LINE__ << ": "                     \
                      << #statement " expected an error message of: \""                   \
                      << (msg) << "\", but instead got: \"" << patch_error_msg << "\"\n"; \
            throw Patch::test_assertion_failure("Test failed");                           \
        }                                                                                 \
    } while (false)

#define EXPECT_FILE_EQ(file, rhs)                                                         \
    do {                                                                                  \
        const auto file_data = Patch::File(file, std::ios_base::in).read_all_as_string(); \
        EXPECT_EQ(file_data, rhs);                                                        \
    } while (false)

#define EXPECT_FILE_BINARY_EQ(file, rhs)                                                                          \
    do {                                                                                                          \
        const auto file_data = Patch::File(file, std::ios_base::in | std::ios_base::binary).read_all_as_string(); \
        EXPECT_EQ(file_data, rhs);                                                                                \
    } while (false)

void register_test(std::string name, const std::function<void()>& test);
void register_test(std::string name, std::function<void(const char*)> test);

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TEST_FUNCTION_NAME(name) test_##name

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TEST_REGISTER_HELPER(name) HelperForRegisteringTest_##name

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PATCH_TEST(name)                                                \
    static void TEST_FUNCTION_NAME(name)(const char* patch_path);       \
    struct TEST_REGISTER_HELPER(name) {                                 \
        TEST_REGISTER_HELPER(name)                                      \
        ()                                                              \
        {                                                               \
            Patch::register_test(#name, TEST_FUNCTION_NAME(name));      \
        }                                                               \
    };                                                                  \
    static const TEST_REGISTER_HELPER(name) TEST_REGISTER_HELPER(name); \
    /* NOLINTNEXTLINE(readability-function-cognitive-complexity) */     \
    static void TEST_FUNCTION_NAME(name)(const char* patch_path)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TEST(name)                                                      \
    static void TEST_FUNCTION_NAME(name)();                             \
    struct TEST_REGISTER_HELPER(name) {                                 \
        TEST_REGISTER_HELPER(name)                                      \
        ()                                                              \
        {                                                               \
            Patch::register_test(#name, TEST_FUNCTION_NAME(name));      \
        }                                                               \
    };                                                                  \
    static const TEST_REGISTER_HELPER(name) TEST_REGISTER_HELPER(name); \
    /* NOLINTNEXTLINE(readability-function-cognitive-complexity) */     \
    static void TEST_FUNCTION_NAME(name)()

} // namespace Patch
