#include <cmath>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/physics.h>

#include "PhysicsTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(TouchesAreReported)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, -10.0f } });
    Body2D ground = Ground(physics);
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 5.5f }, .Radius = 0.5f });
    std::vector<Contact2D> ballTouches;
    std::vector<Contact2D> groundTouches;
    ball.OnTouch = [&](const Contact2D& contact) { ballTouches.push_back(contact); };
    ground.OnTouch = [&](const Contact2D& contact) { groundTouches.push_back(contact); };
    Run(physics, 2.0f);

    EASYFORGE_REQUIRE(ballTouches.size() == 1);
    EASYFORGE_REQUIRE(groundTouches.size() == 1);
    const Contact2D& touch = ballTouches[0];
    EASYFORGE_EXPECT(touch.Other == ground);
    EASYFORGE_EXPECT(groundTouches[0].Other == ball);
    // From the ball toward the ground, and the other way for the ground.
    EASYFORGE_EXPECT_NEAR(touch.Normal.Y, -1.0f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(groundTouches[0].Normal.Y, 1.0f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(touch.Point.Y, 0.0f, 0.05f);
    // Falling five metres, about ten metres a second.
    EASYFORGE_EXPECT(touch.Speed > 9.0f && touch.Speed < 10.5f);
    EASYFORGE_EXPECT(!touch.Sensor);

    // Lifted away, they separate.
    int separations = 0;
    ball.OnSeparate = [&](const Contact2D&) { ++separations; };
    ball.Position = Vector2 { 0.0f, 3.0f };
    physics.Step(StepSeconds);
    EASYFORGE_EXPECT_EQUAL(separations, 1);
}

EASYFORGE_TEST(SensorsReportButDoNotCollide)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, -10.0f } });
    Ground(physics);
    Body2D zone = physics.AddBox({ .Position = { 0.0f, 3.0f }, .Size = { 4.0f, 1.0f }, .Type = BodyType::Static, .Sensor = true });
    EASYFORGE_EXPECT(zone.IsSensor());
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 6.0f }, .Radius = 0.25f });
    int entered = 0;
    int left = 0;
    zone.OnTouch = [&](const Contact2D& contact) {
        ++entered;
        EASYFORGE_EXPECT(contact.Sensor);
        EASYFORGE_EXPECT(contact.Other == ball);
    };
    zone.OnSeparate = [&](const Contact2D&) { ++left; };
    Run(physics, 3.0f);
    EASYFORGE_EXPECT_EQUAL(entered, 1);
    EASYFORGE_EXPECT_EQUAL(left, 1);
    // It fell through the sensor to the ground.
    EASYFORGE_EXPECT_NEAR(ball.Position.Get().Y, 0.25f, 0.01f);
}

EASYFORGE_TEST(LayersChooseWhatCollides)
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0.0f, -10.0f } });
    // The ground is on layer 1; ghosts are on layer 2 and collide only with layer 2.
    Ground(physics);
    Body2D solid = physics.AddBox({ .Position = { -2.0f, 3.0f } });
    Body2D ghost = physics.AddBox({ .Position = { 2.0f, 3.0f }, .Layer = 2, .CollidesWith = 2 });
    Run(physics, 2.0f);
    EASYFORGE_EXPECT_NEAR(solid.Position.Get().Y, 0.5f, 0.01f);
    EASYFORGE_EXPECT(ghost.Position.Get().Y < -5.0f);

    // Changing the layers takes effect at once.
    ghost.Position = Vector2 { 2.0f, 3.0f };
    ghost.Velocity = Vector2 {};
    ghost.CollidesWith = AllLayers;
    ghost.Layer = 1;
    Run(physics, 2.0f);
    EASYFORGE_EXPECT_NEAR(ghost.Position.Get().Y, 0.5f, 0.01f);
}

EASYFORGE_TEST(EveryShapeRestsOnEveryOther)
{
    Physics2D physics = Physics2D::New();
    Ground(physics, 90.0f);
    struct Pair
    {
        Body2D Lower;
        Body2D Upper;
    };
    std::vector<Pair> pairs;
    // Three metres apart, so neighbours never touch: a capsule is two wide.
    float x = -37.0f;
    auto make = [&](int kind, float y) -> Body2D {
        switch (kind)
        {
        case 0: return physics.AddCircle({ .Position = { x, y }, .Radius = 0.5f });
        case 1: return physics.AddBox({ .Position = { x, y }, .Size = { 1.0f, 1.0f } });
        case 2: return physics.AddCapsule({ .Position = { x, y }, .Length = 1.0f, .Radius = 0.5f, .FixedRotation = true });
        case 3: return physics.AddPolygon({ .Position = { x, y }, .Points = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.6f, 0.2f }, { 0.0f, 0.5f }, { -0.6f, 0.2f } } });
        default: return physics.AddBox({ .Position = { x, y }, .Size = { 0.8f, 0.8f }, .Rounding = 0.1f });
        }
    };
    for (int lower = 0; lower < 5; ++lower)
    {
        for (int upper = 0; upper < 5; ++upper)
        {
            // Lower shapes are held still, so only the contact between the two is tested.
            Body2D base = make(lower, 0.5f);
            base.FixedRotation = true;
            Body2D top = make(upper, 2.0f);
            top.FixedRotation = true;
            pairs.push_back({ base, top });
            x += 3.0f;
        }
    }
    Run(physics, 3.0f);
    for (const Pair& pair : pairs)
    {
        // The upper shape sits on the lower, neither falling through nor sinking in.
        Rectangle lower = pair.Lower.Bounds();
        Rectangle upper = pair.Upper.Bounds();
        EASYFORGE_EXPECT_NEAR(upper.Y, lower.Y + lower.Height, 0.02f);
        EASYFORGE_EXPECT_NEAR(lower.Y, 0.0f, 0.02f);
        EASYFORGE_EXPECT(Speed(pair.Upper) < 0.01f);
    }
}

