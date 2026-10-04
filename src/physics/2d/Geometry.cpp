#include "Geometry.h"

#include <algorithm>
#include <limits>
#include <numbers>
#include <vector>

namespace easyforge::internal::physics2d
{
    namespace
    {
        void SetNormals(Shape& shape)
        {
            for (int index = 0; index < shape.Count; ++index)
            {
                Vector2 edge = shape.Points[(index + 1) % shape.Count] - shape.Points[index];
                shape.Normals[index] = DirectionOf(RightPerpendicular(edge)).value_or(Vector2 { 0.0f, -1.0f });
            }
        }

        MassProperties PolygonMass(const Vector2* points, int count, float density)
        {
            MassProperties result;
            Vector2 origin = points[0];
            float area = 0.0f;
            Vector2 center;
            float rotational = 0.0f;
            for (int index = 1; index + 1 < count; ++index)
            {
                Vector2 first = points[index] - origin;
                Vector2 second = points[index + 1] - origin;
                float cross = Cross(first, second);
                float triangle = 0.5f * cross;
                area += triangle;
                center += (first + second) * (triangle / 3.0f);
                float squaresX = first.X * first.X + second.X * first.X + second.X * second.X;
                float squaresY = first.Y * first.Y + second.Y * first.Y + second.Y * second.Y;
                rotational += (0.25f / 3.0f) * cross * (squaresX + squaresY);
            }
            if (area <= 0.0f)
            {
                return result;
            }
            center /= area;
            result.Mass = density * area;
            result.Center = origin + center;
            result.Inertia = density * rotational - result.Mass * Dot(center, center);
            return result;
        }

        RayResult CastRayCircle(Vector2 center, float radius, Vector2 origin, Vector2 translation)
        {
            RayResult result;
            Vector2 offset = origin - center;
            float squared = Dot(translation, translation);
            float outside = Dot(offset, offset) - radius * radius;
            if (outside < 0.0f || squared <= 0.0f)
            {
                return result;
            }
            float along = Dot(offset, translation);
            float discriminant = along * along - squared * outside;
            if (discriminant < 0.0f)
            {
                return result;
            }
            float fraction = (-along - std::sqrt(discriminant)) / squared;
            if (fraction < 0.0f || fraction > 1.0f)
            {
                return result;
            }
            result.Hit = true;
            result.Fraction = fraction;
            result.Normal = DirectionOf(origin + translation * fraction - center).value_or(Vector2 { 0.0f, 1.0f });
            return result;
        }

        RayResult Nearer(RayResult first, RayResult second)
        {
            if (!first.Hit)
            {
                return second;
            }
            if (!second.Hit)
            {
                return first;
            }
            return second.Fraction < first.Fraction ? second : first;
        }
    }

    Shape MakeCircle(float radius)
    {
        Shape shape;
        shape.Kind = ShapeKind2D::Circle;
        shape.Count = 1;
        shape.Radius = std::max(radius, LinearSlop);
        return shape;
    }

    Shape MakeCapsule(float length, float radius)
    {
        Shape shape;
        shape.Kind = ShapeKind2D::Capsule;
        float half = std::max(length, LinearSlop) * 0.5f;
        shape.Points[0] = { -half, 0.0f };
        shape.Points[1] = { half, 0.0f };
        shape.Count = 2;
        shape.Radius = std::max(radius, LinearSlop);
        SetNormals(shape);
        return shape;
    }

    Shape MakeBox(Vector2 size, float rounding)
    {
        Shape shape;
        shape.Kind = ShapeKind2D::Box;
        float halfWidth = std::max(size.X, LinearSlop) * 0.5f;
        float halfHeight = std::max(size.Y, LinearSlop) * 0.5f;
        shape.Points[0] = { -halfWidth, -halfHeight };
        shape.Points[1] = { halfWidth, -halfHeight };
        shape.Points[2] = { halfWidth, halfHeight };
        shape.Points[3] = { -halfWidth, halfHeight };
        shape.Count = 4;
        shape.Radius = std::max(rounding, 0.0f);
        SetNormals(shape);
        return shape;
    }

