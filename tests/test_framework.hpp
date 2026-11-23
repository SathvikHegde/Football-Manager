#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace fm::test {

struct TestCase {
    std::string name;
    std::string tag;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void register_test(std::string name, std::string tag, std::function<void()> func) {
        tests_.push_back({std::move(name), std::move(tag), std::move(func)});
    }

    int run_all() {
        int passed = 0;
        int failed = 0;
        total_assertions_ = 0;
        failed_assertions_ = 0;

        std::cout << "\n======================================================\n";
        std::cout << "Running FM Engine C++20 Test Suite (" << tests_.size() << " test cases)\n";
        std::cout << "======================================================\n";

        for (const auto& test : tests_) {
            current_test_failed_ = false;
            std::cout << "[ RUN      ] " << test.name << " [" << test.tag << "]\n";
            try {
                test.func();
            } catch (const std::exception& e) {
                std::cout << "  [EXCEPTION] Caught: " << e.what() << "\n";
                current_test_failed_ = true;
            } catch (...) {
                std::cout << "  [EXCEPTION] Unknown exception caught\n";
                current_test_failed_ = true;
            }

            if (current_test_failed_) {
                std::cout << "[  FAILED  ] " << test.name << "\n";
                failed++;
            } else {
                std::cout << "[       OK ] " << test.name << "\n";
                passed++;
            }
        }

        std::cout << "======================================================\n";
        std::cout << "Test Summary: " << passed << " passed, " << failed << " failed. "
                  << total_assertions_ << " assertions evaluated.\n";
        std::cout << "======================================================\n\n";

        return (failed == 0) ? 0 : 1;
    }

    void log_assertion(bool success, const char* expr, const char* file, int line, bool is_require) {
        total_assertions_++;
        if (!success) {
            failed_assertions_++;
            current_test_failed_ = true;
            std::cout << "  " << file << ":" << line << ": FAILED:\n"
                      << "    REQUIRE( " << expr << " )\n";
            if (is_require) {
                throw std::runtime_error(std::string("Assertion failed: ") + expr);
            }
        }
    }

private:
    std::vector<TestCase> tests_;
    bool current_test_failed_{false};
    int total_assertions_{0};
    int failed_assertions_{0};
};

struct AutoRegister {
    AutoRegister(std::string name, std::string tag, std::function<void()> func) {
        TestRegistry::instance().register_test(std::move(name), std::move(tag), std::move(func));
    }
};

} // namespace fm::test

#define FM_CONCAT_INTERNAL(a, b) a##b
#define FM_CONCAT(a, b) FM_CONCAT_INTERNAL(a, b)

#define TEST_CASE(name, tag) \
    static void FM_CONCAT(test_func_, __LINE__)(); \
    static ::fm::test::AutoRegister FM_CONCAT(test_reg_, __LINE__)(name, tag, FM_CONCAT(test_func_, __LINE__)); \
    static void FM_CONCAT(test_func_, __LINE__)()

#define REQUIRE(expr) \
    ::fm::test::TestRegistry::instance().log_assertion(static_cast<bool>(expr), #expr, __FILE__, __LINE__, true)

#define CHECK(expr) \
    ::fm::test::TestRegistry::instance().log_assertion(static_cast<bool>(expr), #expr, __FILE__, __LINE__, false)

#define REQUIRE_FALSE(expr) REQUIRE(!(expr))
#define CHECK_FALSE(expr) CHECK(!(expr))
#define SECTION(name)

