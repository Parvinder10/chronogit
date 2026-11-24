#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <chrono>
#include <optional>

namespace chronogit::test {

template <typename T>
std::string to_printable(const T& val) {
    if constexpr (requires { std::declval<std::ostringstream&>() << val; }) {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    } else {
        return "<unprintable>";
    }
}

template <typename T>
std::string to_printable(const std::optional<T>& opt) {
    if (opt.has_value()) {
        return "optional(" + to_printable(*opt) + ")";
    }
    return "nullopt";
}

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void register_test(std::string suite, std::string name, std::function<void()> func) {
        tests_.push_back(TestCase{std::move(suite), std::move(name), std::move(func)});
    }

    int run_all() {
        int passed = 0;
        int failed = 0;

        std::cout << "\033[1m[==========]\033[0m Running " << tests_.size() << " tests from " << tests_.size() << " test cases.\n";
        auto start_total = std::chrono::high_resolution_clock::now();

        for (const auto& tc : tests_) {
            std::cout << "\033[32m[ RUN      ]\033[0m " << tc.suite << "." << tc.name << "\n";
            current_test_failed_ = false;
            failure_message_.clear();

            auto start_test = std::chrono::high_resolution_clock::now();
            try {
                tc.func();
            } catch (const std::exception& e) {
                current_test_failed_ = true;
                failure_message_ = std::string("Unexpected exception: ") + e.what();
            } catch (...) {
                current_test_failed_ = true;
                failure_message_ = "Unknown exception thrown";
            }
            auto end_test = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_test - start_test).count();

            if (!current_test_failed_) {
                std::cout << "\033[32m[       OK ]\033[0m " << tc.suite << "." << tc.name << " (" << duration_ms << " ms)\n";
                passed++;
            } else {
                std::cout << "\033[31m[  FAILED  ]\033[0m " << tc.suite << "." << tc.name << ": " << failure_message_ << "\n";
                failed++;
            }
        }

        auto end_total = std::chrono::high_resolution_clock::now();
        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_total - start_total).count();

        std::cout << "\033[1m[==========]\033[0m " << tests_.size() << " tests ran. (" << total_ms << " ms total)\n";
        std::cout << "\033[32m[  PASSED  ]\033[0m " << passed << " tests.\n";
        if (failed > 0) {
            std::cout << "\033[31m[  FAILED  ]\033[0m " << failed << " tests.\n";
            return 1;
        }
        return 0;
    }

    void set_failed(const std::string& msg) {
        current_test_failed_ = true;
        if (!failure_message_.empty()) failure_message_ += "; ";
        failure_message_ += msg;
    }

private:
    std::vector<TestCase> tests_;
    bool current_test_failed_ = false;
    std::string failure_message_;
};

struct TestRegistrar {
    TestRegistrar(std::string suite, std::string name, std::function<void()> func) {
        TestRegistry::instance().register_test(std::move(suite), std::move(name), std::move(func));
    }
};

} // namespace chronogit::test

#define TEST(Suite, Name) \
    void Suite##_##Name##_Test(); \
    static ::chronogit::test::TestRegistrar Suite##_##Name##_Registrar(#Suite, #Name, Suite##_##Name##_Test); \
    void Suite##_##Name##_Test()

#define EXPECT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            ::chronogit::test::TestRegistry::instance().set_failed(std::string("EXPECT_TRUE failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define EXPECT_FALSE(cond) \
    do { \
        if (cond) { \
            ::chronogit::test::TestRegistry::instance().set_failed(std::string("EXPECT_FALSE failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define EXPECT_EQ(a, b) \
    do { \
        auto val_a = (a); \
        auto val_b = (b); \
        if (!(val_a == val_b)) { \
            std::ostringstream oss; \
            oss << "EXPECT_EQ(" #a ", " #b ") failed: [" << ::chronogit::test::to_printable(val_a) << "] != [" << ::chronogit::test::to_printable(val_b) << "] at " << __FILE__ << ":" << __LINE__; \
            ::chronogit::test::TestRegistry::instance().set_failed(oss.str()); \
        } \
    } while (0)

#define EXPECT_NE(a, b) \
    do { \
        auto val_a = (a); \
        auto val_b = (b); \
        if (val_a == val_b) { \
            std::ostringstream oss; \
            oss << "EXPECT_NE(" #a ", " #b ") failed: values are equal [" << ::chronogit::test::to_printable(val_a) << "] at " << __FILE__ << ":" << __LINE__; \
            ::chronogit::test::TestRegistry::instance().set_failed(oss.str()); \
        } \
    } while (0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            ::chronogit::test::TestRegistry::instance().set_failed(std::string("ASSERT_TRUE failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto val_a = (a); \
        auto val_b = (b); \
        if (!(val_a == val_b)) { \
            std::ostringstream oss; \
            oss << "ASSERT_EQ(" #a ", " #b ") failed: [" << ::chronogit::test::to_printable(val_a) << "] != [" << ::chronogit::test::to_printable(val_b) << "] at " << __FILE__ << ":" << __LINE__; \
            ::chronogit::test::TestRegistry::instance().set_failed(oss.str()); \
            return; \
        } \
    } while (0)

#define EXPECT_THROW(stmt, ExcType) \
    do { \
        bool threw = false; \
        try { \
            stmt; \
        } catch (const ExcType&) { \
            threw = true; \
        } catch (...) {} \
        if (!threw) { \
            ::chronogit::test::TestRegistry::instance().set_failed(std::string("EXPECT_THROW(" #stmt ", " #ExcType ") failed to throw expected exception at ") + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)
