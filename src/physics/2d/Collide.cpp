#include "Collide.h"

#include <algorithm>
#include <cmath>
#include <limits>

// Everything here works in the first shape's coordinates; Collide turns the
// result into world coordinates at the end.

namespace easyforge::internal::physics2d
{
    namespace
    {
        void AddPoint(Manifold& manifold, Vector2 point, float separation, std::uint32_t id)
        {
            ManifoldPoint& added = manifold.Points[manifold.Count++];
            added.Point = point;
            added.Separation = separation;
            added.Id = id;
        }

        Manifold CollideCircles(Vector2 firstCenter, float firstRadius, Vector2 secondCenter, float secondRadius)
        {
            Manifold manifold;
            Vector2 between = secondCenter - firstCenter;
            float distance = Length(between);
            float separation = distance - firstRadius - secondRadius;
            if (separation > SpeculativeDistance)
            {
                return manifold;
            }
            manifold.Normal = distance > 1e-9f ? between / distance : Vector2 { 1.0f, 0.0f };
            Vector2 onFirst = firstCenter + manifold.Normal * firstRadius;
            Vector2 onSecond = secondCenter - manifold.Normal * secondRadius;
            AddPoint(manifold, (onFirst + onSecond) * 0.5f, separation, 0);
            return manifold;
        }

        // A polygon or capsule against a circle at `center`.
        Manifold CollidePolygonCircle(const Shape& polygon, Vector2 center, float circleRadius)
        {
            Manifold manifold;
            int edge = 0;
            float largest = -std::numeric_limits<float>::infinity();
            for (int index = 0; index < polygon.Count; ++index)
            {
                float separation = Dot(polygon.Normals[index], center - polygon.Points[index]);
                if (separation > largest)
                {
                    largest = separation;
                    edge = index;
                }
            }
            float radius = polygon.Radius + circleRadius;
            if (largest > radius + SpeculativeDistance)
            {
                return manifold;
            }

            Vector2 start = polygon.Points[edge];
            Vector2 end = polygon.Points[(edge + 1) % polygon.Count];
            Vector2 normal = polygon.Normals[edge];
            Vector2 closest = center - normal * largest;
            float separation = largest - radius;
            // Outside the core and past an end of the edge, the nearest feature
            // is the corner.
            if (largest > 1e-6f)
            {
                Vector2 corner;
                bool atCorner = false;
                if (Dot(center - start, end - start) < 0.0f)
                {
                    corner = start;
                    atCorner = true;
                }
                else if (Dot(center - end, start - end) < 0.0f)
                {
                    corner = end;
                    atCorner = true;
                }
                if (atCorner)
                {
                    float distance = Distance(center, corner);
                    separation = distance - radius;
                    if (separation > SpeculativeDistance)
                    {
                        return manifold;
                    }
                    normal = distance > 1e-9f ? (center - corner) / distance : normal;
                    closest = corner;
                }
            }
            manifold.Normal = normal;
            Vector2 onPolygon = closest + normal * polygon.Radius;
            Vector2 onCircle = center - normal * circleRadius;
            AddPoint(manifold, (onPolygon + onCircle) * 0.5f, separation, 0);
            return manifold;
        }

        struct EdgeSeparation
        {
            int Edge = 0;
            float Separation = -std::numeric_limits<float>::infinity();
        };

        // The edge of `first` whose plane `second` is farthest out of.
        EdgeSeparation FindMaximumSeparation(const Shape& first, const Shape& second)
        {
            EdgeSeparation best;
            for (int edge = 0; edge < first.Count; ++edge)
            {
                float smallest = std::numeric_limits<float>::infinity();
                for (int point = 0; point < second.Count; ++point)
                {
                    smallest = std::min(smallest, Dot(first.Normals[edge], second.Points[point] - first.Points[edge]));
                }
                if (smallest > best.Separation)
                {
                    best.Separation = smallest;
                    best.Edge = edge;
                }
            }
            return best;
        }

