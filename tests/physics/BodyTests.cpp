#include <cmath>
#include <numbers>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/physics.h>

#include "PhysicsTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(BodiesFallUnderGravity)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, -10.0f } });
    EASYFORGE_REQUIRE(physics);
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 100.0f }, .Radius = 0.5f });
    EASYFORGE_REQUIRE(ball);
    Run(physics, 1.0f);
    // Half of g t squared, give or take what stepping in substeps adds.
    EASYFORGE_EXPECT_NEAR(ball.Position.Get().Y, 95.0f, 0.05f);
    EASYFORGE_EXPECT_NEAR(ball.Velocity.Get().Y, -10.0f, 0.01f);
    EASYFORGE_EXPECT_EQUAL(ball.Position.Get().X, 0.0f);

    // Gravity can change, and each body can feel more or less of it.
    physics.Gravity = Vector2 { 0.0f, 0.0f };
    ball.Velocity = Vector2 { 1.0f, 0.0f };
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(ball.Position.Get().X, 1.0f, 1e-4f);
    Body2D floating = physics.AddBox({ .Position = { 5.0f, 5.0f }, .GravityScale = 0.0f });
    physics.Gravity = Vector2 { 0.0f, -10.0f };
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(floating.Position.Get().Y, 5.0f, 1e-6f);
}

EASYFORGE_TEST(ABoxRestsOnTheGround)
{
    Physics2D physics = Physics2D::New();
    Ground(physics);
    Body2D box = physics.AddBox({ .Position = { 0.0f, 0.5f }, .Size = { 1.0f, 1.0f } });
    Run(physics, 3.0f);
    EASYFORGE_EXPECT_NEAR(box.Position.Get().Y, 0.5f, 0.01f);
    EASYFORGE_EXPECT_NEAR(box.Position.Get().X, 0.0f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(box.Rotation.Get(), 0.0f, 1e-4f);
    EASYFORGE_EXPECT(Speed(box) < 0.01f);
}

EASYFORGE_TEST(AStackOfTenSettlesAndStays)
{
    Physics2D physics = Physics2D::New();
    Ground(physics);
    std::vector<Body2D> boxes;
    for (int level = 0; level < 10; ++level)
    {
        boxes.push_back(physics.AddBox({ .Position = { 0.0f, 0.5f + static_cast<float>(level) * 1.02f }, .Size = { 1.0f, 1.0f } }));
    }
    Run(physics, 10.0f);
    for (int level = 0; level < 10; ++level)
    {
        const Body2D& box = boxes[static_cast<std::size_t>(level)];
        EASYFORGE_EXPECT_NEAR(box.Position.Get().X, 0.0f, 0.01f);
        EASYFORGE_EXPECT_NEAR(box.Position.Get().Y, 0.5f + static_cast<float>(level), 0.05f);
        EASYFORGE_EXPECT_NEAR(box.Rotation.Get(), 0.0f, 0.01f);
        EASYFORGE_EXPECT(Speed(box) < 0.01f);
    }
}

EASYFORGE_TEST(APyramidStands)
{
    Physics2D physics = Physics2D::New();
    Ground(physics);
    std::vector<Body2D> boxes;
    constexpr int base = 10;
    for (int row = 0; row < base; ++row)
    {
        for (int column = 0; column < base - row; ++column)
        {
            float x = (static_cast<float>(column) - static_cast<float>(base - row - 1) * 0.5f) * 1.05f;
            boxes.push_back(physics.AddBox({ .Position = { x, 0.5f + static_cast<float>(row) * 1.02f }, .Size = { 1.0f, 1.0f } }));
        }
    }
    Run(physics, 10.0f);
    float fastest = 0.0f;
    for (const Body2D& box : boxes)
    {
        fastest = std::max(fastest, Speed(box));
    }
    EASYFORGE_EXPECT(fastest < 0.02f);
    EASYFORGE_EXPECT_NEAR(boxes.back().Position.Get().Y, 0.5f + (base - 1), 0.1f);
    EASYFORGE_EXPECT_NEAR(boxes.back().Position.Get().X, 0.0f, 0.05f);
}

EASYFORGE_TEST(FrictionHoldsOnASlope)
{
    constexpr float slope = 20.0f * std::numbers::pi_v<float> / 180.0f;
    Physics2D physics = Physics2D::New();
    physics.AddBox({ .Rotation = slope, .Size = { 30.0f, 1.0f }, .Type = BodyType::Static });
    Vector2 surface { -std::sin(slope) * 0.5f, std::cos(slope) * 0.5f };
    Vector2 up = surface * 2.0f;
    Body2D gripping = physics.AddBox({ .Position = surface + up * 0.5f, .Rotation = slope, .Friction = 0.6f });
    // Down the slope from the first, so it slides away from it.
    Body2D slipping = physics.AddBox({ .Position = Vector2 { std::cos(slope), std::sin(slope) } * -6.0f + surface + up * 0.5f, .Rotation = slope, .Friction = 0.0f });
    Vector2 grippingStart = gripping.Position;
    Vector2 slippingStart = slipping.Position;
    Run(physics, 2.0f);
    // tan 20 degrees is below 0.6, so the box stays; without friction it slides.
    EASYFORGE_EXPECT(Distance(gripping.Position.Get(), grippingStart) < 0.05f);
    EASYFORGE_EXPECT(Distance(slipping.Position.Get(), slippingStart) > 3.0f);
}

EASYFORGE_TEST(RestitutionBounces)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, -10.0f } });
    Ground(physics);
    Body2D lively = physics.AddCircle({ .Position = { -5.0f, 5.5f }, .Radius = 0.5f, .Restitution = 1.0f });
    Body2D dull = physics.AddCircle({ .Position = { 5.0f, 5.5f }, .Radius = 0.5f, .Restitution = 0.5f });
    float livelyPeak = 0.0f;
    float dullPeak = 0.0f;
    bool livelyLanded = false;
    bool dullLanded = false;
    for (int step = 0; step < 240; ++step)
    {
        physics.Step(StepSeconds);
        livelyLanded = livelyLanded || lively.Velocity.Get().Y > 0.0f;
        dullLanded = dullLanded || dull.Velocity.Get().Y > 0.0f;
        if (livelyLanded)
        {
            livelyPeak = std::max(livelyPeak, lively.Position.Get().Y - 0.5f);
        }
        if (dullLanded)
        {
            dullPeak = std::max(dullPeak, dull.Position.Get().Y - 0.5f);
        }
    }
    // A perfect bounce returns to its height; half the speed, a quarter of it.
    EASYFORGE_EXPECT(livelyPeak > 4.6f && livelyPeak < 5.2f);
    EASYFORGE_EXPECT(dullPeak > 1.0f && dullPeak < 1.6f);
}

