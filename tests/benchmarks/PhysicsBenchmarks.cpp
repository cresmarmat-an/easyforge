#include "Benchmark.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <vector>

#include <easyforge/physics.h>

using namespace easyforge;

namespace
{
    // 2,000 boxes and circles dropped into a box 60 meters wide.
    Physics2D Pile(bool allowSleep)
    {
        Physics2D physics = Physics2D::New({ .AllowSleep = allowSleep });
        physics.AddBox({ .Position = { 0.0f, -0.5f }, .Size = { 60.0f, 1.0f }, .Type = BodyType::Static });
        physics.AddBox({ .Position = { -30.5f, 20.0f }, .Size = { 1.0f, 40.0f }, .Type = BodyType::Static });
        physics.AddBox({ .Position = { 30.5f, 20.0f }, .Size = { 1.0f, 40.0f }, .Type = BodyType::Static });
        for (int index = 0; index < 2000; ++index)
        {
            float x = -28.0f + static_cast<float>(index % 50) * 1.12f;
            float y = 1.0f + static_cast<float>(index / 50) * 1.1f;
            if (index % 2 == 0)
            {
                physics.AddBox({ .Position = { x, y }, .Size = { 0.9f, 0.9f } });
            }
            else
            {
                physics.AddCircle({ .Position = { x, y }, .Radius = 0.45f });
            }
        }
        return physics;
    }

    int AwakeBodies(const Physics2D& physics)
    {
        int awake = 0;
        for (const Body2D& body : physics.Bodies())
        {
            awake += body.Type.Get() == BodyType::Dynamic && body.IsAwake() ? 1 : 0;
        }
        return awake;
    }

    // What keeps bodies awake: how many move or turn faster than 5 centimetres
    // or 2 degrees a second, and the fastest of each.
    std::string Restlessness(const Physics2D& physics)
    {
        int moving = 0;
        int turning = 0;
        float fastest = 0.0f;
        float fastestTurn = 0.0f;
        for (const Body2D& body : physics.Bodies())
        {
            if (body.Type.Get() != BodyType::Dynamic)
            {
                continue;
            }
            float speed = Length(body.Velocity.Get());
            float turn = std::abs(body.AngularVelocity.Get());
            moving += speed > 0.05f ? 1 : 0;
            turning += turn > 0.035f ? 1 : 0;
            fastest = std::max(fastest, speed);
            fastestTurn = std::max(fastestTurn, turn);
        }
        return std::format("{} bodies faster than 0.05 m/s (fastest {:.3f}), {} turning faster than 0.035 rad/s (fastest {:.3f})",
            moving, fastest, turning, fastestTurn);
    }
}

EASYFORGE_BENCHMARK(Physics)
{
    constexpr float Step = 1.0f / 60.0f;
    Physics2D physics = Pile(true);
    benchmarks::Measure("2,000 bodies falling and piling up, one step", 600, [&] { physics.Step(Step); });
    for (int step = 0; step < 1200; ++step)
    {
        physics.Step(Step);
    }
    benchmarks::Note(std::format("{} of 2,000 bodies awake after 30 seconds", AwakeBodies(physics)));
    benchmarks::Note(Restlessness(physics));
    benchmarks::Measure("2,000 bodies at rest, asleep, one step", 300, [&] { physics.Step(Step); });

    Physics2D awake = Pile(false);
    for (int step = 0; step < 1800; ++step)
    {
        awake.Step(Step);
    }
    benchmarks::Measure("2,000 bodies at rest, kept awake, one step", 300, [&] { awake.Step(Step); });

    // Fast bullets across a field of still bodies, swept every step.
    Physics2D bullets = Pile(true);
    for (int step = 0; step < 1800; ++step)
    {
        bullets.Step(Step);
    }
    for (int index = 0; index < 50; ++index)
    {
        bullets.AddCircle({ .Position = { -29.0f, 2.0f + static_cast<float>(index) * 0.4f }, .Radius = 0.1f,
            .Velocity = { 200.0f, 0.0f }, .Bullet = true });
    }
    benchmarks::Measure("50 bullets through 2,000 resting bodies, one step", 120, [&] { bullets.Step(Step); });
}
