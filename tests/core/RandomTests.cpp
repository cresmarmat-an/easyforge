#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <array>

using namespace easyforge;

EASYFORGE_TEST(RandomSameSeedSameNumbers)
{
    Random first(1234);
    Random second(1234);
    Random other(4321);

    bool anyDifferent = false;
    for (int index = 0; index < 100; ++index)
    {
        std::uint32_t value = first.Next();
        EASYFORGE_EXPECT_EQUAL(value, second.Next());
        anyDifferent = anyDifferent || value != other.Next();
    }
    EASYFORGE_EXPECT(anyDifferent);
}

EASYFORGE_TEST(RandomKnownSequence)
{
    // These are PCG32's numbers for seed 42 on its default stream. They must be
    // the same on every platform and in every version, or saved seeds break.
    Random random(42);
    EASYFORGE_EXPECT_EQUAL(random.Next(), 3270867926u);
    EASYFORGE_EXPECT_EQUAL(random.Next(), 1795671209u);
    EASYFORGE_EXPECT_EQUAL(random.Next(), 1924641435u);
}

EASYFORGE_TEST(RandomRanges)
{
    Random random(7);
    for (int index = 0; index < 10000; ++index)
    {
        float fraction = random.Fraction();
        EASYFORGE_REQUIRE(fraction >= 0.0f && fraction < 1.0f);

        float between = random.Between(-2.0f, 3.0f);
        EASYFORGE_REQUIRE(between >= -2.0f && between < 3.0f);

        Vector2 inside = random.InsideCircle();
        EASYFORGE_REQUIRE(LengthSquared(inside) <= 1.0f);

        EASYFORGE_REQUIRE(NearlyEqual(Length(random.Direction()), 1.0f, 0.0001f));
    }
}

EASYFORGE_TEST(RandomIntegersReachBothEnds)
{
    Random random(99);
    std::array<int, 6> counts = {};
    for (int index = 0; index < 6000; ++index)
    {
        int value = random.IntegerBetween(1, 6);
        EASYFORGE_REQUIRE(value >= 1 && value <= 6);
        ++counts[static_cast<std::size_t>(value - 1)];
    }
    for (int count : counts)
    {
        // Each face is expected about 1000 times.
        EASYFORGE_EXPECT(count > 800 && count < 1200);
    }

    EASYFORGE_EXPECT_EQUAL(random.IntegerBetween(5, 5), 5);
    int swapped = random.IntegerBetween(10, 1);
    EASYFORGE_EXPECT(swapped >= 1 && swapped <= 10);
}

EASYFORGE_TEST(RandomChance)
{
    Random random(3);
    int hits = 0;
    for (int index = 0; index < 10000; ++index)
    {
        hits += random.Chance(0.25f) ? 1 : 0;
    }
    EASYFORGE_EXPECT(hits > 2200 && hits < 2800);
    EASYFORGE_EXPECT(!random.Chance(0.0f));
    EASYFORGE_EXPECT(random.Chance(1.0f));
}