        // Polygons and capsules against each other: the edge that separates them
        // most is the reference, and the other shape's edge facing it most is
        // clipped against it.
        Manifold CollidePolygons(const Shape& first, const Shape& second)
        {
            Manifold manifold;
            EdgeSeparation fromFirst = FindMaximumSeparation(first, second);
            EdgeSeparation fromSecond = FindMaximumSeparation(second, first);
            float radius = first.Radius + second.Radius;
            if (fromFirst.Separation > SpeculativeDistance + radius || fromSecond.Separation > SpeculativeDistance + radius)
            {
                return manifold;
            }

            // Prefer the first shape's edge unless the second's is clearly better,
            // so the choice does not flicker between steps.
            bool flip = fromSecond.Separation > 0.1f * LinearSlop + fromFirst.Separation;
            const Shape& reference = flip ? second : first;
            const Shape& incident = flip ? first : second;
            int referenceEdge = flip ? fromSecond.Edge : fromFirst.Edge;
            float separation = flip ? fromSecond.Separation : fromFirst.Separation;
            Vector2 normal = reference.Normals[referenceEdge];

            int incidentEdge = 0;
            float facing = std::numeric_limits<float>::infinity();
            for (int edge = 0; edge < incident.Count; ++edge)
            {
                float alignment = Dot(normal, incident.Normals[edge]);
                if (alignment < facing)
                {
                    facing = alignment;
                    incidentEdge = edge;
                }
            }

            Vector2 referenceStart = reference.Points[referenceEdge];
            Vector2 referenceEnd = reference.Points[(referenceEdge + 1) % reference.Count];
            Vector2 incidentStart = incident.Points[incidentEdge];
            Vector2 incidentEnd = incident.Points[(incidentEdge + 1) % incident.Count];
            std::uint32_t featureBase = (flip ? 0x10000u : 0u) | (static_cast<std::uint32_t>(referenceEdge) << 8) |
                                        (static_cast<std::uint32_t>(incidentEdge) << 1);

            Vector2 tangent = DirectionOf(referenceEnd - referenceStart).value_or(LeftPerpendicular(normal));
            float referenceLength = Dot(referenceEnd - referenceStart, tangent);
            float incidentFirst = Dot(incidentStart - referenceStart, tangent);
            float incidentSecond = Dot(incidentEnd - referenceStart, tangent);
            bool edgesFace = std::max(incidentFirst, incidentSecond) > 0.0f && std::min(incidentFirst, incidentSecond) < referenceLength;
            if (separation > 0.1f * LinearSlop && !edgesFace)
            {
                // The cores are apart and the edges do not face each other, so the
                // nearest features are two corners: one point, along the line
                // between them.
                SegmentDistance closest = ClosestOnSegments(referenceStart, referenceEnd, incidentStart, incidentEnd);
                float distance = std::sqrt(closest.DistanceSquared);
                if (distance > SpeculativeDistance + radius)
                {
                    return manifold;
                }
                Vector2 cornerNormal = distance > 1e-9f ? (closest.Second - closest.First) / distance : normal;
                Vector2 onReference = closest.First + cornerNormal * reference.Radius;
                Vector2 onIncident = closest.Second - cornerNormal * incident.Radius;
                manifold.Normal = flip ? -cornerNormal : cornerNormal;
                AddPoint(manifold, (onReference + onIncident) * 0.5f, distance - radius, featureBase | 0x20000u);
                return manifold;
            }

            // Clip the incident edge to the reference edge's extent. The incident
            // edge runs the other way, from upper to lower.
            float lowerReference = 0.0f;
            float upperReference = referenceLength;
            float upperIncident = incidentFirst;
            float lowerIncident = incidentSecond;
            float span = upperIncident - lowerIncident;

            Vector2 lower = incidentEnd;
            if (lowerIncident < lowerReference && span > 1e-9f)
            {
                lower = incidentEnd + (incidentStart - incidentEnd) * ((lowerReference - lowerIncident) / span);
            }
            Vector2 upper = incidentStart;
            if (upperIncident > upperReference && span > 1e-9f)
            {
                upper = incidentEnd + (incidentStart - incidentEnd) * ((upperReference - lowerIncident) / span);
            }

            float lowerSeparation = Dot(lower - referenceStart, normal);
            float upperSeparation = Dot(upper - referenceStart, normal);
            // Halfway between the two surfaces, allowing for their rounding.
            lower += normal * (0.5f * (reference.Radius - incident.Radius - lowerSeparation));
            upper += normal * (0.5f * (reference.Radius - incident.Radius - upperSeparation));
            lowerSeparation -= radius;
            upperSeparation -= radius;

            manifold.Normal = flip ? -normal : normal;
            if (lowerSeparation <= SpeculativeDistance)
            {
                AddPoint(manifold, lower, lowerSeparation, featureBase);
            }
            if (upperSeparation <= SpeculativeDistance)
            {
                AddPoint(manifold, upper, upperSeparation, featureBase | 1u);
            }
            return manifold;
        }
    }

    Manifold Collide(const Shape& first, const Pose& firstPose, const Shape& second, const Pose& secondPose)
    {
        Pose relative = Between(firstPose, secondPose);
        Shape local = second;
        for (int index = 0; index < second.Count; ++index)
        {
            local.Points[index] = Apply(relative, second.Points[index]);
            local.Normals[index] = Rotate(relative.Turn, second.Normals[index]);
        }

        Manifold manifold;
        if (first.Count == 1 && local.Count == 1)
        {
            manifold = CollideCircles(first.Points[0], first.Radius, local.Points[0], local.Radius);
        }
        else if (local.Count == 1)
        {
            manifold = CollidePolygonCircle(first, local.Points[0], local.Radius);
        }
        else if (first.Count == 1)
        {
            // The circle is first, so the normal from the polygon is turned round.
            manifold = CollidePolygonCircle(local, first.Points[0], first.Radius);
            manifold.Normal = -manifold.Normal;
        }
        else
        {
            manifold = CollidePolygons(first, local);
        }

        manifold.Normal = Rotate(firstPose.Turn, manifold.Normal);
        for (int index = 0; index < manifold.Count; ++index)
        {
            manifold.Points[index].Point = Apply(firstPose, manifold.Points[index].Point);
        }
        return manifold;
    }
}
