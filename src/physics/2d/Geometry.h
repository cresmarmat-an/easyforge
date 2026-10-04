#pragma once

#include <cmath>
#include <optional>
#include <span>

#include <easyforge/core/Vector.h>
#include <easyforge/physics/Body2D.h>

namespace easyforge::internal::physics2d
{
    // The most corners a polygon keeps.
    inline constexpr int MaximumPoints = 8;

    // How far shapes may overlap without being pushed apart, in metres. Keeps
    // resting contacts from jittering.
    inline constexpr float LinearSlop = 0.005f;

    // Contacts begin this far before shapes touch, so the solver sees them
    // coming.
    inline constexpr float SpeculativeDistance = 4.0f * LinearSlop;

    // How much larger than a shape its box in the broadphase is, so small moves
    // do not need the tree changed.
    inline constexpr float BoundsMargin = 0.1f;

    struct Rotation
    {
        float Cosine = 1.0f;
        float Sine = 0.0f;

        static Rotation FromAngle(float angle) { return { std::cos(angle), std::sin(angle) }; }
        float Angle() const { return std::atan2(Sine, Cosine); }
    };

    inline Vector2 Rotate(Rotation turn, Vector2 vector)
    {
        return { turn.Cosine * vector.X - turn.Sine * vector.Y, turn.Sine * vector.X + turn.Cosine * vector.Y };
    }

    inline Vector2 Unrotate(Rotation turn, Vector2 vector)
    {
        return { turn.Cosine * vector.X + turn.Sine * vector.Y, -turn.Sine * vector.X + turn.Cosine * vector.Y };
    }

    // The first turn followed by the second.
    inline Rotation Combine(Rotation first, Rotation second)
    {
        return { first.Cosine * second.Cosine - first.Sine * second.Sine, first.Sine * second.Cosine + first.Cosine * second.Sine };
    }

    // The second turn as seen from the first.
    inline Rotation Between(Rotation first, Rotation second)
    {
        return { first.Cosine * second.Cosine + first.Sine * second.Sine, first.Cosine * second.Sine - first.Sine * second.Cosine };
    }

    // Turns by a small angle, keeping the rotation's length 1.
    inline Rotation Integrate(Rotation turn, float angle)
    {
        Rotation next { turn.Cosine - angle * turn.Sine, turn.Sine + angle * turn.Cosine };
        float length = std::sqrt(next.Cosine * next.Cosine + next.Sine * next.Sine);
        return length > 0.0f ? Rotation { next.Cosine / length, next.Sine / length } : Rotation {};
    }

    struct Pose
    {
        Vector2 Position;
        Rotation Turn;
    };

    inline Vector2 Apply(const Pose& pose, Vector2 local)
    {
        return pose.Position + Rotate(pose.Turn, local);
    }

    inline Vector2 Unapply(const Pose& pose, Vector2 world)
    {
        return Unrotate(pose.Turn, world - pose.Position);
    }

    // The second pose seen from the first.
    inline Pose Between(const Pose& first, const Pose& second)
    {
        return { Unrotate(first.Turn, second.Position - first.Position), Between(first.Turn, second.Turn) };
    }

    inline Vector2 LeftPerpendicular(Vector2 vector)
    {
        return { -vector.Y, vector.X };
    }

    inline Vector2 RightPerpendicular(Vector2 vector)
    {
        return { vector.Y, -vector.X };
    }

    // The velocity of a point at offset `arm` on a body turning at `turning`.
    inline Vector2 TurnVelocity(float turning, Vector2 arm)
    {
        return { -turning * arm.Y, turning * arm.X };
    }

    // The direction of a vector, or nothing for a vector too short to have one.
    inline std::optional<Vector2> DirectionOf(Vector2 vector)
    {
        float length = Length(vector);
        if (length < 1e-9f)
        {
            return std::nullopt;
        }
        return vector / length;
    }

    // An axis-aligned box: the broadphase's bounds.
    struct Extent
    {
        Vector2 Lower;
        Vector2 Upper;

        bool Overlaps(const Extent& other) const
        {
            return !(other.Lower.X > Upper.X || other.Lower.Y > Upper.Y || Lower.X > other.Upper.X || Lower.Y > other.Upper.Y);
        }

        bool Contains(const Extent& other) const
        {
            return Lower.X <= other.Lower.X && Lower.Y <= other.Lower.Y && other.Upper.X <= Upper.X && other.Upper.Y <= Upper.Y;
        }

        float Perimeter() const { return 2.0f * ((Upper.X - Lower.X) + (Upper.Y - Lower.Y)); }

        Extent Grown(float margin) const { return { Lower - Vector2 { margin, margin }, Upper + Vector2 { margin, margin } }; }
    };

    inline Extent Join(const Extent& first, const Extent& second)
    {
        return { Min(first.Lower, second.Lower), Max(first.Upper, second.Upper) };
    }

    // A convex polygon with rounded corners: a circle has one point, a capsule
    // two, and boxes and polygons their corners, counterclockwise. Normals[i]
    // points out of the edge from point i to the next.
    struct Shape
    {
        ShapeKind2D Kind = ShapeKind2D::Circle;
        Vector2 Points[MaximumPoints] {};
        Vector2 Normals[MaximumPoints] {};
        int Count = 0;
        float Radius = 0.0f;
    };

    Shape MakeCircle(float radius);
    Shape MakeCapsule(float length, float radius);
    Shape MakeBox(Vector2 size, float rounding);

    // The convex outline of the points, or nothing when they enclose no area.
    std::optional<Shape> MakePolygon(std::span<const Vector2> points, float rounding);

    struct MassProperties
    {
        float Mass = 0.0f;
        Vector2 Center;

        // About the center of mass.
        float Inertia = 0.0f;
    };

    MassProperties ComputeMass(const Shape& shape, float density);
    Extent ComputeExtent(const Shape& shape, const Pose& pose);
    bool ContainsPoint(const Shape& shape, Vector2 local);

    struct RayResult
    {
        bool Hit = false;

        // How far along the translation the hit is, from 0 to 1.
        float Fraction = 0.0f;
        Vector2 Normal;
    };

    // A ray from `origin` along `translation`, both in the shape's coordinates,
    // against the shape grown by `extraRadius`: a circle cast is a ray against
    // shapes grown by the circle's radius. A ray starting inside misses.
    RayResult CastRay(const Shape& shape, Vector2 origin, Vector2 translation, float extraRadius);

    struct SegmentDistance
    {
        Vector2 First;
        Vector2 Second;
        float FirstFraction = 0.0f;
        float SecondFraction = 0.0f;
        float DistanceSquared = 0.0f;
    };

    // The closest points of two segments.
    SegmentDistance ClosestOnSegments(Vector2 firstStart, Vector2 firstEnd, Vector2 secondStart, Vector2 secondEnd);
}