    std::optional<Shape> MakePolygon(std::span<const Vector2> points, float rounding)
    {
        // Points closer than the slop are one point.
        std::vector<Vector2> kept;
        for (Vector2 point : points)
        {
            bool near = std::any_of(kept.begin(), kept.end(), [&](Vector2 other) { return Distance(point, other) < LinearSlop * 0.5f; });
            if (!near)
            {
                kept.push_back(point);
            }
        }
        if (kept.size() < 3)
        {
            return std::nullopt;
        }

        // Andrew's monotone chain gives the outline counterclockwise, without
        // points along a straight edge.
        std::sort(kept.begin(), kept.end(), [](Vector2 first, Vector2 second) {
            return first.X < second.X || (first.X == second.X && first.Y < second.Y);
        });
        std::vector<Vector2> hull;
        auto build = [&](auto begin, auto end) {
            std::size_t start = hull.size();
            for (auto point = begin; point != end; ++point)
            {
                while (hull.size() >= start + 2 && Cross(hull.back() - hull[hull.size() - 2], *point - hull[hull.size() - 2]) <= 0.0f)
                {
                    hull.pop_back();
                }
                hull.push_back(*point);
            }
            hull.pop_back();
        };
        build(kept.begin(), kept.end());
        build(kept.rbegin(), kept.rend());
        if (hull.size() < 3)
        {
            return std::nullopt;
        }

        // Too many corners: drop the ones that matter least to the outline.
        while (hull.size() > static_cast<std::size_t>(MaximumPoints))
        {
            std::size_t weakest = 0;
            float smallest = std::numeric_limits<float>::infinity();
            for (std::size_t index = 0; index < hull.size(); ++index)
            {
                Vector2 before = hull[(index + hull.size() - 1) % hull.size()];
                Vector2 after = hull[(index + 1) % hull.size()];
                float area = std::fabs(Cross(before - hull[index], after - hull[index]));
                if (area < smallest)
                {
                    smallest = area;
                    weakest = index;
                }
            }
            hull.erase(hull.begin() + static_cast<std::ptrdiff_t>(weakest));
        }

        Shape shape;
        shape.Kind = ShapeKind2D::Polygon;
        shape.Count = static_cast<int>(hull.size());
        std::copy(hull.begin(), hull.end(), shape.Points);
        shape.Radius = std::max(rounding, 0.0f);
        SetNormals(shape);
        if (PolygonMass(shape.Points, shape.Count, 1.0f).Mass < LinearSlop * LinearSlop)
        {
            return std::nullopt;
        }
        return shape;
    }

    MassProperties ComputeMass(const Shape& shape, float density)
    {
        float radius = shape.Radius;
        if (shape.Count == 1)
        {
            MassProperties result;
            result.Mass = density * std::numbers::pi_v<float> * radius * radius;
            result.Center = shape.Points[0];
            result.Inertia = result.Mass * 0.5f * radius * radius;
            return result;
        }
        if (shape.Count == 2)
        {
            // A box between two half circles. Each half circle's own inertia is
            // moved out to the end of the box, through its centroid 4r/3pi from
            // the flat side.
            MassProperties result;
            float length = Distance(shape.Points[0], shape.Points[1]);
            float circleMass = density * std::numbers::pi_v<float> * radius * radius;
            float boxMass = density * 2.0f * radius * length;
            float centroid = 4.0f * radius / (3.0f * std::numbers::pi_v<float>);
            float half = 0.5f * length;
            result.Mass = circleMass + boxMass;
            result.Center = (shape.Points[0] + shape.Points[1]) * 0.5f;
            result.Inertia = circleMass * (0.5f * radius * radius + half * half + 2.0f * half * centroid) +
                             boxMass * (4.0f * radius * radius + length * length) / 12.0f;
            return result;
        }
        if (radius <= 0.0f)
        {
            return PolygonMass(shape.Points, shape.Count, density);
        }
        // Rounded corners: close enough to push each corner out along its own
        // direction, as if the rounding were square.
        Vector2 grown[MaximumPoints];
        for (int index = 0; index < shape.Count; ++index)
        {
            Vector2 before = shape.Normals[(index + shape.Count - 1) % shape.Count];
            Vector2 after = shape.Normals[index];
            Vector2 middle = DirectionOf(before + after).value_or(after);
            float cosine = std::max(Dot(middle, after), 0.1f);
            grown[index] = shape.Points[index] + middle * (radius / cosine);
        }
        return PolygonMass(grown, shape.Count, density);
    }

    Extent ComputeExtent(const Shape& shape, const Pose& pose)
    {
        Vector2 first = Apply(pose, shape.Points[0]);
        Extent extent { first, first };
        for (int index = 1; index < shape.Count; ++index)
        {
            Vector2 point = Apply(pose, shape.Points[index]);
            extent.Lower = Min(extent.Lower, point);
            extent.Upper = Max(extent.Upper, point);
        }
        return extent.Grown(shape.Radius);
    }

    bool ContainsPoint(const Shape& shape, Vector2 local)
    {
        if (shape.Count == 1)
        {
            return Distance(local, shape.Points[0]) <= shape.Radius;
        }
        if (shape.Count == 2)
        {
            SegmentDistance closest = ClosestOnSegments(shape.Points[0], shape.Points[1], local, local);
            return closest.DistanceSquared <= shape.Radius * shape.Radius;
        }
        bool inside = true;
        for (int index = 0; index < shape.Count; ++index)
        {
            inside = inside && Dot(shape.Normals[index], local - shape.Points[index]) <= 0.0f;
        }
        if (inside || shape.Radius <= 0.0f)
        {
            return inside;
        }
        for (int index = 0; index < shape.Count; ++index)
        {
            SegmentDistance closest = ClosestOnSegments(shape.Points[index], shape.Points[(index + 1) % shape.Count], local, local);
            if (closest.DistanceSquared <= shape.Radius * shape.Radius)
            {
                return true;
            }
        }
        return false;
    }

