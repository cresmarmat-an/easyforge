#pragma once

#include <format>
#include <string>
#include <type_traits>

namespace easyforge
{
    // Runs every test written with EASYFORGE_TEST and returns 0 when all pass,
    // so it can be returned from main. Arguments that do not start with "--" keep
    // only the tests whose names contain one of them; "--list" prints the names.
    int RunTests(int argumentCount, char** arguments);

    namespace testing
    {
        using TestFunction = void (*)();

        struct Registration
        {
            Registration(const char* name, const char* file, int line, TestFunction function);
        };

        void ReportFailure(const char* file, int line, const std::string& message);

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
#define EASYFORGE_EXPECT_EQUAL(actual, expected)                                                                  \
    do                                                                                                            \
    {                                                                                                             \
        const auto& easyforgeActual = (actual);                                                                   \
        const auto& easyforgeExpected = (expected);                                                               \
        if (!(easyforgeActual == easyforgeExpected))                                                              \
        {                                                                                                         \
            ::easyforge::testing::ReportFailure(__FILE__, __LINE__,                                               \
                "expected " #actual " to equal " #expected ", but it was " +                                      \
                    ::easyforge::testing::Describe(easyforgeActual) + " instead of " +                            \
                    ::easyforge::testing::Describe(easyforgeExpected));                                           \
        }                                                                                                         \
    } while (false)

// Records a failure unless NearlyEqual(actual, expected, tolerance) is true.
// Works with numbers, vectors, quaternions, matrices, and colors.
#define EASYFORGE_EXPECT_NEAR(actual, expected, tolerance)                                                        \
    do                                                                                                            \
    {                                                                                                             \
        const auto& easyforgeActual = (actual);                                                                   \
        const auto& easyforgeExpected = (expected);                                                               \
        if (!::easyforge::NearlyEqual(easyforgeActual, easyforgeExpected, (tolerance)))                           \
        {                                                                                                         \
            ::easyforge::testing::ReportFailure(__FILE__, __LINE__,                                               \
                "expected " #actual " to be near " #expected ", but it was " +                                    \
                    ::easyforge::testing::Describe(easyforgeActual) + " instead of " +                            \
                    ::easyforge::testing::Describe(easyforgeExpected));                                           \
        }                                                                                                         \
    } while (false)

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
