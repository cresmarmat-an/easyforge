#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <format>
#include <string>
#include <thread>

using namespace easyforge;

EASYFORGE_TEST(ClockMeasuresTime)
{
    Clock clock;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    double first = clock.Seconds();
    EASYFORGE_EXPECT(first >= 0.015);

    double measured = clock.Restart();
    EASYFORGE_EXPECT(measured >= first);
    EASYFORGE_EXPECT(clock.Seconds() < measured);
}

EASYFORGE_TEST(VersionTextMatchesNumbers)
{
    std::string expected = std::format("{}.{}.{}", Version.Major, Version.Minor, Version.Patch);
    if (!Version.Label.empty())
    {
        expected += std::format("-{}", Version.Label);
    }
    EASYFORGE_EXPECT_EQUAL(std::string(VersionText), expected);
}