    RayResult CastRay(const Shape& shape, Vector2 origin, Vector2 translation, float extraRadius)
    {
        float radius = shape.Radius + extraRadius;
        if (shape.Count == 1)
        {
            return CastRayCircle(shape.Points[0], radius, origin, translation);
        }

        // The planes of the edges, pushed out by the radius, clipped in turn.
        float lower = 0.0f;
        float upper = 1.0f;
        int entered = -1;
        for (int index = 0; index < shape.Count; ++index)
        {
            float numerator = Dot(shape.Normals[index], shape.Points[index] - origin) + radius;
            float denominator = Dot(shape.Normals[index], translation);
            if (denominator == 0.0f)
            {
                if (numerator < 0.0f)
                {
                    return {};
                }
            }
            else if (denominator < 0.0f && numerator < lower * denominator)
            {
                lower = numerator / denominator;
                entered = index;
            }
            else if (denominator > 0.0f && numerator < upper * denominator)
            {
                upper = numerator / denominator;
            }
            if (upper < lower)
            {
                return {};
            }
        }

        if (entered < 0)
        {
            // Inside every pushed out plane: inside the shape, or near a rounded
            // corner, where only the corners' circles can be hit.
            Shape grown = shape;
            grown.Radius = radius;
            if (radius <= 0.0f || ContainsPoint(grown, origin))
            {
                return {};
            }
            RayResult nearest;
            for (int index = 0; index < shape.Count; ++index)
            {
                nearest = Nearer(nearest, CastRayCircle(shape.Points[index], radius, origin, translation));
            }
            return nearest;
        }

        Vector2 start = shape.Points[entered];
        Vector2 end = shape.Points[(entered + 1) % shape.Count];
        if (radius > 0.0f)
        {
            // Past either end of the edge, the shape is the corner's circle.
            Vector2 hit = origin + translation * lower;
            Vector2 edge = end - start;
            float along = Dot(hit - start, edge) / std::max(Dot(edge, edge), 1e-12f);
            if (along < 0.0f)
            {
                return CastRayCircle(start, radius, origin, translation);
            }
            if (along > 1.0f)
            {
                return CastRayCircle(end, radius, origin, translation);
            }
        }
        RayResult result;
        result.Hit = true;
        result.Fraction = lower;
        result.Normal = shape.Normals[entered];
        return result;
    }

    SegmentDistance ClosestOnSegments(Vector2 firstStart, Vector2 firstEnd, Vector2 secondStart, Vector2 secondEnd)
    {
        Vector2 first = firstEnd - firstStart;
        Vector2 second = secondEnd - secondStart;
        Vector2 between = firstStart - secondStart;
        float firstSquared = Dot(first, first);
        float secondSquared = Dot(second, second);
        float secondAlong = Dot(second, between);
        constexpr float tiny = 1e-12f;

        float firstFraction = 0.0f;
        float secondFraction = 0.0f;
        if (firstSquared <= tiny && secondSquared <= tiny)
        {
        }
        else if (firstSquared <= tiny)
        {
            secondFraction = std::clamp(secondAlong / secondSquared, 0.0f, 1.0f);
        }
        else
        {
            float firstAlong = Dot(first, between);
            if (secondSquared <= tiny)
            {
                firstFraction = std::clamp(-firstAlong / firstSquared, 0.0f, 1.0f);
            }
            else
            {
                float both = Dot(first, second);
                float denominator = firstSquared * secondSquared - both * both;
                firstFraction = denominator != 0.0f ? std::clamp((both * secondAlong - firstAlong * secondSquared) / denominator, 0.0f, 1.0f) : 0.0f;
                secondFraction = (both * firstFraction + secondAlong) / secondSquared;
                if (secondFraction < 0.0f)
                {
                    secondFraction = 0.0f;
                    firstFraction = std::clamp(-firstAlong / firstSquared, 0.0f, 1.0f);
                }
                else if (secondFraction > 1.0f)
                {
                    secondFraction = 1.0f;
                    firstFraction = std::clamp((both - firstAlong) / firstSquared, 0.0f, 1.0f);
                }
            }
        }
        SegmentDistance result;
        result.First = firstStart + first * firstFraction;
        result.Second = secondStart + second * secondFraction;
        result.FirstFraction = firstFraction;
        result.SecondFraction = secondFraction;
        Vector2 gap = result.Second - result.First;
        result.DistanceSquared = Dot(gap, gap);
        return result;
    }
}
