#include <cmath>
#include <numbers>

#include <easyforge/core/Testing.h>
#include <easyforge/physics.h>

#include "PhysicsTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(RodsKeepTheirLength)
{
    Physics2D physics = Physics2D::New();
    Body2D pin = physics.AddBox({ .Position = { 0.0f, 10.0f }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D ball = physics.AddCircle({ .Position = { 3.0f, 10.0f }, .Radius = 0.25f });
    Joint2D rod = physics.AddDistanceJoint({ .First = pin, .Second = ball, .FirstAnchor = { 0.0f, 10.0f }, .SecondAnchor = { 3.0f, 10.0f } });
    EASYFORGE_REQUIRE(rod);
    EASYFORGE_EXPECT_NEAR(rod.Length.Get(), 3.0f, 1e-5f);
    EASYFORGE_EXPECT(rod.First() == pin);
    EASYFORGE_EXPECT(rod.Second() == ball);

    float worst = 0.0f;
    float lowest = 10.0f;
    for (int step = 0; step < 180; ++step)
    {
        physics.Step(StepSeconds);
        worst = std::max(worst, std::fabs(Distance(ball.Position.Get(), Vector2 { 0.0f, 10.0f }) - 3.0f));
        lowest = std::min(lowest, ball.Position.Get().Y);
    }
    EASYFORGE_EXPECT(worst < 0.02f);
    EASYFORGE_EXPECT(lowest < 7.1f);   // it swung through the bottom

    // A shorter rod pulls the ball in.
    rod.Length = 2.0f;
    Run(physics, 2.0f);
    EASYFORGE_EXPECT_NEAR(Distance(ball.Position.Get(), Vector2 { 0.0f, 10.0f }), 2.0f, 0.02f);
}

EASYFORGE_TEST(SpringsStretchAndSettle)
{
    Physics2D physics = Physics2D::New();
    Body2D pin = physics.AddBox({ .Position = { 0.0f, 10.0f }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D weight = physics.AddBox({ .Position = { 0.0f, 8.0f }, .Size = { 1.0f, 1.0f }, .FixedRotation = true });
    physics.AddDistanceJoint({
        .First = pin,
        .Second = weight,
        .FirstAnchor = { 0.0f, 10.0f },
        .SecondAnchor = { 0.0f, 8.0f },
        .Length = 2.0f,
        .Stiffness = 2.0f,
        .Damping = 0.7f,
    });
    Run(physics, 6.0f);
    // A spring of 2 hertz on one kilogram stretches by g / (2 pi 2)^2.
    float omega = 2.0f * std::numbers::pi_v<float> * 2.0f;
    float stretch = 9.8f / (omega * omega);
    EASYFORGE_EXPECT_NEAR(10.0f - weight.Position.Get().Y, 2.0f + stretch, 0.02f);
    EASYFORGE_EXPECT(Speed(weight) < 0.01f);
}

EASYFORGE_TEST(HingesPinAndDrive)
{
    Physics2D physics = Physics2D::New();
    Body2D frame = physics.AddBox({ .Position = { 0.0f, 5.0f }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D wheel = physics.AddCircle({ .Position = { 0.0f, 5.0f }, .Radius = 1.0f });
    Joint2D axle = physics.AddHingeJoint({
        .First = frame,
        .Second = wheel,
        .Anchor = { 0.0f, 5.0f },
        .Motor = true,
        .MotorSpeed = 3.0f,
        .MaximumMotorTorque = 1000.0f,
    });
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(wheel.AngularVelocity.Get(), 3.0f, 0.01f);
    EASYFORGE_EXPECT(Distance(wheel.Position.Get(), Vector2 { 0.0f, 5.0f }) < 0.01f);

    axle.MotorSpeed = -1.0f;
    EASYFORGE_EXPECT_EQUAL(axle.MotorSpeed.Get(), -1.0f);
    Run(physics, 0.5f);
    EASYFORGE_EXPECT_NEAR(wheel.AngularVelocity.Get(), -1.0f, 0.01f);
}

EASYFORGE_TEST(HingeLimitsHold)
{
    Physics2D physics = Physics2D::New();
    Body2D wall = physics.AddBox({ .Position = { -0.5f, 5.0f }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D plank = physics.AddBox({ .Position = { 1.0f, 5.0f }, .Size = { 2.0f, 0.2f } });
    Joint2D hinge = physics.AddHingeJoint({
        .First = wall,
        .Second = plank,
        .Anchor = { 0.0f, 5.0f },
        .Limit = true,
        .LowerAngle = -0.5f,
        .UpperAngle = 0.25f,
    });
    EASYFORGE_EXPECT_NEAR(hinge.Angle(), 0.0f, 1e-5f);
    Run(physics, 3.0f);
    EASYFORGE_EXPECT_NEAR(hinge.Angle(), -0.5f, 0.02f);
    EASYFORGE_EXPECT_NEAR(plank.Rotation.Get(), -0.5f, 0.02f);
}

EASYFORGE_TEST(SlidersKeepToTheirAxis)
{
    Physics2D physics = Physics2D::New();
    Body2D rail = physics.AddBox({ .Position = { 0.0f, 5.0f }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D car = physics.AddBox({ .Position = { 0.0f, 5.0f }, .Size = { 1.0f, 0.5f } });
    Joint2D slider = physics.AddSliderJoint({
        .First = rail,
        .Second = car,
        .Anchor = { 0.0f, 5.0f },
        .Axis = { 1.0f, 0.0f },
        .Motor = true,
        .MotorSpeed = 1.0f,
        .MaximumMotorForce = 100.0f,
    });
    Run(physics, 2.0f);
    // Gravity cannot pull it off the rail, and it does not turn.
    EASYFORGE_EXPECT_NEAR(car.Position.Get().Y, 5.0f, 0.01f);
    EASYFORGE_EXPECT_NEAR(car.Position.Get().X, 2.0f, 0.05f);
    EASYFORGE_EXPECT_NEAR(slider.Translation(), car.Position.Get().X, 1e-4f);
    EASYFORGE_EXPECT_NEAR(car.Rotation.Get(), 0.0f, 0.01f);

    // With limits, it stops at the end of its travel.
    Body2D lift = physics.AddBox({ .Position = { 5.0f, 2.0f }, .Size = { 1.0f, 0.2f } });
    Joint2D shaft = physics.AddSliderJoint({
        .First = rail,
        .Second = lift,
        .Anchor = { 5.0f, 2.0f },
        .Axis = { 0.0f, 1.0f },
        .Limit = true,
        .Lower = -1.0f,
        .Upper = 1.5f,
        .Motor = true,
        .MotorSpeed = 2.0f,
        .MaximumMotorForce = 100.0f,
    });
    Run(physics, 2.0f);
    EASYFORGE_EXPECT_NEAR(shaft.Translation(), 1.5f, 0.02f);
    EASYFORGE_EXPECT_NEAR(lift.Position.Get().X, 5.0f, 0.01f);
    // A motor at speed 0 holds the lift where it is, against gravity.
    shaft.MotorSpeed = 0.0f;
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(shaft.Translation(), 1.5f, 0.02f);
    EASYFORGE_EXPECT(Speed(lift) < 0.01f);
}

EASYFORGE_TEST(WeldsHoldBodiesTogether)
{
    Physics2D physics = Physics2D::New();
    Body2D wall = physics.AddBox({ .Position = { 0.0f, 5.0f }, .Size = { 1.0f, 1.0f }, .Type = BodyType::Static });
    Body2D arm = physics.AddBox({ .Position = { 1.5f, 5.0f }, .Size = { 2.0f, 0.4f } });
    physics.AddWeldJoint({ .First = wall, .Second = arm, .Anchor = { 0.5f, 5.0f } });
    Run(physics, 2.0f);
    EASYFORGE_EXPECT(Distance(arm.Position.Get(), Vector2 { 1.5f, 5.0f }) < 0.02f);
    EASYFORGE_EXPECT(std::fabs(arm.Rotation.Get()) < 0.02f);

    // With some give, it sags.
    Body2D floppy = physics.AddBox({ .Position = { 1.5f, 8.0f }, .Size = { 2.0f, 0.4f } });
    Body2D post = physics.AddBox({ .Position = { 0.0f, 8.0f }, .Size = { 1.0f, 1.0f }, .Type = BodyType::Static });
    physics.AddWeldJoint({ .First = post, .Second = floppy, .Anchor = { 0.5f, 8.0f }, .Stiffness = 1.0f, .Damping = 1.0f });
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(floppy.Rotation.Get() < -0.05f);
}

EASYFORGE_TEST(MotorJointsDragBodies)
{
    Physics2D physics = Physics2D::New({ .Gravity = {} });
    Body2D ground = physics.AddBox({ .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
    Body2D puck = physics.AddCircle({ .Position = { 0.0f, 2.0f }, .Radius = 0.3f });
    Joint2D hand = physics.AddMotorJoint({
        .First = ground,
        .Second = puck,
        .LinearOffset = { 3.0f, 2.0f },
        .MaximumForce = 50.0f,
        .MaximumTorque = 50.0f,
    });
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(Distance(puck.Position.Get(), Vector2 { 3.0f, 2.0f }) < 0.05f);

    hand.LinearOffset = Vector2 { -2.0f, 1.0f };
    EASYFORGE_EXPECT(hand.LinearOffset.Get() == (Vector2 { -2.0f, 1.0f }));
    Run(physics, 3.0f);
    EASYFORGE_EXPECT(Distance(puck.Position.Get(), Vector2 { -2.0f, 1.0f }) < 0.05f);
}

EASYFORGE_TEST(JoinedBodiesDoNotCollideUnlessAsked)
{
    Physics2D physics = Physics2D::New({ .Gravity = {} });
    Body2D first = physics.AddBox({ .Position = { 0.0f, 0.0f } });
    Body2D second = physics.AddBox({ .Position = { 0.5f, 0.0f } });
    physics.AddDistanceJoint({ .First = first, .Second = second, .FirstAnchor = { 0.0f, 0.0f }, .SecondAnchor = { 0.5f, 0.0f }, .Stiffness = 0.5f });
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(Distance(first.Position.Get(), second.Position.Get()), 0.5f, 0.01f);

    Body2D third = physics.AddBox({ .Position = { 5.0f, 0.0f } });
    Body2D fourth = physics.AddBox({ .Position = { 5.5f, 0.0f } });
    physics.AddDistanceJoint({ .First = third, .Second = fourth, .FirstAnchor = { 5.0f, 0.0f }, .SecondAnchor = { 5.5f, 0.0f }, .Stiffness = 0.5f, .Collide = true });
    Run(physics, 1.0f);
    EASYFORGE_EXPECT(Distance(third.Position.Get(), fourth.Position.Get()) > 0.9f);
}

EASYFORGE_TEST(JointsGoWithTheirBodies)
{
    Physics2D physics = Physics2D::New();
    Body2D pin = physics.AddBox({ .Position = { 0.0f, 10.0f }, .Type = BodyType::Static });
    Body2D ball = physics.AddCircle({ .Position = { 0.0f, 8.0f }, .Radius = 0.25f });
    Joint2D rope = physics.AddDistanceJoint({ .First = pin, .Second = ball, .FirstAnchor = { 0.0f, 10.0f }, .SecondAnchor = { 0.0f, 8.0f } });
    Joint2D copy = rope;
    Run(physics, 1.0f);
    EASYFORGE_EXPECT_NEAR(ball.Position.Get().Y, 8.0f, 0.01f);

    rope.Remove();
    EASYFORGE_EXPECT(!rope);
    EASYFORGE_EXPECT(!copy);
    Run(physics, 1.0f);
    EASYFORGE_EXPECT(ball.Position.Get().Y < 4.0f);

    // Removing a body removes the joints on it.
    Body2D other = physics.AddCircle({ .Position = { 3.0f, 8.0f } });
    Joint2D hinge = physics.AddHingeJoint({ .First = pin, .Second = other, .Anchor = { 3.0f, 8.0f } });
    EASYFORGE_EXPECT(hinge);
    other.Remove();
    EASYFORGE_EXPECT(!hinge);
    EASYFORGE_EXPECT(!hinge.First());

    // A joint needs two different bodies of the same world.
    EASYFORGE_EXPECT(!physics.AddWeldJoint({ .First = pin, .Second = pin }));
    Physics2D elsewhere = Physics2D::New();
    Body2D stranger = elsewhere.AddBox({});
    EASYFORGE_EXPECT(!physics.AddWeldJoint({ .First = pin, .Second = stranger }));
    EASYFORGE_EXPECT(!physics.AddWeldJoint({ .First = pin, .Second = Body2D() }));
}
