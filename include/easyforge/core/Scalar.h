#pragma once

#include <cmath>

namespace easyforge
{
    inline constexpr float Pi = 3.14159265358979323846f;

    // A full turn in radians: two times Pi.
    inline constexpr float Tau = 6.28318530717958647692f;

    constexpr float Radians(float degrees)
    {
        return degrees * (Pi / 180.0f);
    }

    constexpr float Degrees(float radians)
    {
        return radians * (180.0f / Pi);
    }

    template <typename Number>
    constexpr Number Min(Number first, Number second)
    {
        return second < first ? second : first;
    }

    template <typename Number>
    constexpr Number Max(Number first, Number second)
    {
        return first < second ? second : first;
    }

    template <typename Number>
    constexpr Number Clamp(Number value, Number minimum, Number maximum)
    {
        return Min(Max(value, minimum), maximum);
    }

    // Goes from `from` (amount 0) to `to` (amount 1). Amounts outside 0 to 1 go past either end.
    constexpr float Lerp(float from, float to, float amount)
    {
        return from + (to - from) * amount;
    }

    inline bool NearlyEqual(float first, float second, float tolerance = 0.00001f)
    {
        return std::abs(first - second) <= tolerance;
    }
}