EASYFORGE_TEST(RaysFindTheFirstBody)
{
    Physics2D physics = Physics2D::New();
    Body2D ground = Ground(physics);
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 3.0f }, .Radius = 1.0f, .Type = BodyType::Static });
    Body2D capsule = physics.AddCapsule({ .Position = { 6.0f, 3.0f }, .Length = 2.0f, .Radius = 0.5f, .Type = BodyType::Static });
    Body2D rounded = physics.AddBox({ .Position = { -6.0f, 3.0f }, .Size = { 2.0f, 2.0f }, .Rounding = 0.5f, .Type = BodyType::Static });
    Body2D sensor = physics.AddBox({ .Position = { 0.0f, 8.0f }, .Size = { 4.0f, 1.0f }, .Type = BodyType::Static, .Sensor = true });

    // Straight down: through the sensor, onto the ball.
    RayHit2D hit = physics.CastRay({ 0.0f, 10.0f }, { 0.0f, -1.0f }, 20.0f);
    EASYFORGE_REQUIRE(hit);
    EASYFORGE_EXPECT(hit.Body == ball);
    EASYFORGE_EXPECT_NEAR(hit.Distance, 6.0f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(hit.Point.Y, 4.0f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(hit.Normal.Y, 1.0f, 1e-4f);

    // Beside the ball, the ground; too short, nothing.
    hit = physics.CastRay({ 2.0f, 10.0f }, { 0.0f, -1.0f }, 20.0f);
    EASYFORGE_EXPECT(hit.Body == ground);
    EASYFORGE_EXPECT_NEAR(hit.Point.Y, 0.0f, 1e-4f);
    EASYFORGE_EXPECT(!physics.CastRay({ 2.0f, 10.0f }, { 0.0f, -1.0f }, 5.0f));

    // A capsule's round end, from the side.
    hit = physics.CastRay({ 10.0f, 3.0f }, { -1.0f, 0.0f }, 20.0f);
    EASYFORGE_EXPECT(hit.Body == capsule);
    EASYFORGE_EXPECT_NEAR(hit.Point.X, 7.5f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(hit.Normal.X, 1.0f, 1e-4f);
    hit = physics.CastRay({ 6.0f, 10.0f }, { 0.0f, -1.0f }, 20.0f);
    EASYFORGE_EXPECT_NEAR(hit.Point.Y, 3.5f, 1e-4f);

    // A rounded corner, along the diagonal: the circle around the corner.
    hit = physics.CastRay({ -10.0f, 7.0f }, { 1.0f, -1.0f }, 20.0f);
    EASYFORGE_EXPECT(hit.Body == rounded);
    Vector2 corner { -7.0f, 4.0f };
    EASYFORGE_EXPECT_NEAR(Distance(hit.Point, corner), 0.5f, 1e-3f);
    EASYFORGE_EXPECT_NEAR(hit.Normal.X, -std::sqrt(0.5f), 1e-3f);

    // From inside a body, that body is not hit; layers leave bodies out.
    hit = physics.CastRay({ 0.0f, 3.0f }, { 0.0f, -1.0f }, 20.0f);
    EASYFORGE_EXPECT(hit.Body == ground);
    EASYFORGE_EXPECT(!physics.CastRay({ 0.0f, 10.0f }, { 0.0f, -1.0f }, 20.0f, 2));
    EASYFORGE_EXPECT(!physics.CastRay({ 0.0f, 10.0f }, { 0.0f, 0.0f }, 20.0f));

    // Sensors are never hit, even straight on.
    hit = physics.CastRay({ -5.0f, 8.0f }, { 1.0f, 0.0f }, 4.0f);
    EASYFORGE_EXPECT(sensor.IsSensor());
    EASYFORGE_EXPECT(!hit);
}

EASYFORGE_TEST(CirclesCanBeCast)
{
    Physics2D physics = Physics2D::New();
    Body2D ground = Ground(physics);
    Body2D post = physics.AddBox({ .Position = { 4.0f, 2.0f }, .Size = { 1.0f, 4.0f }, .Type = BodyType::Static });

    RayHit2D hit = physics.CastCircle({ 0.0f, 5.0f }, 0.5f, { 0.0f, -1.0f }, 10.0f);
    EASYFORGE_REQUIRE(hit);
    EASYFORGE_EXPECT(hit.Body == ground);
    EASYFORGE_EXPECT_NEAR(hit.Distance, 4.5f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(hit.Point.Y, 0.0f, 1e-4f);

    // Sideways into the post's face.
    hit = physics.CastCircle({ 0.0f, 2.0f }, 0.5f, { 1.0f, 0.0f }, 10.0f);
    EASYFORGE_EXPECT(hit.Body == post);
    EASYFORGE_EXPECT_NEAR(hit.Distance, 3.0f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(hit.Point.X, 3.5f, 1e-4f);
    EASYFORGE_EXPECT_NEAR(hit.Normal.X, -1.0f, 1e-4f);

    // Over the top of the post, just clearing its corner.
    EASYFORGE_EXPECT(!physics.CastCircle({ 0.0f, 4.51f }, 0.5f, { 1.0f, 0.0f }, 10.0f));
    hit = physics.CastCircle({ 0.0f, 4.4f }, 0.5f, { 1.0f, 0.0f }, 10.0f);
    EASYFORGE_EXPECT(hit.Body == post);
    EASYFORGE_EXPECT(hit.Distance > 3.0f);
}

EASYFORGE_TEST(BodiesAreFoundByPlace)
{
    Physics2D physics = Physics2D::New({ .Gravity = {} });
    Body2D left = physics.AddBox({ .Position = { -2.0f, 0.0f } });
    Body2D right = physics.AddCircle({ .Position = { 2.0f, 0.0f }, .Radius = 0.5f });
    Body2D tilted = physics.AddBox({ .Position = { 0.0f, 3.0f }, .Rotation = 0.7853982f, .Size = { 1.0f, 1.0f }, .Layer = 4 });

    std::vector<Body2D> at = physics.BodiesAt({ 2.2f, 0.1f });
    EASYFORGE_REQUIRE(at.size() == 1);
    EASYFORGE_EXPECT(at[0] == right);
    EASYFORGE_EXPECT(physics.BodiesAt({ 2.45f, 0.45f }).empty());   // inside the box around it, outside the circle
    EASYFORGE_EXPECT(physics.BodiesAt({ 0.6f, 3.0f }).size() == 1);  // the tilted box's corner reaches here
    EASYFORGE_EXPECT(physics.BodiesAt({ 0.6f, 3.0f }, 1).empty());

    std::vector<Body2D> in = physics.BodiesIn({ -3.0f, -1.0f, 5.6f, 2.0f });
    EASYFORGE_REQUIRE(in.size() == 2);
    EASYFORGE_EXPECT(in[0] == left);
    EASYFORGE_EXPECT(in[1] == right);
    EASYFORGE_EXPECT(physics.BodiesIn({ 0.55f, 2.45f, 0.1f, 0.1f }).empty());   // past the tilted box's edge
    EASYFORGE_EXPECT(physics.BodiesIn({ -10.0f, -10.0f, 20.0f, 20.0f }).size() == 3);
    EASYFORGE_EXPECT_EQUAL(physics.Bodies().size(), std::size_t(3));
}

EASYFORGE_TEST(ACrowdFindsEveryContact)
{
    // Hundreds of circles dropped into a box; if the tree missed a pair, some
    // would end up sunk into each other.
    Physics2D physics = Physics2D::New();
    Ground(physics, 22.0f);
    physics.AddBox({ .Position = { -10.5f, 10.0f }, .Size = { 1.0f, 20.0f }, .Type = BodyType::Static });
    physics.AddBox({ .Position = { 10.5f, 10.0f }, .Size = { 1.0f, 20.0f }, .Type = BodyType::Static });
    std::vector<Body2D> circles;
    Random random(7);
    for (int index = 0; index < 300; ++index)
    {
        circles.push_back(physics.AddCircle({ .Position = { random.Between(-9.5f, 9.5f), random.Between(1.0f, 40.0f) }, .Radius = 0.3f }));
    }
    Run(physics, 8.0f);
    float deepest = 0.0f;
    for (std::size_t first = 0; first < circles.size(); ++first)
    {
        for (std::size_t second = first + 1; second < circles.size(); ++second)
        {
            float overlap = 0.6f - Distance(circles[first].Position.Get(), circles[second].Position.Get());
            deepest = std::max(deepest, overlap);
        }
    }
    EASYFORGE_EXPECT(deepest < 0.03f);
    for (const Body2D& circle : circles)
    {
        EASYFORGE_EXPECT(circle.Position.Get().Y > 0.25f);
    }
}
