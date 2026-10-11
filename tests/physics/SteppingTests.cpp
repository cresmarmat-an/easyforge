#include <cmath>
#include <limits>
#include <numbers>

#include <easyforge/core/Testing.h>
#include <easyforge/physics.h>

#include "PhysicsTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(UpdateRunsWholeStepsAndKeepsTheRest)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    EASYFORGE_EXPECT_NEAR(physics.StepSeconds.Get(), 1.0f / 60.0f, 1e-9f);
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.01f), 0);
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.01f), 1);
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.035f), 2);
    // 0.055 seconds in all is three steps of 1/60 and 0.005 seconds kept.
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.0001f), 0);
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.012f), 1);
}

EASYFORGE_TEST(RulesRunWithEachStep)
{
    // A push before each step, counted, so the rules keep to the steps however
    // the frames fall; and a body a touch marked goes after the step.
    int before = 0;
    int after = 0;
    bool touched = false;
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f }, .StepSeconds = 0.01f });
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 0.0f }, .Radius = 0.5f });
    Body2D wall = physics.AddBox({ .Position = { 2.0f, 0.0f }, .Size = { 0.5f, 4.0f }, .Type = BodyType::Static });
    ball.OnTouch = [&](const Contact2D&) { touched = true; };
    physics.BeforeStep = [&](float seconds) {
        EASYFORGE_EXPECT_NEAR(seconds, 0.01f, 1e-9f);
        ++before;
        if (ball)
        {
            ball.ApplyForce({ 50.0f, 0.0f });
        }
    };
    physics.AfterStep = [&](float) {
        ++after;
        if (touched && ball)
        {
            ball.Remove();
        }
    };

    EASYFORGE_EXPECT_EQUAL(physics.Update(0.035f), 3);
    EASYFORGE_EXPECT_EQUAL(before, 3);
    EASYFORGE_EXPECT_EQUAL(after, 3);
    for (int frame = 0; frame < 120 && physics.BodyCount() == 2; ++frame)
    {
        physics.Update(1.0f / 60.0f);
    }
    EASYFORGE_EXPECT(touched);
    EASYFORGE_EXPECT_EQUAL(physics.BodyCount(), std::size_t { 1 });
    EASYFORGE_EXPECT_EQUAL(before, after);
}

EASYFORGE_TEST(UpdateCatchesUpOnlySoFar)
{
    Physics2D physics = Physics2D::New({ .MaximumStepsPerUpdate = 5 });
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 100.0f } });
    EASYFORGE_EXPECT_EQUAL(physics.Update(2.0f), 5);
    // What could not be stepped is let go, not saved for later.
    EASYFORGE_EXPECT_EQUAL(physics.Update(1.0f / 60.0f), 1);
    physics.MaximumStepsPerUpdate = 0;
    EASYFORGE_EXPECT_EQUAL(physics.MaximumStepsPerUpdate.Get(), 1);
    EASYFORGE_EXPECT(ball.Position.Get().Y < 100.0f);
}

EASYFORGE_TEST(UpdateIgnoresBadTimes)
{
    Physics2D physics = Physics2D::New();
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 10.0f } });
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.0f), 0);
    EASYFORGE_EXPECT_EQUAL(physics.Update(-1.0f), 0);
    EASYFORGE_EXPECT_EQUAL(physics.Update(std::numeric_limits<float>::quiet_NaN()), 0);
    EASYFORGE_EXPECT_EQUAL(physics.Update(std::numeric_limits<float>::infinity()), 0);
    EASYFORGE_EXPECT_EQUAL(ball.Position.Get().Y, 10.0f);
    EASYFORGE_EXPECT_EQUAL(Physics2D().Update(1.0f), 0);

    physics.StepSeconds = -1.0f;
    EASYFORGE_EXPECT_NEAR(physics.StepSeconds.Get(), 1.0f / 60.0f, 1e-9f);
    physics.StepSeconds = 0.01f;
    EASYFORGE_EXPECT_EQUAL(physics.Update(0.025f), 2);
}

