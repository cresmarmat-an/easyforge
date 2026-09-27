#include <easyforge/core/Random.h>

#include <cmath>
#include <random>

namespace easyforge
{
    namespace
    {
        // PCG32's multiplier and its default stream.
        constexpr std::uint64_t Multiplier = 6364136223846793005ULL;
        constexpr std::uint64_t Increment = 1442695040888963407ULL;
    }

    Random::Random() : Random((static_cast<std::uint64_t>(std::random_device {}()) << 32) ^ std::random_device {}())
    {
    }

    Random::Random(std::uint64_t seed)
    {
        Next();
        State += seed;
        Next();
    }

    std::uint32_t Random::Next()
    {
        std::uint64_t previous = State;
        State = previous * Multiplier + Increment;
        auto shifted = static_cast<std::uint32_t>(((previous >> 18u) ^ previous) >> 27u);
        auto rotation = static_cast<std::uint32_t>(previous >> 59u);
        return (shifted >> rotation) | (shifted << ((32u - rotation) & 31u));
    }

    float Random::Fraction()
    {
        // The top 24 bits fill a float's precision exactly, so 1 is never reached.
        return static_cast<float>(Next() >> 8) * (1.0f / 16777216.0f);
    }

    float Random::Between(float minimum, float maximum)
    {
        float value = minimum + (maximum - minimum) * Fraction();

        // Rounding can land exactly on `maximum`, which is promised never to happen.
        if (minimum < maximum && value >= maximum)
        {
            return std::nextafter(maximum, minimum);
        }
        return value;
    }

    int Random::IntegerBetween(int minimum, int maximum)
    {
        if (minimum > maximum)
        {
            int swap = minimum;
            minimum = maximum;
            maximum = swap;
        }

        std::uint64_t span = static_cast<std::uint64_t>(static_cast<std::int64_t>(maximum) - minimum) + 1u;
        if (span > 0xFFFFFFFFull)
        {
            return static_cast<int>(static_cast<std::int32_t>(Next()));
        }

        // Numbers below `threshold` would make some results more likely than others.
        auto range = static_cast<std::uint32_t>(span);
        std::uint32_t threshold = (0u - range) % range;
        for (;;)
        {
            std::uint32_t value = Next();
            if (value >= threshold)
            {
                return static_cast<int>(static_cast<std::int64_t>(minimum) + value % range);
            }
        }
    }

    bool Random::Chance(float probability)
    {
        return Fraction() < probability;
    }

    Vector2 Random::InsideCircle()
    {
        for (;;)
        {
            Vector2 point = { Between(-1.0f, 1.0f), Between(-1.0f, 1.0f) };
            if (LengthSquared(point) <= 1.0f)
            {
                return point;
            }
        }
    }

    Vector3 Random::Direction()
    {
        // Height is uniform on a sphere, so pick it and an angle around the axis.
        float height = Between(-1.0f, 1.0f);
        float angle = Between(0.0f, Tau);
        float radius = std::sqrt(Max(0.0f, 1.0f - height * height));
        return { radius * std::cos(angle), height, radius * std::sin(angle) };
    }
}