EASYFORGE_TEST(KinematicBodiesPushButAreNotPushed)
{
    Physics2D physics = Physics2D::New();
    Body2D ground = physics.AddBox({ .Position = { 0.0f, -0.5f }, .Size = { 40.0f, 1.0f }, .Type = BodyType::Static, .Friction = 0.0f });
    Body2D pusher = physics.AddBox({ .Position = { -3.0f, 0.5f }, .Type = BodyType::Kinematic, .Velocity = { 2.0f, 0.0f } });
    Body2D crate = physics.AddBox({ .Position = { 0.0f, 0.5f }, .Friction = 0.0f });
    Run(physics, 3.0f);
    EASYFORGE_EXPECT_NEAR(pusher.Position.Get().X, 3.0f, 1e-3f);
    EASYFORGE_EXPECT_NEAR(pusher.Velocity.Get().X, 2.0f, 1e-6f);
    EASYFORGE_EXPECT(crate.Position.Get().X > pusher.Position.Get().X + 0.9f);
    EASYFORGE_EXPECT(ground.Velocity.Get() == (Vector2 {}));
}

EASYFORGE_TEST(ForcesAndImpulsesMoveBodies)
{
    Physics2D physics = Physics2D::New({ .Gravity = {} });
    Body2D box = physics.AddBox({ .Size = { 2.0f, 1.0f } });
    EASYFORGE_EXPECT_NEAR(box.Mass(), 2.0f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(box.Inertia(), 2.0f * (4.0f + 1.0f) / 12.0f, 1e-5f);

    box.ApplyImpulse({ 4.0f, 0.0f });
    EASYFORGE_EXPECT_NEAR(box.Velocity.Get().X, 2.0f, 1e-5f);
    box.ApplyAngularImpulse(box.Inertia());
    EASYFORGE_EXPECT_NEAR(box.AngularVelocity.Get(), 1.0f, 1e-5f);

    // An impulse off center also turns the body.
    box.Velocity = Vector2 {};
    box.AngularVelocity = 0.0f;
    box.ApplyImpulse({ 0.0f, 1.0f }, box.Center() + Vector2 { 1.0f, 0.0f });
    EASYFORGE_EXPECT(box.AngularVelocity.Get() > 0.0f);

    // A force acts for one step and is then cleared.
    box.Velocity = Vector2 {};
    box.AngularVelocity = 0.0f;
    box.ApplyForce({ 120.0f, 0.0f });
    physics.Step(StepSeconds);
    EASYFORGE_EXPECT_NEAR(box.Velocity.Get().X, 1.0f, 1e-4f);
    physics.Step(StepSeconds);
    EASYFORGE_EXPECT_NEAR(box.Velocity.Get().X, 1.0f, 1e-4f);
    box.ApplyTorque(box.Inertia() * 60.0f);
    physics.Step(StepSeconds);
    EASYFORGE_EXPECT_NEAR(box.AngularVelocity.Get(), 1.0f, 1e-4f);

    // Static bodies ignore them.
    Body2D wall = physics.AddBox({ .Type = BodyType::Static });
    wall.ApplyImpulse({ 5.0f, 5.0f });
    wall.Velocity = Vector2 { 1.0f, 0.0f };
    EASYFORGE_EXPECT(wall.Velocity.Get() == (Vector2 {}));
    EASYFORGE_EXPECT_EQUAL(wall.Mass(), 0.0f);

    Body2D ball = physics.AddCircle({ .Radius = 1.0f });
    EASYFORGE_EXPECT_NEAR(ball.Mass(), std::numbers::pi_v<float>, 1e-5f);
    EASYFORGE_EXPECT_NEAR(ball.Inertia(), std::numbers::pi_v<float> * 0.5f, 1e-5f);
}

EASYFORGE_TEST(ShapesDescribeThemselves)
{
    Physics2D physics = Physics2D::New();
    Body2D box = physics.AddBox({ .Position = { 1.0f, 2.0f }, .Size = { 2.0f, 4.0f }, .Rounding = 0.1f });
    EASYFORGE_EXPECT(box.Shape() == ShapeKind2D::Box);
    EASYFORGE_EXPECT(box.Size() == (Vector2 { 2.0f, 4.0f }));
    EASYFORGE_EXPECT_NEAR(box.Radius(), 0.1f, 1e-6f);
    EASYFORGE_EXPECT_EQUAL(box.Points().size(), std::size_t(4));
    Rectangle bounds = box.Bounds();
    EASYFORGE_EXPECT_NEAR(bounds.X, -0.1f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(bounds.Width, 2.2f, 1e-5f);

    Body2D capsule = physics.AddCapsule({ .Length = 2.0f, .Radius = 0.5f });
    EASYFORGE_EXPECT(capsule.Shape() == ShapeKind2D::Capsule);
    EASYFORGE_EXPECT_NEAR(capsule.Length(), 2.0f, 1e-6f);
    EASYFORGE_EXPECT(capsule.Size() == (Vector2 { 3.0f, 1.0f }));

    // A polygon keeps the outline of its points: the inner one and the one on a
    // straight edge are left out.
    Body2D polygon = physics.AddPolygon({ .Points = { { 0, 0 }, { 2, 0 }, { 1, 0 }, { 2, 2 }, { 0, 2 }, { 1, 1 } } });
    EASYFORGE_REQUIRE(polygon);
    std::vector<Vector2> corners = polygon.Points();
    EASYFORGE_EXPECT_EQUAL(corners.size(), std::size_t(4));
    float area = 0.0f;
    for (std::size_t index = 0; index < corners.size(); ++index)
    {
        area += Cross(corners[index], corners[(index + 1) % corners.size()]) * 0.5f;
    }
    EASYFORGE_EXPECT_NEAR(area, 4.0f, 1e-5f);   // counterclockwise, so positive
    EASYFORGE_EXPECT_NEAR(polygon.Mass(), 4.0f, 1e-5f);
    EASYFORGE_EXPECT(Distance(polygon.Center(), Vector2 { 1.0f, 1.0f }) < 1e-5f);

    // Many points keep at most eight corners; points in a line make no shape.
    std::vector<Vector2> circle;
    for (int index = 0; index < 20; ++index)
    {
        float angle = static_cast<float>(index) * 2.0f * std::numbers::pi_v<float> / 20.0f;
        circle.push_back({ std::cos(angle), std::sin(angle) });
    }
    EASYFORGE_EXPECT_EQUAL(physics.AddPolygon({ .Points = circle }).Points().size(), std::size_t(8));
    EASYFORGE_EXPECT(!physics.AddPolygon({ .Points = { { 0, 0 }, { 1, 1 }, { 2, 2 } } }));
    EASYFORGE_EXPECT_EQUAL(physics.BodyCount(), std::size_t(4));
}

EASYFORGE_TEST(BodiesCanBeChangedAndRemoved)
{
    Physics2D physics = Physics2D::New();
    Ground(physics);
    Body2D box = physics.AddBox({ .Position = { 0.0f, 5.0f } });
    Body2D copy = box;
    EASYFORGE_EXPECT(copy == box);
    EASYFORGE_EXPECT(box.Id() != 0);

    // Moved by hand, it lands where it was put.
    box.Position = Vector2 { 3.0f, 0.5f };
    box.Rotation = 0.0f;
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(box.Position.Get().X, 3.0f, 1e-3f);
    EASYFORGE_EXPECT_NEAR(box.Position.Get().Y, 0.5f, 0.01f);

    // Made static, it stops; made dynamic again, it falls.
    box.Position = Vector2 { 3.0f, 4.0f };
    box.Type = BodyType::Static;
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(box.Position.Get().Y, 4.0f, 1e-6f);
    box.Type = BodyType::Dynamic;
    EASYFORGE_EXPECT(box.Mass() > 0.0f);
    Run(physics, 2.0f);
    EASYFORGE_EXPECT_NEAR(box.Position.Get().Y, 0.5f, 0.01f);

    // Turning can be switched off.
    box.FixedRotation = true;
    box.AngularVelocity = 3.0f;
    EASYFORGE_EXPECT_EQUAL(box.AngularVelocity.Get(), 0.0f);
    EASYFORGE_EXPECT(box.FixedRotation.Get());

    // Removed, every handle to it lets go.
    int separations = 0;
    Body2D lid = physics.AddBox({ .Position = { 3.0f, 1.5f } });
    Run(physics, 1.0f);
    lid.OnSeparate = [&](const Contact2D& contact) {
        ++separations;
        EASYFORGE_EXPECT(!contact.Other);
    };
    box.Remove();
    EASYFORGE_EXPECT(!box);
    EASYFORGE_EXPECT(!copy);
    EASYFORGE_EXPECT_EQUAL(separations, 1);
    box.Position = Vector2 { 1.0f, 1.0f };
    EASYFORGE_EXPECT(box.Position.Get() == (Vector2 {}));
    EASYFORGE_EXPECT_EQUAL(physics.BodyCount(), std::size_t(2));

    // A new body may take the old one's place, but never its handles.
    Body2D next = physics.AddCircle({ .Position = { -3.0f, 3.0f } });
    EASYFORGE_EXPECT(next);
    EASYFORGE_EXPECT(!box);
    EASYFORGE_EXPECT(next.Id() != copy.Id());

    // Handles outlive their world harmlessly.
    Body2D orphan;
    {
        Physics2D brief = Physics2D::New();
        orphan = brief.AddBox({});
        EASYFORGE_EXPECT(orphan);
    }
    EASYFORGE_EXPECT(!orphan);
    orphan.Velocity = Vector2 { 1.0f, 1.0f };
    Physics2D none;
    EASYFORGE_EXPECT(!none);
    EASYFORGE_EXPECT(!none.AddBox({}));
    none.Step(1.0f);
}

EASYFORGE_TEST(TheSameStepsGiveTheSameWorld)
{
    auto build = [] {
        Physics2D physics = Physics2D::New();
        Ground(physics);
        Random random(42);
        for (int index = 0; index < 60; ++index)
        {
            Vector2 place { random.Between(-6.0f, 6.0f), random.Between(1.0f, 30.0f) };
            if (index % 3 == 0)
            {
                physics.AddCircle({ .Position = place, .Radius = random.Between(0.2f, 0.6f) });
            }
            else if (index % 3 == 1)
            {
                physics.AddBox({ .Position = place, .Rotation = random.Between(0.0f, 3.0f), .Size = { random.Between(0.3f, 1.2f), random.Between(0.3f, 1.2f) } });
            }
            else
            {
                physics.AddCapsule({ .Position = place, .Rotation = random.Between(0.0f, 3.0f), .Length = random.Between(0.3f, 1.0f), .Radius = 0.25f });
            }
        }
        Run(physics, 6.0f);
        return physics;
    };
    Physics2D first = build();
    Physics2D second = build();
    std::vector<Body2D> firstBodies = first.Bodies();
    std::vector<Body2D> secondBodies = second.Bodies();
    EASYFORGE_REQUIRE(firstBodies.size() == secondBodies.size());
    bool identical = true;
    for (std::size_t index = 0; index < firstBodies.size(); ++index)
    {
        identical = identical && firstBodies[index].Position.Get() == secondBodies[index].Position.Get() &&
                    firstBodies[index].Rotation.Get() == secondBodies[index].Rotation.Get();
    }
    EASYFORGE_EXPECT(identical);
}