EASYFORGE_TEST(SmoothPositionBlendsTheLastTwoSteps)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    Body2D puck = physics.AddCircle({ .Velocity = { 6.0f, 0.0f } });
    EASYFORGE_EXPECT_EQUAL(physics.Update(1.5f / 60.0f), 1);
    EASYFORGE_EXPECT_NEAR(puck.Position.Get().X, 0.1f, 1e-5f);
    // Half a step into the next: halfway between the last two steps.
    EASYFORGE_EXPECT_NEAR(puck.SmoothPosition().X, 0.05f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(puck.SmoothWorldPoint({ 0.5f, 0.0f }).X, 0.55f, 1e-5f);
}

EASYFORGE_TEST(SmoothPositionMovesEvenlyAtAnyFrameRate)
{
    // At 144 frames a second, steps of 1/60 land on some frames and not on
    // others, but the smooth position moves the same amount every frame.
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    Body2D puck = physics.AddCircle({ .Velocity = { 6.0f, 0.0f } });
    float frame = 1.0f / 144.0f;
    physics.Update(frame);
    float last = puck.SmoothPosition().X;
    int stepsSeen[3] = {};
    for (int index = 0; index < 300; ++index)
    {
        int steps = physics.Update(frame);
        ++stepsSeen[std::min(steps, 2)];
        float now = puck.SmoothPosition().X;
        if (index > 2)
        {
            EASYFORGE_EXPECT_NEAR(now - last, 6.0f * frame, 1e-3f);
        }
        last = now;
    }
    EASYFORGE_EXPECT(stepsSeen[0] > 0);
    EASYFORGE_EXPECT(stepsSeen[1] > 0);
}

EASYFORGE_TEST(SmoothRotationTakesTheShortWayRound)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    float start = std::numbers::pi_v<float> - 0.05f;
    Body2D wheel = physics.AddCircle({ .Rotation = start, .AngularVelocity = 6.0f });
    physics.Update(1.5f / 60.0f);
    // The wheel turned 0.1 radians past pi, so halfway is just past pi, not
    // somewhere back near zero.
    float smooth = wheel.SmoothRotation();
    float expected = start + 0.05f;
    float difference = std::remainder(smooth - expected, 2.0f * std::numbers::pi_v<float>);
    EASYFORGE_EXPECT_NEAR(difference, 0.0f, 1e-3f);
}

EASYFORGE_TEST(PlacingABodyMovesItsSmoothPlaceAtOnce)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, 0.0f } });
    Body2D puck = physics.AddCircle({ .Velocity = { 6.0f, 0.0f } });
    physics.Update(1.5f / 60.0f);
    puck.Position = Vector2 { 20.0f, 3.0f };
    EASYFORGE_EXPECT_EQUAL(puck.SmoothPosition(), (Vector2 { 20.0f, 3.0f }));
    puck.Rotation = 1.0f;
    EASYFORGE_EXPECT_NEAR(puck.SmoothRotation(), 1.0f, 1e-6f);

    // A body added between steps is drawn where it was put.
    Body2D added = physics.AddBox({ .Position = { -4.0f, 2.0f } });
    EASYFORGE_EXPECT_EQUAL(added.SmoothPosition(), (Vector2 { -4.0f, 2.0f }));

    // After a manual Step, the smooth place is the place.
    physics.Step(1.0f / 60.0f);
    EASYFORGE_EXPECT_EQUAL(puck.SmoothPosition(), puck.Position.Get());
    EASYFORGE_EXPECT_EQUAL(Body2D().SmoothPosition(), (Vector2 {}));
}

EASYFORGE_TEST(UpdateGivesTheSameResultEveryRun)
{
    auto run = [] {
        Physics2D physics = Physics2D::New();
        Ground(physics);
        for (int level = 0; level < 6; ++level)
        {
            physics.AddBox({ .Position = { 0.1f * static_cast<float>(level), 0.5f + static_cast<float>(level) * 1.01f } });
        }
        // Uneven frames, the same each run.
        for (int frame = 0; frame < 400; ++frame)
        {
            physics.Update(frame % 3 == 0 ? 0.021f : 0.013f);
        }
        float total = 0.0f;
        for (const Body2D& body : physics.Bodies())
        {
            total += body.Position.Get().X * 3.0f + body.Position.Get().Y + body.Rotation.Get();
        }
        return total;
    };
    EASYFORGE_EXPECT_EQUAL(run(), run());
}
