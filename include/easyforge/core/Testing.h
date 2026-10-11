#pragma once

#include <format>
#include <string>
#include <type_traits>

#include <easyforge/core/Export.h>
#include <easyforge/core/Scalar.h>

namespace easyforge
{
    // Runs every test written with EASYFORGE_TEST and returns 0 when all pass,
    // so it can be returned from main. Arguments that do not start with "--" keep
    // only the tests whose names contain one of them; "--list" prints the names.
    EASYFORGE_CORE_EXPORT int RunTests(int argumentCount, char** arguments);

    namespace testing
    {
        using TestFunction = void (*)();

        struct EASYFORGE_CORE_EXPORT Registration
        {
            Registration(const char* name, const char* file, int line, TestFunction function);
        };

        EASYFORGE_CORE_EXPORT void ReportFailure(const char* file, int line, const std::string& message);

        // The value as text for a failure message, when std::format knows the type.
        template <typename Type>
        std::string Describe(const Type& value)
        {
            if constexpr (std::is_same_v<Type, bool>)
            {
                return value ? "true" : "false";
            }
            else if constexpr (std::is_default_constructible_v<std::formatter<Type, char>>)
            {
                return std::format("{}", value);
            }
            else
            {
                return "(a value that cannot be printed)";
            }
        }

        // The checks behind EASYFORGE_EXPECT_EQUAL and EASYFORGE_EXPECT_NEAR. The
        // values are parameters, so a temporary they come from, such as the
        // vector in `body.Position.Get().Y`, lives until the check is done.
        template <typename Actual, typename Expected>
        bool CheckEqual(const char* file, int line, const char* actualText, const char* expectedText,
            const Actual& actual, const Expected& expected)
        {
            if (actual == expected)
            {
                return true;
            }
            ReportFailure(file, line,
                std::string("expected ") + actualText + " to equal " + expectedText + ", but it was " + Describe(actual) +
                    " instead of " + Describe(expected));
            return false;
        }

        template <typename Actual, typename Expected, typename Tolerance>
        bool CheckNear(const char* file, int line, const char* actualText, const char* expectedText, const Actual& actual,
            const Expected& expected, const Tolerance& tolerance)
        {
            if (NearlyEqual(actual, expected, tolerance))
            {
                return true;
            }
            ReportFailure(file, line,
                std::string("expected ") + actualText + " to be near " + expectedText + ", but it was " +
                    Describe(actual) + " instead of " + Describe(expected));
            return false;
        }
    }
}

// Declares a test. The name must be unique in its file and usable as part of a C++ name.
//
//     EASYFORGE_TEST(VectorLength)
//     {
//         EASYFORGE_EXPECT_EQUAL(easyforge::Length(easyforge::Vector2 { 3, 4 }), 5.0f);
//     }
#define EASYFORGE_TEST(name)                                                                                      \
    static void EasyforgeTest_##name();                                                                           \
    static const ::easyforge::testing::Registration EasyforgeTestRegistration_##name(                             \
        #name, __FILE__, __LINE__, &EasyforgeTest_##name);                                                        \
    static void EasyforgeTest_##name()

// Records a failure when `condition` is false, and carries on with the test.
#define EASYFORGE_EXPECT(condition)                                                                               \
    do                                                                                                            \
    {                                                                                                             \
        if (!(condition))                                                                                         \
        {                                                                                                         \
            ::easyforge::testing::ReportFailure(__FILE__, __LINE__, "expected " #condition);                      \
        }                                                                                                         \
    } while (false)

// Records a failure when the two are not equal, showing both values when it can.
#define EASYFORGE_EXPECT_EQUAL(actual, expected) \
    ((void)::easyforge::testing::CheckEqual(__FILE__, __LINE__, #actual, #expected, (actual), (expected)))

// Records a failure unless NearlyEqual(actual, expected, tolerance) is true.
// Works with numbers, vectors, quaternions, matrices, and colors.
#define EASYFORGE_EXPECT_NEAR(actual, expected, tolerance) \
    ((void)::easyforge::testing::CheckNear(__FILE__, __LINE__, #actual, #expected, (actual), (expected), (tolerance)))

// Records a failure and ends the test when `condition` is false. Use it when the
// rest of the test cannot run, for example before reading a failed Result.
#define EASYFORGE_REQUIRE(condition)                                                                              \
    do                                                                                                            \
    {                                                                                                             \
        if (!(condition))                                                                                         \
        {                                                                                                         \
            ::easyforge::testing::ReportFailure(__FILE__, __LINE__, "required " #condition);                      \
            return;                                                                                               \
        }                                                                                                         \
    } while (false)
