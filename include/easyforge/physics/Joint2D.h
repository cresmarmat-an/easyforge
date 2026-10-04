#pragma once

#include <limits>
#include <memory>

#include <easyforge/core/Property.h>
#include <easyforge/core/Vector.h>
#include <easyforge/physics/Body2D.h>

namespace easyforge
{
    namespace internal
    {
        struct JointReference2D;
    }

    // Anchors are points in the world, where the bodies are when the joint is
    // made. Bodies joined together do not collide with each other unless
    // Collide is set.

    // Keeps two points at a distance: a rod, or with Stiffness, a spring.
    struct DistanceJointSettings
    {
        Body2D First;
        Body2D Second;
        Vector2 FirstAnchor;
        Vector2 SecondAnchor;

        // Negative keeps the distance the anchors have now.
        float Length = -1.0f;

        // A spring's shortest and longest stretch; a rod ignores them.
        float MinimumLength = 0.0f;
        float MaximumLength = std::numeric_limits<float>::infinity();

        // Hertz: how fast the spring swings back. 0 makes a rigid rod.
        float Stiffness = 0.0f;

        // How quickly the swinging dies away: 0 never, 1 without overshooting.
        float Damping = 0.0f;

        bool Collide = false;
    };

    // Pins two bodies together at a point they turn around: a wheel, a door.
    struct HingeJointSettings
    {
        Body2D First;
        Body2D Second;
        Vector2 Anchor;

        // Keeps the angle between them, counted from where they are now, within
        // a range in radians.
        bool Limit = false;
        float LowerAngle = 0.0f;
        float UpperAngle = 0.0f;

        // Turns the second body at a speed in radians a second, with at most the
        // given torque.
        bool Motor = false;
        float MotorSpeed = 0.0f;
        float MaximumMotorTorque = 0.0f;

        bool Collide = false;
    };

    // Lets the second body slide along an axis of the first, without turning:
    // a piston, an elevator.
    struct SliderJointSettings
    {
        Body2D First;
        Body2D Second;
        Vector2 Anchor;

        // The direction of sliding, in the world when the joint is made.
        Vector2 Axis { 1.0f, 0.0f };

        // Keeps the distance moved along the axis within a range in metres.
        bool Limit = false;
        float Lower = 0.0f;
        float Upper = 0.0f;

        bool Motor = false;
        float MotorSpeed = 0.0f;
        float MaximumMotorForce = 0.0f;

        bool Collide = false;
    };

    // Holds two bodies together as one, rigidly or with some give.
    struct WeldJointSettings
    {
        Body2D First;
        Body2D Second;
        Vector2 Anchor;

        // Hertz; 0 holds rigidly.
        float Stiffness = 0.0f;
        float Damping = 1.0f;

        bool Collide = false;
    };

    // Moves the second body toward a place and angle relative to the first,
    // with limited force: dragging with the mouse, or a character controller.
    struct MotorJointSettings
    {
        Body2D First;
        Body2D Second;

        // Where the second body's origin should be, in the first body's
        // coordinates, and its angle relative to the first.
        Vector2 LinearOffset;
        float AngularOffset = 0.0f;

        float MaximumForce = 1.0f;
        float MaximumTorque = 1.0f;

        // How much of the remaining way to cover each second, from 0 to 1 of the
        // step.
        float Correction = 0.3f;

        bool Collide = false;
    };

    // A joint in a Physics2D world. Joint2D is a handle; once the joint or one
    // of its bodies is removed, it tests as false and does nothing.
    class Joint2D
    {
    public:
        // A handle that refers to no joint.
        Joint2D();

        Joint2D(const Joint2D& other);
        Joint2D& operator=(const Joint2D& other);
        ~Joint2D();

        explicit operator bool() const;

        Body2D First() const;
        Body2D Second() const;

        // A hinge's angle, counted from where it was made.
        float Angle() const;

        // How far a slider has moved along its axis.
        float Translation() const;

        void Remove() const;

        // Each kind of joint uses its own; the others are ignored.
        Property<float> MotorSpeed;       // hinge and slider
        Property<float> Length;           // distance
        Property<Vector2> LinearOffset;   // motor
        Property<float> AngularOffset;    // motor

    private:
        explicit Joint2D(std::shared_ptr<internal::JointReference2D> reference);
        void RebindProperties();

        std::shared_ptr<internal::JointReference2D> Reference;

        friend class Physics2D;
    };
}
