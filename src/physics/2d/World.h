#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <easyforge/physics/Physics2D.h>

#include "Collide.h"
#include "Tree.h"

namespace easyforge::internal
{
    class WorldState2D;

    // Behind a Body2D: which body of which world.
    struct BodyReference2D
    {
        std::weak_ptr<WorldState2D> World;
        int Index = -1;
        std::uint32_t Generation = 0;

        static Body2D Make(const std::shared_ptr<WorldState2D>& world, int index);
        static Body2D Make(const std::shared_ptr<WorldState2D>& world, int index, std::uint32_t generation);
    };

    // Behind a Joint2D.
    struct JointReference2D
    {
        std::weak_ptr<WorldState2D> World;
        int Index = -1;
        std::uint32_t Generation = 0;
    };
}

namespace easyforge::internal::physics2d
{
    // Everything that describes a new body, whatever its shape.
    struct BodyDefinition
    {
        Shape Outline;
        Vector2 Size;
        float Length = 0.0f;
        Vector2 Position;
        float Rotation = 0.0f;
        BodyType Type = BodyType::Dynamic;
        Vector2 Velocity;
        float AngularVelocity = 0.0f;
        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;
        float RollingResistance = 0.01f;
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;
        float GravityScale = 1.0f;
        bool FixedRotation = false;
        bool Sensor = false;
        std::uint32_t Layer = 1;
        std::uint32_t CollidesWith = AllLayers;
        bool Bullet = false;
        bool AllowSleep = true;
    };

    struct BodyData
    {
        std::uint32_t Generation = 0;
        bool Alive = false;

        BodyType Type = BodyType::Dynamic;
        Shape Outline;
        Vector2 Size;
        float Length = 0.0f;

        // The origin the shape is placed around, and the center of mass.
        Pose Origin;

        // The origin at the start of the last step, for drawing between steps.
        Pose PreviousOrigin;
        Vector2 LocalCenter;
        Vector2 Center;

        Vector2 Velocity;
        float AngularVelocity = 0.0f;
        Vector2 Force;
        float Torque = 0.0f;

        float Mass = 0.0f;
        float InverseMass = 0.0f;
        float Inertia = 0.0f;
        float InverseInertia = 0.0f;

        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;
        float RollingResistance = 0.01f;
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;
        float GravityScale = 1.0f;
        bool FixedRotation = false;
        bool Sensor = false;
        std::uint32_t Layer = 1;
        std::uint32_t CollidesWith = AllLayers;

        // Swept through its move against dynamic bodies too, not only walls.
        bool Bullet = false;

        // A dynamic body still for long enough, with everything it touches,
        // sleeps: it is not moved or solved until something wakes it. Bodies
        // that fell asleep together share a group and wake together. Static and
        // kinematic bodies are always awake.
        bool AllowSleep = true;
        bool Awake = true;
        float StillTime = 0.0f;
        int SleepGroup = -1;

        // Where three points of the body were when it began to keep still: its
        // center, and two points at its reach. It stays still while each stays
        // near where it was, so a body trembling in place still rests. A still
        // time of zero means they are taken again.
        std::array<Vector2, 3> RestPoints {};

        // How far the shape reaches from the center of mass.
        Reach Extents;

        int Proxy = -1;
        std::vector<int> Joints;

        std::function<void(const Contact2D&)> OnTouch;
        std::function<void(const Contact2D&)> OnSeparate;

        // During a step: how far the body has moved, and its turn at the start.
        Vector2 DeltaPosition;
        Rotation StartTurn;
    };

    struct ContactData
    {
        int First = -1;
        int Second = -1;
        Manifold Points;
        bool Sensor = false;
        bool Touching = false;
    };

    enum class JointKind
    {
        Distance,
        Hinge,
        Slider,
        Weld,
        Motor,
    };

    struct JointData
    {
        std::uint32_t Generation = 0;
        bool Alive = false;
        JointKind Kind = JointKind::Distance;
        int First = -1;
        int Second = -1;
        bool Collide = false;

        // Anchors from each body's center of mass, in the body's coordinates.
        Vector2 LocalArmFirst;
        Vector2 LocalArmSecond;
        float ReferenceAngle = 0.0f;

        float Length = 0.0f;
        float MinimumLength = 0.0f;
        float MaximumLength = 0.0f;
        float Stiffness = 0.0f;
        float Damping = 0.0f;

        bool Limit = false;
        bool Motor = false;
        float Lower = 0.0f;
        float Upper = 0.0f;
        float MotorSpeed = 0.0f;
        float MaximumMotor = 0.0f;
        Vector2 LocalAxis;

        Vector2 LinearOffset;
        float AngularOffset = 0.0f;
        float MaximumForce = 0.0f;
        float MaximumTorque = 0.0f;
        float Correction = 0.0f;

        // Impulses kept between substeps and steps, to start from.
        Vector2 LinearImpulse;
        float AngularImpulse = 0.0f;
        float AxialImpulse = 0.0f;
        float LowerImpulse = 0.0f;
        float UpperImpulse = 0.0f;
        float MotorImpulse = 0.0f;
    };

