#pragma once

#include <cstdint>

#include <easyforge/core/Vector.h>

namespace easyforge
{
    // Random numbers for games and tools, from the PCG32 generator. Not suitable
    // for anything secret, such as passwords or keys.
    //
    // A Random made with a seed gives the same numbers every run, on every
    // platform. One made without a seed starts somewhere different each time.
    class Random
    {
    public:
        Random();
        explicit Random(std::uint64_t seed);

        // The next 32 random bits.
        std::uint32_t Next();

        // A number from 0 up to, but not including, 1.
        float Fraction();

        // A number from `minimum` up to, but not including, `maximum`.
        float Between(float minimum, float maximum);

        // A whole number from `minimum` to `maximum`, both included. Every value
        // is equally likely.
        int IntegerBetween(int minimum, int maximum);

        // True with the given probability: 0.25 is true a quarter of the time.
        bool Chance(float probability);

        // A point inside a circle of radius 1, every spot equally likely.
        Vector2 InsideCircle();

        // A direction of length 1, every direction equally likely.
        Vector3 Direction();

    private:
        std::uint64_t State = 0;
    };
}
