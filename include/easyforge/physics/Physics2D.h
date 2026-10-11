#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <easyforge/core/Property.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/core/Vector.h>
#include <easyforge/physics/Body2D.h>
#include <easyforge/physics/Export.h>
#include <easyforge/physics/Joint2D.h>

namespace easyforge
{
    namespace internal
    {
        class WorldState2D;
    }

    struct Physics2DSettings
    {
        // Metres a second squared.
        Vector2 Gravity { 0.0f, -9.8f };

        // Passes the solver makes within each step. More make tall stacks and
        // chains stiffer, and steps slower.
        int Substeps = 4;

        // The length of each step Update runs, in seconds. Steps of one length
        // keep the world stable and give the same result every run.
        float StepSeconds = 1.0f / 60.0f;

        // The most steps one Update runs. After a long pause, such as a window
        // being dragged, the world falls behind a little instead of the program
        // spending ever longer catching up.
        int MaximumStepsPerUpdate = 8;

        // Lets bodies that have been still for half a second sleep until
        // something wakes them.
        bool AllowSleep = true;
    };

    // A world of 2D bodies that collide, rest on each other, and are joined
    // together.
    //
    //     Physics2D physics = Physics2D::New({ .Gravity = { 0, -9.8f } });
    //     physics.AddBox({ .Size = { 50, 1 }, .Type = BodyType::Static });
    //     Body2D crate = physics.AddBox({ .Position = { 0, 10 }, .Size = { 1, 1 } });
    //
    //     window.OnFrame = [&](float deltaSeconds) { physics.Update(deltaSeconds); };
    //     Vector2 shown = crate.SmoothPosition();   // for drawing
    //     Vector2 exact = crate.Position;           // for game rules
    //
    // The same steps with the same bodies give the same result every run. A
    // world is used from one thread at a time. Physics2D is a handle: copies
    // share the world, which lives while any copy or body handle uses it.
    class EASYFORGE_PHYSICS_EXPORT Physics2D
    {
    public:
        // A world that holds nothing. It tests as false.
        Physics2D();

        [[nodiscard]] static Physics2D New(const Physics2DSettings& settings = {});

        Physics2D(const Physics2D& other);
        Physics2D& operator=(const Physics2D& other);
        ~Physics2D();

        explicit operator bool() const;

        Body2D AddBox(const BoxSettings& settings) const;
        Body2D AddCircle(const CircleSettings& settings) const;
        Body2D AddCapsule(const CapsuleSettings& settings) const;
        Body2D AddPolygon(const PolygonSettings& settings) const;

        Joint2D AddDistanceJoint(const DistanceJointSettings& settings) const;
        Joint2D AddHingeJoint(const HingeJointSettings& settings) const;
        Joint2D AddSliderJoint(const SliderJointSettings& settings) const;
        Joint2D AddWeldJoint(const WeldJointSettings& settings) const;
        Joint2D AddMotorJoint(const MotorJointSettings& settings) const;

        // Moves the world on by the seconds since the last frame, in as many
        // steps of StepSeconds as fit, and keeps the rest for the next call.
        // Returns how many steps ran. Draw bodies at SmoothPosition and
        // SmoothRotation, which blend the last two steps by the time kept, so
        // motion is even at any frame rate. The touches of each step are
        // reported after it.
        int Update(float seconds) const;

        // Moves the world on by exactly one step of the given seconds, for
        // programs that keep time themselves. Steps of the same length every
        // time keep the world stable and repeatable. After Step, the smooth
        // position of every body is its position.
        void Step(float seconds) const;

        // The first body a ray meets within a distance. A ray that starts inside
        // a body does not hit that body, and sensors are never hit.
        RayHit2D CastRay(Vector2 origin, Vector2 direction, float distance, std::uint32_t layers = AllLayers) const;

        // The first body a circle moving along a direction meets.
        RayHit2D CastCircle(Vector2 center, float radius, Vector2 direction, float distance, std::uint32_t layers = AllLayers) const;

        // The bodies at a point, and those overlapping an area whose X and Y
        // are its lowest corner, in the same order every run.
        std::vector<Body2D> BodiesAt(Vector2 point, std::uint32_t layers = AllLayers) const;
        std::vector<Body2D> BodiesIn(const Rectangle& area, std::uint32_t layers = AllLayers) const;

        std::vector<Body2D> Bodies() const;
        std::size_t BodyCount() const;

        Property<Vector2> Gravity;
        Property<int> Substeps;
        Property<float> StepSeconds;
        Property<int> MaximumStepsPerUpdate;
        Property<bool> AllowSleep;

    private:
        explicit Physics2D(std::shared_ptr<internal::WorldState2D> state);
        void RebindProperties();

        std::shared_ptr<internal::WorldState2D> State;
    };
}