    // A contact point as the solver works on it during a step.
    struct PointConstraint
    {
        // From each body's center of mass to the point, at the start of the step.
        Vector2 ArmFirst;
        Vector2 ArmSecond;
        // The separation with the arms taken out, so the current separation
        // follows from how far the bodies have moved.
        float BaseSeparation = 0.0f;
        float NormalMass = 0.0f;
        float TangentMass = 0.0f;
        float NormalImpulse = 0.0f;
        float TangentImpulse = 0.0f;
        float MaximumNormalImpulse = 0.0f;
        float RelativeVelocity = 0.0f;
    };

    struct ContactConstraint
    {
        std::size_t Contact = 0;
        int First = -1;
        int Second = -1;
        Vector2 Normal;
        float Friction = 0.0f;
        float Restitution = 0.0f;

        // Rolling resistance: the most torque per unit of normal impulse, the
        // mass for turning the two bodies against each other, and the
        // impulse so far this step.
        float RollingResistance = 0.0f;
        float RollingMass = 0.0f;
        float RollingImpulse = 0.0f;

        PointConstraint Points[2];
        int Count = 0;
    };

    // A step's report of two bodies starting or stopping to touch.
    struct TouchEvent
    {
        int First = -1;
        std::uint32_t FirstGeneration = 0;
        int Second = -1;
        std::uint32_t SecondGeneration = 0;
        Vector2 Point;
        Vector2 Normal;
        float Speed = 0.0f;
        bool Sensor = false;
        bool Began = true;
    };
}

namespace easyforge::internal
{
    class WorldState2D
    {
    public:
        explicit WorldState2D(const Physics2DSettings& settings);

        std::weak_ptr<WorldState2D> Self;
        Vector2 Gravity;
        int Substeps = 4;
        float StepSeconds = 1.0f / 60.0f;
        int MaximumStepsPerUpdate = 8;

        // Time Update has kept for the next step, and how far into that step it
        // is, from 0 to 1, which smooth poses blend by.
        double Waiting = 0.0;
        float StepShare = 1.0f;

        // Whether bodies may fall asleep at all.
        bool AllowSleep = true;

        std::function<void(float)> BeforeStep;
        std::function<void(float)> AfterStep;

        std::vector<physics2d::BodyData> Bodies;
        std::vector<physics2d::JointData> Joints;

        // The body at an index, if it is there and of that generation.
        physics2d::BodyData* Find(int index, std::uint32_t generation);
        physics2d::JointData* FindJoint(int index, std::uint32_t generation);

        int AddBody(const physics2d::BodyDefinition& definition);
        void RemoveBody(int index);
        int AddJoint(physics2d::JointData joint);
        void RemoveJoint(int index);

        // After a body's place, shape, or type changes outside a step.
        void Placed(int index);
        void Retyped(int index);

        // Wakes a sleeping body and everything that fell asleep with it.
        void Wake(int index);

        // Wakes a body and every body it has a contact with.
        void WakeNeighbors(int index);

        void WakeAll();

        void Step(float seconds);
        int Update(float seconds);

        // A body's origin to draw at, between its last two steps.
        physics2d::Pose SmoothOrigin(const physics2d::BodyData& body) const;

        RayHit2D CastRay(Vector2 origin, Vector2 direction, float distance, float radius, std::uint32_t layers);
        std::vector<int> BodiesAt(Vector2 point, std::uint32_t layers);
        std::vector<int> BodiesIn(const Rectangle& area, std::uint32_t layers);

    private:
        bool ShouldCollide(int first, int second) const;

        // Moving this step: an awake dynamic body, or a kinematic one with a
        // velocity.
        static bool IsMoving(const physics2d::BodyData& body);

        void FindNewContacts();
        void DestroyContact(std::size_t index);
        void UpdateContacts(std::vector<physics2d::TouchEvent>& events);
        void Solve(float seconds);
        void SweepFastBodies();
        void FinishBodies();
        void UpdateSleep(float seconds);
        void Report(const std::vector<physics2d::TouchEvent>& events);

        // Kept between steps, so a step allocates nothing once the world has
        // settled into its size.
        std::vector<physics2d::TouchEvent> StepEvents;
        std::vector<physics2d::TouchEvent> ReportedEvents;
        bool Reporting = false;
        std::vector<std::uint64_t> FoundPairs;
        std::vector<physics2d::ContactConstraint> Constraints;
        std::vector<physics2d::JointData*> ActiveJoints;
        std::vector<int> IslandParents;
        std::vector<float> IslandStillTimes;

        physics2d::BoundsTree Tree;
        std::vector<int> FreeBodies;
        std::vector<int> FreeJoints;
        std::vector<physics2d::ContactData> Contacts;
        std::unordered_map<std::uint64_t, std::size_t> ContactIndex;

        // Bodies whose boxes changed, to look for new contacts around.
        std::vector<int> Moved;
    };
}
