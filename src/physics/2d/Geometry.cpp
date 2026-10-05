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

    namespace
    {
        // A shape's core in the world: its points, and its outward normals.
        struct WorldShape
        {
            Vector2 Points[MaximumPoints] {};
            Vector2 Normals[MaximumPoints] {};
            int Count = 0;
        };

        WorldShape InWorld(const Shape& shape, const Pose& pose)
        {
            WorldShape placed;
            placed.Count = shape.Count;
            for (int index = 0; index < shape.Count; ++index)
            {
                placed.Points[index] = Apply(pose, shape.Points[index]);
                placed.Normals[index] = Rotate(pose.Turn, shape.Normals[index]);
            }
            return placed;
        }

        bool Inside(const WorldShape& polygon, Vector2 point)
        {
            for (int index = 0; index < polygon.Count; ++index)
            {
                if (Dot(polygon.Normals[index], point - polygon.Points[index]) > 0.0f)
                {
                    return false;
                }
            }
            return true;
        }

        // How far the second shape's core is outside the first polygon along the
        // polygon's best edge normal, and that normal: positive when an edge has
        // all of it outside, negative by about how deep it is in when none has.
        struct EdgeGap
        {
            float Separation = -std::numeric_limits<float>::infinity();
            Vector2 Normal;
        };

        EdgeGap EdgeSeparation(const WorldShape& polygon, const WorldShape& other)
        {
            EdgeGap best;
            for (int edge = 0; edge < polygon.Count; ++edge)
            {
                float nearest = std::numeric_limits<float>::infinity();
                for (int point = 0; point < other.Count; ++point)
                {
                    nearest = std::min(nearest, Dot(polygon.Normals[edge], other.Points[point] - polygon.Points[edge]));
                }
                if (nearest > best.Separation)
                {
                    best = { nearest, polygon.Normals[edge] };
                }
            }
            return best;
        }

        bool Separates(const WorldShape& first, const WorldShape& second)
        {
            return EdgeSeparation(first, second).Separation > 0.0f;
        }

        // Cores overlap when a polygon holds a point of the other shape, or two
        // polygons have no edge between them. A segment crossing a polygon
        // without a point inside it is caught by the edge distances instead.
        bool CoresOverlap(const WorldShape& first, const WorldShape& second)
        {
            if (first.Count >= 3 && second.Count >= 3)
            {
                return !Separates(first, second) && !Separates(second, first);
            }
            if (first.Count >= 3)
            {
                return Inside(first, second.Points[0]);
            }
            if (second.Count >= 3)
            {
                return Inside(second, first.Points[0]);
            }
            return false;
        }

        // One point for a circle, one segment for a capsule, every side for a
        // polygon.
        int EdgeCount(const WorldShape& shape)
        {
            return shape.Count >= 3 ? shape.Count : 1;
        }

        Vector2 EdgeEnd(const WorldShape& shape, int edge)
        {
            return shape.Count == 1 ? shape.Points[0] : shape.Points[(edge + 1) % shape.Count];
        }
    }

    ShapeGap Gap(const Shape& first, const Pose& firstPose, const Shape& second, const Pose& secondPose)
    {
        WorldShape one = InWorld(first, firstPose);
        WorldShape other = InWorld(second, secondPose);
        float radius = first.Radius + second.Radius;
        if (CoresOverlap(one, other))
        {
            // How deep, along the polygons' best edge normal, so getting deeper
            // shows as a smaller gap. The second polygon's normal points back
            // toward the first, so it is turned around.
            EdgeGap best;
            if (one.Count >= 3)
            {
                best = EdgeSeparation(one, other);
            }
            if (other.Count >= 3)
            {
                EdgeGap reverse = EdgeSeparation(other, one);
                if (reverse.Separation > best.Separation)
                {
                    best = { reverse.Separation, -reverse.Normal };
                }
            }
            if (!std::isfinite(best.Separation))
            {
                return { -radius, {} };
            }
            return { std::min(best.Separation, 0.0f) - radius, best.Normal };
        }

        SegmentDistance closest;
        closest.DistanceSquared = std::numeric_limits<float>::infinity();
        for (int edge = 0; edge < EdgeCount(one); ++edge)
        {
            for (int otherEdge = 0; otherEdge < EdgeCount(other); ++otherEdge)
            {
                SegmentDistance distance = ClosestOnSegments(one.Points[edge], EdgeEnd(one, edge), other.Points[otherEdge],
                    EdgeEnd(other, otherEdge));
                if (distance.DistanceSquared < closest.DistanceSquared)
                {
                    closest = distance;
                }
            }
        }
        float length = std::sqrt(closest.DistanceSquared);
        Vector2 normal = length > 1e-6f ? (closest.Second - closest.First) / length : Vector2 {};
        return { length - radius, normal };
    }

    Reach ComputeReach(const Shape& shape, Vector2 center)
    {
        Reach reach;
        float most = 0.0f;
        for (int index = 0; index < shape.Count; ++index)
        {
            most = std::max(most, Length(shape.Points[index] - center));
        }
        reach.Most = most + shape.Radius;

        float least = 0.0f;
        if (shape.Count >= 3)
        {
            least = std::numeric_limits<float>::infinity();
            for (int index = 0; index < shape.Count; ++index)
            {
                least = std::min(least, Dot(shape.Normals[index], shape.Points[index] - center));
            }
            least = std::max(least, 0.0f) + shape.Radius;
        }
        else
        {
            Vector2 end = shape.Count == 2 ? shape.Points[1] : shape.Points[0];
            float away = std::sqrt(ClosestOnSegments(shape.Points[0], end, center, center).DistanceSquared);
            least = std::max(shape.Radius - away, 0.0f);
        }
        reach.Least = std::max(least, LinearSlop);
        return reach;
    }

    float TimeOfImpact(const Shape& moving, const Sweep& sweep, float reach, const Shape& still, const Pose& stillPose, float target)
    {
        float turning = std::abs(sweep.Angle) * reach;
        if (!(Length(sweep.Move) + turning > 1e-6f))
        {
            return 1.0f;
        }
        // A shape that starts closer than the target, such as one resting on
        // or sliding along the still shape, counts only once it gets deeper
        // than it started, so it can slide on while a part of it that turns
        // toward the still shape is still caught.
        ShapeGap gap = Gap(moving, sweep.At(0.0f), still, stillPose);
        if (gap.Distance <= target)
        {
            target = gap.Distance - LinearSlop;
        }
        // Aiming a little inside the target makes each move end within it when
        // heading straight on, so few moves are needed.
        float aim = target - LinearSlop * 0.5f;
        float time = 0.0f;
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            // Along the line between the closest points, nothing on the moving
            // shape closes in faster than its move toward the still one plus its
            // turning, so the gap cannot close before then. Moving alongside
            // closes nothing.
            bool known = gap.Normal.X != 0.0f || gap.Normal.Y != 0.0f;
            float approach = turning + (known ? std::max(Dot(sweep.Move, gap.Normal), 0.0f) : Length(sweep.Move));
            if (!(approach > 1e-6f))
            {
                return 1.0f;
            }
            time += (gap.Distance - aim) / approach;
            if (time >= 1.0f)
            {
                return 1.0f;
            }
            gap = Gap(moving, sweep.At(time), still, stillPose);
            if (gap.Distance <= target)
            {
                return time;
            }
        }
        return time;
    }
}
