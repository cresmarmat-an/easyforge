#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <string>
#include <vector>

using namespace easyforge;

namespace
{
    struct Captured
    {
        LogLevel Level;
        std::string Message;
    };

    // Sends log messages into a list for the length of a test.
    class CaptureLog
    {
    public:
        CaptureLog()
        {
            SetLogHandler([this](LogLevel level, std::string_view message) {
                Messages.push_back({ level, std::string(message) });
            });
        }

        ~CaptureLog()
        {
            SetLogHandler(nullptr);
            SetLogLevel(LogLevel::Information);
        }

        CaptureLog(const CaptureLog&) = delete;
        CaptureLog& operator=(const CaptureLog&) = delete;

        std::vector<Captured> Messages;
    };
}

EASYFORGE_TEST(LogReachesTheHandler)
{
    CaptureLog capture;

    Log("plain");
    Log(LogLevel::Warning, "careful");
    Log("loaded {} files from {}", 3, "assets");
    Log(LogLevel::Error, "code {}", 42);

    EASYFORGE_REQUIRE(capture.Messages.size() == 4);
    EASYFORGE_EXPECT_EQUAL(capture.Messages[0].Message, std::string("plain"));
    EASYFORGE_EXPECT(capture.Messages[0].Level == LogLevel::Information);
    EASYFORGE_EXPECT(capture.Messages[1].Level == LogLevel::Warning);
    EASYFORGE_EXPECT_EQUAL(capture.Messages[2].Message, std::string("loaded 3 files from assets"));
    EASYFORGE_EXPECT_EQUAL(capture.Messages[3].Message, std::string("code 42"));
}

EASYFORGE_TEST(LogBracesWithoutArgumentsAreText)
{
    CaptureLog capture;
    Log("{} is printed as it is");
    EASYFORGE_REQUIRE(capture.Messages.size() == 1);
    EASYFORGE_EXPECT_EQUAL(capture.Messages[0].Message, std::string("{} is printed as it is"));
}

EASYFORGE_TEST(LogLevelFilters)
{
    CaptureLog capture;

    Log(LogLevel::Detail, "hidden by default");
    SetLogLevel(LogLevel::Warning);
    Log("hidden now");
    Log(LogLevel::Warning, "shown");
    SetLogLevel(LogLevel::Detail);
    Log(LogLevel::Detail, "shown too");

    EASYFORGE_REQUIRE(capture.Messages.size() == 2);
    EASYFORGE_EXPECT_EQUAL(capture.Messages[0].Message, std::string("shown"));
    EASYFORGE_EXPECT_EQUAL(capture.Messages[1].Message, std::string("shown too"));
}

namespace
{
    struct CountsFormatting
    {
        int* Count;
    };
}

template <>
struct std::formatter<CountsFormatting> : std::formatter<int>
{
    template <typename FormatContext>
    auto format(const CountsFormatting& value, FormatContext& context) const
    {
        ++*value.Count;
        return std::formatter<int>::format(*value.Count, context);
    }
};

EASYFORGE_TEST(LogDoesNotFormatHiddenMessages)
{
    CaptureLog capture;
    int formatted = 0;

    Log(LogLevel::Detail, "hidden {}", CountsFormatting { &formatted });
    EASYFORGE_EXPECT_EQUAL(formatted, 0);

    Log(LogLevel::Warning, "shown {}", CountsFormatting { &formatted });
    EASYFORGE_EXPECT_EQUAL(formatted, 1);
    EASYFORGE_REQUIRE(capture.Messages.size() == 1);
    EASYFORGE_EXPECT_EQUAL(capture.Messages[0].Message, std::string("shown 1"));
}

EASYFORGE_TEST(LogHandlerMayLog)
{
    std::vector<std::string> seen;
    SetLogHandler([&seen](LogLevel, std::string_view message) {
        seen.emplace_back(message);
        if (message == "outer")
        {
            Log("inner");
        }
    });
    Log("outer");
    SetLogHandler(nullptr);

    EASYFORGE_REQUIRE(seen.size() == 2);
    EASYFORGE_EXPECT_EQUAL(seen[1], std::string("inner"));
}

EASYFORGE_TEST(LogLevelNames)
{
    EASYFORGE_EXPECT(LogLevelName(LogLevel::Detail) == "detail");
    EASYFORGE_EXPECT(LogLevelName(LogLevel::Information) == "information");
    EASYFORGE_EXPECT(LogLevelName(LogLevel::Warning) == "warning");
    EASYFORGE_EXPECT(LogLevelName(LogLevel::Error) == "error");
}
