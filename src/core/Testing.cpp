#include <easyforge/core/Testing.h>

#include <cstdio>
#include <exception>
#include <string_view>
#include <vector>

namespace easyforge
{
    namespace
    {
        struct TestCase
        {
            const char* Name;
            const char* File;
            int Line;
            testing::TestFunction Function;
        };

        std::vector<TestCase>& Registered()
        {
            static std::vector<TestCase> tests;
            return tests;
        }

        // Failure messages of the test that is running.
        std::vector<std::string>& CurrentFailures()
        {
            static std::vector<std::string> failures;
            return failures;
        }

        bool Selected(const TestCase& test, const std::vector<std::string_view>& filters)
        {
            if (filters.empty())
            {
                return true;
            }
            std::string_view name = test.Name;
            for (std::string_view filter : filters)
            {
                if (name.find(filter) != std::string_view::npos)
                {
                    return true;
                }
            }
            return false;
        }
    }

    namespace testing
    {
        Registration::Registration(const char* name, const char* file, int line, TestFunction function)
        {
            Registered().push_back({ name, file, line, function });
        }

        void ReportFailure(const char* file, int line, const std::string& message)
        {
            CurrentFailures().push_back(std::format("{}({}): {}", file, line, message));
        }
    }

    int RunTests(int argumentCount, char** arguments)
    {
        std::vector<std::string_view> filters;
        bool listOnly = false;
        for (int index = 1; index < argumentCount; ++index)
        {
            std::string_view argument = arguments[index];
            if (argument == "--list")
            {
                listOnly = true;
            }
            else if (!argument.starts_with("--"))
            {
                filters.push_back(argument);
            }
        }

        if (listOnly)
        {
            for (const TestCase& test : Registered())
            {
                if (Selected(test, filters))
                {
                    std::printf("%s\n", test.Name);
                }
            }
            return 0;
        }

        int passed = 0;
        int failed = 0;
        for (const TestCase& test : Registered())
        {
            if (!Selected(test, filters))
            {
                continue;
            }

            CurrentFailures().clear();
            try
            {
                test.Function();
            }
            catch (const std::exception& exception)
            {
                testing::ReportFailure(test.File, test.Line, std::format("threw an exception: {}", exception.what()));
            }
            catch (...)
            {
                testing::ReportFailure(test.File, test.Line, "threw an exception");
            }

            if (CurrentFailures().empty())
            {
                ++passed;
                std::printf("[pass] %s\n", test.Name);
                std::fflush(stdout);
            }
            else
            {
                ++failed;
                std::printf("[fail] %s\n", test.Name);
                for (const std::string& failure : CurrentFailures())
                {
                    std::printf("       %s\n", failure.c_str());
                }
                std::fflush(stdout);
            }
        }

        if (passed + failed == 0)
        {
            std::printf("No tests matched.\n");
            std::fflush(stdout);
            return 1;
        }

        std::printf("\n%d passed, %d failed\n", passed, failed);
        std::fflush(stdout);
        return failed == 0 ? 0 : 1;
    }
}
