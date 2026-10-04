#include <easyforge/core/FrameTimer.h>
#include <easyforge/core/Random.h>
#include <easyforge/core/Testing.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace easyforge;

namespace
{
    // Frames shown at every refresh of a screen, but each measured a little
    // late, by up to `wobble` seconds: how frame times look in a real loop.
    std::vector<double> MeasuredFrames(double interval, double wobble, int count, std::uint64_t seed)
    {
        Random random(seed);
        std::vector<double> measured;
        double previous = 0.0;
        for (int frame = 1; frame <= count; ++frame)
        {
            double started = frame * interval + random.Fraction() * wobble;
            measured.push_back(started - previous);
            previous = started;
        }
        return measured;
    }
}

EASYFORGE_TEST(FrameTimerOffPassesTimesThrough)
{
    FrameTimer timer;
    EASYFORGE_EXPECT_EQUAL(timer.Next(0.0213), 0.0213);
    timer.SetRefreshRate(60.0f);
    timer.SetRefreshRate(0.0f);
    EASYFORGE_EXPECT_EQUAL(timer.Next(0.0213), 0.0213);
    EASYFORGE_EXPECT_EQUAL(timer.RefreshInterval(), 0.0);
}

EASYFORGE_TEST(FrameTimerEvensOutWobblingFrames)
{
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    std::vector<double> measured = MeasuredFrames(1.0 / 60.0, 0.004, 1200, 7);
    double measuredTotal = 0.0;
    double reportedTotal = 0.0;
    double worst = 0.0;
    for (std::size_t frame = 0; frame < measured.size(); ++frame)
    {
        double reported = timer.Next(measured[frame]);
        measuredTotal += measured[frame];
        reportedTotal += reported;
        if (frame >= 300)
        {
            worst = std::max(worst, std::abs(reported - 1.0 / 60.0));
        }
    }
    // Measured times wobble by up to 4 ms; reported ones by a fraction of one.
    EASYFORGE_EXPECT(worst < 0.0005);
    // Nothing lost or added beyond what is still carried.
    EASYFORGE_EXPECT(std::abs(reportedTotal - measuredTotal) < 0.25 / 60.0);
}

EASYFORGE_TEST(FrameTimerFollowsTheTrueRate)
{
    // The screen says 60 hertz but runs at 59.94.
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    double trueInterval = 1001.0 / 60000.0;
    std::vector<double> measured = MeasuredFrames(trueInterval, 0.003, 4000, 11);
    double measuredTotal = 0.0;
    double reportedTotal = 0.0;
    double worst = 0.0;
    for (std::size_t frame = 0; frame < measured.size(); ++frame)
    {
        double reported = timer.Next(measured[frame]);
        measuredTotal += measured[frame];
        reportedTotal += reported;
        if (frame >= 2000)
        {
            worst = std::max(worst, std::abs(reported - trueInterval));
        }
    }
    EASYFORGE_EXPECT_NEAR(timer.RefreshInterval(), trueInterval, 0.00002);
    EASYFORGE_EXPECT(worst < 0.0005);
    EASYFORGE_EXPECT(std::abs(reportedTotal - measuredTotal) < 0.25 / 60.0);
}

EASYFORGE_TEST(FrameTimerKeepsHitchesAndStalls)
{
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    for (int frame = 0; frame < 60; ++frame)
    {
        timer.Next(1.0 / 60.0);
    }
    // A frame that took three refreshes stays three refreshes.
    EASYFORGE_EXPECT_NEAR(timer.Next(0.0502), 3.0 / 60.0, 0.0003);
    // A long stall is not rounded to refreshes at all.
    EASYFORGE_EXPECT_NEAR(timer.Next(1.0), 1.0, 0.01);
    // Steady again afterwards.
    EASYFORGE_EXPECT_NEAR(timer.Next(1.0 / 60.0), 1.0 / 60.0, 0.0003);
}

EASYFORGE_TEST(FrameTimerEvensOutALateFrameAndTheEarlyOneAfter)
{
    // As measured on a 60 hertz screen: a frame that started 9 milliseconds
    // late, and the next one on time again. The screen showed both evenly.
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    for (int frame = 0; frame < 120; ++frame)
    {
        timer.Next(1.0 / 60.0);
    }
    EASYFORGE_EXPECT_NEAR(timer.Next(0.0257), 1.0 / 60.0, 0.0006);
    EASYFORGE_EXPECT_NEAR(timer.Next(0.0076), 1.0 / 60.0, 0.0006);
    EASYFORGE_EXPECT_NEAR(timer.Next(1.0 / 60.0), 1.0 / 60.0, 0.0006);

    // The other way round: early first, then late.
    EASYFORGE_EXPECT_NEAR(timer.Next(0.0076), 1.0 / 60.0, 0.0006);
    EASYFORGE_EXPECT_NEAR(timer.Next(0.0257), 1.0 / 60.0, 0.0006);
}

EASYFORGE_TEST(FrameTimerLetsFramesOutOfStepPassThrough)
{
    // Told the screen runs at 60 hertz, but frames keep coming every 5
    // milliseconds: they are not shown in step with it.
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    double total = 0.0;
    for (int frame = 0; frame < 30; ++frame)
    {
        double reported = timer.Next(0.005);
        total += reported;
        if (frame >= 3)
        {
            EASYFORGE_EXPECT_NEAR(reported, 0.005, 0.0001);
        }
    }
    EASYFORGE_EXPECT_NEAR(total, 0.15, 0.02);
}

EASYFORGE_TEST(FrameTimerSnapsFramesThatSkipRefreshes)
{
    // A program that manages about 100 frames a second on a 144 hertz screen
    // shows each frame for one or two refreshes.
    FrameTimer timer;
    timer.SetRefreshRate(144.0f);
    double interval = 1.0 / 144.0;
    Random random(3);
    double refresh = 0.0;
    double previous = 0.0;
    for (int frame = 0; frame < 600; ++frame)
    {
        refresh += random.Chance(0.4f) ? 2.0 : 1.0;
        double started = refresh * interval + random.Fraction() * 0.0015;
        double reported = timer.Next(started - previous);
        previous = started;
        if (frame >= 300)
        {
            double share = reported / interval;
            EASYFORGE_EXPECT(std::abs(share - std::round(share)) < 0.06);
        }
    }
}

EASYFORGE_TEST(FrameTimerStartsOverOnAnotherScreen)
{
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    for (int frame = 0; frame < 100; ++frame)
    {
        timer.Next(1.0 / 60.0 + 0.0001);
    }
    timer.SetRefreshRate(60.0f);
    EASYFORGE_EXPECT(timer.RefreshInterval() > 1.0 / 60.0);
    timer.SetRefreshRate(144.0f);
    EASYFORGE_EXPECT_EQUAL(timer.RefreshInterval(), 1.0 / 144.0);
    EASYFORGE_EXPECT_NEAR(timer.Next(1.0 / 144.0), 1.0 / 144.0, 0.00001);
}

EASYFORGE_TEST(FrameTimerIgnoresBadTimes)
{
    FrameTimer timer;
    timer.SetRefreshRate(60.0f);
    EASYFORGE_EXPECT_EQUAL(timer.Next(-1.0), 0.0);
    EASYFORGE_EXPECT_EQUAL(timer.Next(std::numeric_limits<double>::quiet_NaN()), 0.0);
    EASYFORGE_EXPECT_EQUAL(timer.Next(std::numeric_limits<double>::infinity()), 0.0);
    timer.SetRefreshRate(std::numeric_limits<float>::quiet_NaN());
    EASYFORGE_EXPECT_EQUAL(timer.RefreshInterval(), 0.0);
}
