#pragma once

#include <cmath>
#include <vector>

#include <easyforge/physics.h>

namespace easyforge::testing
{
    inline constexpr float StepSeconds = 1.0f / 60.0f;

    inline void Run(const Physics2D& physics, float seconds)
    {
        int steps = static_cast<int>(std::lround(seconds / StepSeconds));
        for (int step = 0; step < steps; ++step)
        {
            physics.Step(StepSeconds);
        }
    }

    // Wide static ground whose top is at y = 0.
    inline Body2D Ground(const Physics2D& physics, float width = 40.0f)
    {
        return physics.AddBox({ .Position = { 0.0f, -0.5f }, .Size = { width, 1.0f }, .Type = BodyType::Static });
    }

    inline float Speed(const Body2D& body)
    {
        return Length(body.Velocity.Get());
    }
}
