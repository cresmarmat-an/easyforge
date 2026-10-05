#include <atomic>
#include <cstdlib>
#include <new>

#include <easyforge/core/Testing.h>
#include <easyforge/physics.h>

#include "PhysicsTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

// Counts allocations while a test asks, so a test can check that stepping a
// settled world allocates nothing.
namespace
{
    std::atomic<bool> Counting { false };
    std::atomic<int> Allocations { 0 };
}

void* operator new(std::size_t size)
{
    if (Counting.load(std::memory_order_relaxed))
    {
        Allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{
    Physics2D StackOf(int count, bool allowSleep = true)
    {
        Physics2D physics = Physics2D::New({ .AllowSleep = allowSleep });
        Ground(physics);
        for (int level = 0; level < count; ++level)
        {
            physics.AddBox({ .Position = { 0.0f, 0.5f + static_cast<float>(level) * 1.0f } });
        }
        return physics;
    }

    bool AllAsleep(const Physics2D& physics)
    {
        for (const Body2D& body : physics.Bodies())
        {
            if (body.Type.Get() == BodyType::Dynamic && body.IsAwake())
            {
                return false;
            }
        }
        return true;
    }
}

EASYFORGE_TEST(RestingBodiesFallAsleepAndStayPut)
{
    Physics2D physics = StackOf(3);
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(AllAsleep(physics));
    std::vector<Body2D> bodies = physics.Bodies();
    Vector2 top = bodies.back().Position;
    Run(physics, 2.0f);
    EASYFORGE_EXPECT_EQUAL(bodies.back().Position.Get(), top);
    EASYFORGE_EXPECT_EQUAL(bodies.back().Velocity.Get(), (Vector2 {}));
    // Static bodies are always awake.
    EASYFORGE_EXPECT(bodies.front().IsAwake());
}

EASYFORGE_TEST(SomethingFallingOnASleepingStackWakesIt)
{
    Physics2D physics = StackOf(3);
    Run(physics, 3.0f);
    EASYFORGE_REQUIRE(AllAsleep(physics));
    Body2D top = physics.Bodies().back();
    Body2D crate = physics.AddBox({ .Position = { 0.1f, 6.0f }, .Size = { 0.6f, 0.6f }, .Density = 5.0f });
    bool woke = false;
    for (int step = 0; step < 120 && !woke; ++step)
    {
        physics.Step(StepSeconds);
        woke = top.IsAwake();
    }
    EASYFORGE_EXPECT(woke);
    // The crate comes to rest on top, and everything sleeps again.
    Run(physics, 5.0f);
    EASYFORGE_EXPECT(crate.Position.Get().Y > 3.0f);
    EASYFORGE_EXPECT(AllAsleep(physics));
}

EASYFORGE_TEST(PushingOrChangingABodyWakesIt)
{
    Physics2D physics = StackOf(1);
    Body2D box = physics.Bodies().back();
    Run(physics, 2.0f);
    EASYFORGE_REQUIRE(!box.IsAwake());

    box.ApplyImpulse({ 0.0f, 5.0f });
    EASYFORGE_EXPECT(box.IsAwake());
    Run(physics, 0.1f);
    EASYFORGE_EXPECT(box.Position.Get().Y > 0.6f);

    Run(physics, 3.0f);
    EASYFORGE_REQUIRE(!box.IsAwake());
    box.Velocity = Vector2 { 1.0f, 0.0f };
    EASYFORGE_EXPECT(box.IsAwake());

    Run(physics, 3.0f);
    EASYFORGE_REQUIRE(!box.IsAwake());
    physics.Gravity = Vector2 { 2.0f, -9.8f };
    EASYFORGE_EXPECT(box.IsAwake());

    Run(physics, 0.5f);
    box.Wake();
    EASYFORGE_EXPECT(box.IsAwake());
}

EASYFORGE_TEST(RemovingWhatABodyRestsOnMakesItFall)
{
    Physics2D physics = Physics2D::New();
    Body2D shelf = physics.AddBox({ .Position = { 0.0f, 5.0f }, .Size = { 4.0f, 0.2f }, .Type = BodyType::Static });
    Body2D box = physics.AddBox({ .Position = { 0.0f, 5.6f } });
    Run(physics, 2.0f);
    EASYFORGE_REQUIRE(!box.IsAwake());
    shelf.Remove();
    EASYFORGE_EXPECT(box.IsAwake());
    Run(physics, 0.5f);
    EASYFORGE_EXPECT(box.Position.Get().Y < 5.0f);
}

EASYFORGE_TEST(SleepCanBeTurnedOff)
{
    Physics2D physics = StackOf(2, false);
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(!AllAsleep(physics));

    physics.AllowSleep = true;
    Body2D top = physics.Bodies().back();
    top.AllowSleep = false;
    Run(physics, 3.0f);
    // A body that may not sleep keeps everything touching it awake.
    EASYFORGE_EXPECT(!AllAsleep(physics));
    top.AllowSleep = true;
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(AllAsleep(physics));

    physics.AllowSleep = false;
    EASYFORGE_EXPECT(top.IsAwake());
}

EASYFORGE_TEST(APileOfRoundBodiesFallsAsleep)
{
    // Circles packed together tremble a little where friction joins them, like
    // gears; trembling in place still counts as resting.
    Physics2D physics = Physics2D::New();
    Ground(physics, 12.0f);
    physics.AddBox({ .Position = { -5.5f, 15.0f }, .Size = { 1.0f, 30.0f }, .Type = BodyType::Static });
    physics.AddBox({ .Position = { 5.5f, 15.0f }, .Size = { 1.0f, 30.0f }, .Type = BodyType::Static });
    for (int index = 0; index < 150; ++index)
    {
        float x = -4.5f + static_cast<float>(index % 10) * 1.0f;
        float y = 0.5f + static_cast<float>(index / 10) * 1.0f;
        if (index % 3 == 0)
        {
            physics.AddBox({ .Position = { x, y }, .Size = { 0.8f, 0.8f } });
        }
        else
        {
            physics.AddCircle({ .Position = { x, y }, .Radius = 0.4f });
        }
    }
    Run(physics, 20.0f);
    EASYFORGE_EXPECT(AllAsleep(physics));
}

EASYFORGE_TEST(BallsRollToAStopUnlessNothingResists)
{
    Physics2D physics = Physics2D::New();
    Ground(physics, 200.0f);
    auto ball = [&](float x, float resistance) {
        return physics.AddCircle({ .Position = { x, 0.5f }, .Radius = 0.5f, .Velocity = { 3.0f, 0.0f },
            .AngularVelocity = -6.0f, .RollingResistance = resistance });
    };
    Body2D strong = ball(-80.0f, 0.1f);
    Body2D usual = ball(-40.0f, 0.01f);
    Body2D free = ball(0.0f, 0.0f);
    Run(physics, 6.0f);

    // A strong resistance stops a ball within a few metres.
    EASYFORGE_EXPECT(Speed(strong) < 0.01f);
    EASYFORGE_EXPECT(strong.Position.Get().X > -76.0f);
    // The usual one, about a hard ball on a hard floor, slows it gently.
    EASYFORGE_EXPECT(Speed(usual) < 2.8f);
    EASYFORGE_EXPECT(Speed(usual) > 2.3f);
    // Without any, nothing slows a ball rolling on a flat floor.
    EASYFORGE_EXPECT_NEAR(Speed(free), 3.0f, 0.05f);
}

EASYFORGE_TEST(ABodyCloseByWakesASleeperOnlyByComingAtIt)
{
    Physics2D physics = Physics2D::New();
    Ground(physics);
    Body2D sleeper = physics.AddBox({ .Position = { 0.0f, 0.5f } });
    Run(physics, 2.0f);
    EASYFORGE_REQUIRE(!sleeper.IsAwake());

    // A centimetre from its side, moving along it: near enough to be checked
    // for contact, but neither touching nor coming closer.
    Body2D passing = physics.AddBox({ .Position = { 1.01f, 0.5f }, .Size = { 1.0f, 1.0f }, .Type = BodyType::Kinematic,
        .Velocity = { 0.0f, 0.02f } });
    Run(physics, 2.0f);
    EASYFORGE_EXPECT(!sleeper.IsAwake());

    // Coming at it wakes it.
    passing.Velocity = { -0.5f, 0.0f };
    bool woke = false;
    for (int step = 0; step < 30 && !woke; ++step)
    {
        physics.Step(StepSeconds);
        woke = sleeper.IsAwake();
    }
    EASYFORGE_EXPECT(woke);
}

EASYFORGE_TEST(StacksApartSleepAndWakeApart)
{
    Physics2D physics = Physics2D::New();
    Ground(physics);
    Body2D left = physics.AddBox({ .Position = { -5.0f, 0.5f } });
    Body2D right = physics.AddBox({ .Position = { 5.0f, 0.5f } });
    Run(physics, 2.0f);
    EASYFORGE_REQUIRE(!left.IsAwake() && !right.IsAwake());
    left.ApplyImpulse({ 0.0f, 1.0f });
    EASYFORGE_EXPECT(left.IsAwake());
    EASYFORGE_EXPECT(!right.IsAwake());
}

EASYFORGE_TEST(ABodyRidingAMovingPlatformStaysAwake)
{
    Physics2D physics = Physics2D::New();
    Body2D platform = physics.AddBox({ .Position = { 0.0f, 0.0f }, .Size = { 6.0f, 0.5f }, .Type = BodyType::Kinematic,
        .Velocity = { 0.02f, 0.0f } });
    Body2D rider = physics.AddBox({ .Position = { 0.0f, 0.75f } });
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(rider.IsAwake());
    // It went along with the platform.
    EASYFORGE_EXPECT(rider.Position.Get().X > 0.03f);
    EASYFORGE_EXPECT(platform.IsAwake());
}

EASYFORGE_TEST(JoinedBodiesSleepAndWakeTogether)
{
    Physics2D physics = Physics2D::New();
    Body2D anchor = physics.AddBox({ .Position = { 0.0f, 5.0f }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D first = physics.AddBox({ .Position = { 0.0f, 4.0f }, .Size = { 0.2f, 0.2f } });
    Body2D second = physics.AddBox({ .Position = { 0.0f, 3.0f }, .Size = { 0.2f, 0.2f } });
    physics.AddHingeJoint({ .First = anchor, .Second = first, .Anchor = { 0.0f, 5.0f } });
    physics.AddHingeJoint({ .First = first, .Second = second, .Anchor = { 0.0f, 4.0f } });
    Run(physics, 4.0f);
    EASYFORGE_REQUIRE(!first.IsAwake() && !second.IsAwake());
    second.ApplyImpulse({ 0.5f, 0.0f });
    EASYFORGE_EXPECT(first.IsAwake());
}

EASYFORGE_TEST(AFastBodyDoesNotPassThroughAThinWall)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    physics.AddBox({ .Position = { 0.0f, 0.0f }, .Size = { 0.02f, 4.0f }, .Type = BodyType::Static });
    Body2D pellet = physics.AddCircle({ .Position = { -5.0f, 0.0f }, .Radius = 0.05f, .Velocity = { 300.0f, 0.0f } });
    Run(physics, 0.5f);
    EASYFORGE_EXPECT(pellet.Position.Get().X < 0.0f);

    // Turning as well as moving.
    Body2D stick = physics.AddCapsule({ .Position = { -5.0f, 1.0f }, .Length = 0.4f, .Radius = 0.03f, .Velocity = { 250.0f, 0.0f },
        .AngularVelocity = 40.0f });
    Run(physics, 0.5f);
    EASYFORGE_EXPECT(stick.Position.Get().X < 0.0f);
}

EASYFORGE_TEST(ABulletDoesNotPassThroughThinMovingBodies)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    Body2D plank = physics.AddBox({ .Position = { 0.0f, 0.0f }, .Size = { 0.05f, 3.0f }, .Density = 50.0f, .FixedRotation = true });
    Body2D bullet = physics.AddCircle({ .Position = { -6.0f, 0.0f }, .Radius = 0.05f, .Velocity = { 300.0f, 0.0f }, .Bullet = true });
    Run(physics, 0.3f);
    EASYFORGE_EXPECT(bullet.Position.Get().X < plank.Position.Get().X);
    // The plank took the hit.
    EASYFORGE_EXPECT(plank.Velocity.Get().X > 0.0f);
}

EASYFORGE_TEST(AFastBodySlidesAlongAWallWithoutStopping)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, -9.8f } });
    physics.AddBox({ .Position = { 50.0f, -0.5f }, .Size = { 200.0f, 1.0f }, .Type = BodyType::Static, .Friction = 0.0f });
    Body2D sled = physics.AddBox({ .Position = { 0.0f, 0.2f }, .Size = { 0.6f, 0.4f }, .Velocity = { 40.0f, 0.0f }, .Friction = 0.0f });
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(sled.Position.Get().X, 40.0f, 1.0f);
    EASYFORGE_EXPECT_NEAR(sled.Position.Get().Y, 0.2f, 0.05f);
}

EASYFORGE_TEST(StepsOfASettledWorldAllocateNothing)
{
    // Kept awake, so every part of a step runs.
    Physics2D physics = StackOf(20, false);
    for (int column = 0; column < 10; ++column)
    {
        physics.AddCircle({ .Position = { 3.0f + static_cast<float>(column), 0.5f } });
    }
    Run(physics, 3.0f);
    Allocations = 0;
    Counting = true;
    Run(physics, 1.0f);
    Counting = false;
    EASYFORGE_EXPECT_EQUAL(Allocations.load(), 0);
}

EASYFORGE_TEST(SleepingGivesTheSameResultEveryRun)
{
    auto run = [] {
        Physics2D physics = StackOf(5);
        Run(physics, 3.0f);
        physics.AddCircle({ .Position = { 0.3f, 9.0f }, .Radius = 0.3f });
        Run(physics, 4.0f);
        float total = 0.0f;
        for (const Body2D& body : physics.Bodies())
        {
            total += body.Position.Get().X * 7.0f + body.Position.Get().Y * 3.0f + body.Rotation.Get();
        }
        return total;
    };
    EASYFORGE_EXPECT_EQUAL(run(), run());
}
