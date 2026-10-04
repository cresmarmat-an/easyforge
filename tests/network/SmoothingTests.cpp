#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <thread>
#include <vector>

#include <easyforge/core/Random.h>
#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "NetworkTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    struct Arrival
    {
        double SentAt = 0.0;
        double ArrivesAt = 0.0;
        float Value = 0.0f;
    };

    // A value moving at 2 units a second, sent 20 times a second, arriving
    // 20 to 60 milliseconds later, some in a different order, some never.
    std::vector<Arrival> Stream(double seconds, float lost, std::uint64_t seed)
    {
        Random random(seed);
        std::vector<Arrival> arrivals;
        for (double sent = 0.05; sent <= seconds; sent += 0.05)
        {
            if (random.Chance(lost))
            {
                continue;
            }
            arrivals.push_back({ sent, sent + 0.02 + random.Fraction() * 0.04, static_cast<float>(sent * 2.0) });
        }
        std::sort(arrivals.begin(), arrivals.end(), [](const Arrival& first, const Arrival& second) { return first.ArrivesAt < second.ArrivesAt; });
        return arrivals;
    }

    // Plays a stream into a Smoothed value at 60 frames a second and gives the
    // largest share by which one frame's movement differed from an even one,
    // after the first second.
    float WorstUnevenness(const std::vector<Arrival>& arrivals, double seconds)
    {
        Smoothed<float> value;
        std::size_t next = 0;
        float last = 0.0f;
        float worst = 0.0f;
        double frame = 1.0 / 60.0;
        for (double now = 0.0; now < seconds; now += frame)
        {
            while (next < arrivals.size() && arrivals[next].ArrivesAt <= now)
            {
                value.Add(arrivals[next].Value, arrivals[next].SentAt, now);
                ++next;
            }
            float shown = value.Get(now);
            if (now > 1.0)
            {
                float moved = shown - last;
                worst = std::max(worst, std::abs(moved - static_cast<float>(2.0 * frame)) / static_cast<float>(2.0 * frame));
            }
            last = shown;
        }
        return worst;
    }
}

EASYFORGE_TEST(SmoothedMovesEvenlyThroughUnevenArrivals)
{
    EASYFORGE_EXPECT(WorstUnevenness(Stream(6.0, 0.0f, 5), 5.5) < 0.06f);
}

EASYFORGE_TEST(SmoothedCoversLostValues)
{
    EASYFORGE_EXPECT(WorstUnevenness(Stream(6.0, 0.1f, 9), 5.5) < 0.06f);
}

EASYFORGE_TEST(SmoothedHoldsTheLastValueWhenValuesStop)
{
    Smoothed<Vector2> place;
    EASYFORGE_EXPECT(!place.HasValue());
    EASYFORGE_EXPECT_EQUAL(place.Get(1.0), (Vector2 {}));
    place.Add({ 0.0f, 0.0f }, 1.0, 1.02);
    place.Add({ 1.0f, 0.0f }, 1.1, 1.13);
    place.Get(1.13);
    // Long after the last value, it stays where it ended instead of running on.
    EASYFORGE_EXPECT_EQUAL(place.Get(5.0), (Vector2 { 1.0f, 0.0f }));
    EASYFORGE_EXPECT_EQUAL(place.Newest(), (Vector2 { 1.0f, 0.0f }));
}

EASYFORGE_TEST(SmoothedJumpsOnReset)
{
    Smoothed<float> value;
    for (int index = 1; index <= 20; ++index)
    {
        double sent = index * 0.05;
        value.Add(static_cast<float>(index), sent, sent + 0.03);
        value.Get(sent + 0.03);
    }
    value.Reset(100.0f);
    EASYFORGE_EXPECT_EQUAL(value.Get(1.04), 100.0f);
    // Values after the reset carry on from it.
    value.Add(101.0f, 1.1, 1.13);
    float shown = value.Get(1.5);
    EASYFORGE_EXPECT(shown >= 100.0f && shown <= 101.0f);
}

EASYFORGE_TEST(SmoothedIgnoresValuesForMomentsAlreadyShown)
{
    Smoothed<float> value;
    value.Add(1.0f, 1.0, 1.0);
    value.Add(2.0f, 1.1, 1.1);
    value.Get(2.0);
    value.Add(-50.0f, 1.05, 2.0);
    EASYFORGE_EXPECT_EQUAL(value.Get(2.1), 2.0f);
}

EASYFORGE_TEST(SmoothedAngleTurnsTheShortWayRound)
{
    SmoothedAngle angle;
    angle.Add(2.8f, 1.0, 1.0);
    angle.Add(3.0f, 1.1, 1.1);
    angle.Add(-3.0f, 1.2, 1.2);
    angle.Get(1.2);
    // Halfway between 3 and -3 the short way round is pi, not 0.
    float halfway = angle.Get(1.15 + angle.Delay());
    EASYFORGE_EXPECT(std::abs(std::abs(halfway) - std::numbers::pi_v<float>) < 0.05f);
}

EASYFORGE_TEST(MessagesCarryWhenTheyWereSent)
{
    ConnectedPair pair = Connect({ .Latency = 0.04f, .Jitter = 0.03f });
    std::vector<double> sentTimes;
    std::vector<double> stamps;
    std::vector<double> arrivals;
    pair.Guest.OnMessage("Tick", [&](const Message& message) {
        stamps.push_back(message.SentAt());
        arrivals.push_back(NetworkTime());
    });

    for (int tick = 0; tick < 25; ++tick)
    {
        pair.Host.SendToAll("Tick", { { "Number", tick } }, Delivery::Unreliable);
        pair.Host.Update();
        sentTimes.push_back(NetworkTime());
        UpdateFor([&] { pair.Update(); }, 0.02);
    }
    UpdateFor([&] { pair.Update(); }, 0.3);

    EASYFORGE_REQUIRE(stamps.size() >= 20);
    for (std::size_t index = 0; index < stamps.size(); ++index)
    {
        // Never later than it arrived.
        EASYFORGE_EXPECT(stamps[index] > 0.0 && stamps[index] <= arrivals[index] + 0.001);
    }
    // However unevenly they arrived, the stamps are spaced as they were sent.
    std::vector<double> sorted = stamps;
    std::sort(sorted.begin(), sorted.end());
    double sentSpan = sentTimes.back() - sentTimes.front();
    double stampedSpan = sorted.back() - sorted.front();
    EASYFORGE_EXPECT(stampedSpan <= sentSpan + 0.01);
    EASYFORGE_EXPECT(stampedSpan >= sentSpan * (static_cast<double>(stamps.size() - 1) / 24.0) - 0.06);

    // Messages made here carry no time.
    EASYFORGE_EXPECT_EQUAL((Message { { "A", 1 } }).SentAt(), 0.0);
}
