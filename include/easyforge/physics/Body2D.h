#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <easyforge/core/Property.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    namespace internal
    {
        struct BodyReference2D;
    }

    enum class BodyType
    {
        // Never moves; walls and the ground.
        Static,

        // Moves at the velocity it is given, pushing dynamic bodies aside but
        // never pushed itself; moving platforms.
        Kinematic,

        // Moves by gravity, forces, and collisions.
        Dynamic,
    };

    enum class ShapeKind2D
    {
        Circle,
        Capsule,
        Box,
        Polygon,
    };

    // Every collision layer, for CollidesWith and the layers of a query.
    inline constexpr std::uint32_t AllLayers = 0xFFFFFFFFu;

    // Positions are in metres and angles in radians, counterclockwise, with Y
    // up. The settings after the shape's own are the same for every shape.
    struct BoxSettings
    {
        Vector2 Position;
        float Rotation = 0.0f;
        Vector2 Size { 1.0f, 1.0f };

        // Rounds the corners by this many metres, outside the size.
        float Rounding = 0.0f;

        BodyType Type = BodyType::Dynamic;
        Vector2 Velocity;
        float AngularVelocity = 0.0f;

        // Kilograms per square metre.
        float Density = 1.0f;

        // How much the surface grips: 0 is ice, 1 is rubber.
        float Friction = 0.6f;

        // How much a collision bounces: 0 stops dead, 1 bounces back as fast.
        float Restitution = 0.0f;

        // Slows the body down over time, as air would.
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;

        float GravityScale = 1.0f;

        // The body never turns.
        bool FixedRotation = false;

        // The body reports what touches it but collides with nothing.
        bool Sensor = false;

        // Bits naming the layers the body is on, and the layers it collides
        // with. Two bodies collide when each is on a layer the other collides with.
        std::uint32_t Layer = 1;
        std::uint32_t CollidesWith = AllLayers;
    };

    struct CircleSettings
    {
        Vector2 Position;
        float Rotation = 0.0f;
        float Radius = 0.5f;

        BodyType Type = BodyType::Dynamic;
        Vector2 Velocity;
        float AngularVelocity = 0.0f;
        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;
        float GravityScale = 1.0f;
        bool FixedRotation = false;
        bool Sensor = false;
        std::uint32_t Layer = 1;
        std::uint32_t CollidesWith = AllLayers;
    };

    // A rectangle with round ends, lying along the body's X axis: Length is the
    // distance between the centers of the ends.
    struct CapsuleSettings
    {
        Vector2 Position;
        float Rotation = 0.0f;
        float Length = 1.0f;
        float Radius = 0.25f;

        BodyType Type = BodyType::Dynamic;
        Vector2 Velocity;
        float AngularVelocity = 0.0f;
        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;
        float GravityScale = 1.0f;
        bool FixedRotation = false;
        bool Sensor = false;
        std::uint32_t Layer = 1;
        std::uint32_t CollidesWith = AllLayers;
    };

    // A convex shape around the given points, in the body's own coordinates.
    // Points inside the outline are left out, and at most eight corners are kept.
    struct PolygonSettings
    {
        Vector2 Position;
        float Rotation = 0.0f;
        std::vector<Vector2> Points;
        float Rounding = 0.0f;

        BodyType Type = BodyType::Dynamic;
        Vector2 Velocity;
        float AngularVelocity = 0.0f;
        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;
        float GravityScale = 1.0f;
        bool FixedRotation = false;
        bool Sensor = false;
        std::uint32_t Layer = 1;
        std::uint32_t CollidesWith = AllLayers;
    };

    struct Contact2D;

    // A body in a Physics2D world: one shape, its place, and how it moves.
    //
    //     Body2D crate = physics.AddBox({ .Position = { 0, 10 }, .Size = { 1, 1 } });
    //     crate.Velocity = { 2, 0 };
    //     crate.OnTouch = [](const Contact2D& contact) { /* ... */ };
    //
    // Body2D is a handle: copies refer to the same body. Once the body is
    // removed, or its world is gone, the handle tests as false and does nothing.
    class Body2D
    {
    public:
        // A handle that refers to no body.
        Body2D();

        Body2D(const Body2D& other);
        Body2D& operator=(const Body2D& other);
        ~Body2D();

        explicit operator bool() const;
        bool operator==(const Body2D& other) const;

        // A number for the body, different from every other body's in its world.
        std::uint64_t Id() const;

        ShapeKind2D Shape() const;

        // The box's size; for other shapes, the size of the box around them.
        Vector2 Size() const;

        // A circle's or capsule's radius, or a box's or polygon's rounding.
        float Radius() const;

        // A capsule's length between the centers of its ends.
        float Length() const;

        // The corners of a box or polygon in the body's own coordinates, without
        // rounding; the two end centers of a capsule; the center of a circle.
        std::vector<Vector2> Points() const;

        bool IsSensor() const;

        // In kilograms, and kilogram square metres about the center of mass.
        float Mass() const;
        float Inertia() const;

        // The center of mass, in the world.
        Vector2 Center() const;

        // The smallest box around the body, in the world: X and Y are its lowest
        // corner.
        Rectangle Bounds() const;

        Vector2 WorldPoint(Vector2 local) const;
        Vector2 LocalPoint(Vector2 world) const;

        // A force acts over the next step, at the center of mass or at a point in
        // the world. An impulse changes the velocity at once.
        void ApplyForce(Vector2 force) const;
        void ApplyForce(Vector2 force, Vector2 point) const;
        void ApplyImpulse(Vector2 impulse) const;
        void ApplyImpulse(Vector2 impulse, Vector2 point) const;
        void ApplyTorque(float torque) const;
        void ApplyAngularImpulse(float impulse) const;

        // Takes the body out of its world, with the joints attached to it.
        void Remove() const;

        // The body's origin, which its shape is placed around.
        Property<Vector2> Position;
        Property<float> Rotation;
        Property<Vector2> Velocity;
        Property<float> AngularVelocity;
        Property<BodyType> Type;
        Property<float> Friction;
        Property<float> Restitution;
        Property<float> LinearDamping;
        Property<float> AngularDamping;
        Property<float> GravityScale;
        Property<bool> FixedRotation;
        Property<std::uint32_t> Layer;
        Property<std::uint32_t> CollidesWith;

        // Called after a step when another body starts or stops touching this one.
        Property<std::function<void(const Contact2D&)>> OnTouch;
        Property<std::function<void(const Contact2D&)>> OnSeparate;

    private:
        explicit Body2D(std::shared_ptr<internal::BodyReference2D> reference);
        void RebindProperties();

        std::shared_ptr<internal::BodyReference2D> Reference;

        friend class Physics2D;
        friend class Joint2D;
        friend struct internal::BodyReference2D;
    };

    // Two bodies starting or stopping to touch, as the body told sees it.
    struct Contact2D
    {
        Body2D Other;

        // Where they touch, and the direction from this body toward the other.
        Vector2 Point;
        Vector2 Normal;

        // How fast they were moving toward each other, in metres a second.
        float Speed = 0.0f;

        // True when either body is a sensor, which only reports.
        bool Sensor = false;
    };

    // What a ray or a cast circle hit first. Tests as false when it hit nothing.
    struct RayHit2D
    {
        Body2D Body;
        Vector2 Point;
        Vector2 Normal;
        float Distance = 0.0f;

        explicit operator bool() const { return static_cast<bool>(Body); }
    };
}
