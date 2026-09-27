#pragma once

#include <limits>
#include <span>

#include <easyforge/core/Vector.h>

namespace easyforge
{
    // A box lined up with the X, Y, and Z axes. The default value is empty, and
    // Include grows it to cover points and other boxes.
    struct BoundingBox
    {
        Vector3 Minimum = {
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
        };
        Vector3 Maximum = {
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
        };

        static constexpr BoundingBox FromPoints(std::span<const Vector3> points)
        {
            BoundingBox box;
            for (Vector3 point : points)
            {
                box.Include(point);
            }
            return box;
        }

        // True until something has been included. A box around a single point is not empty.
        constexpr bool IsEmpty() const
        {
            return Minimum.X > Maximum.X || Minimum.Y > Maximum.Y || Minimum.Z > Maximum.Z;
        }

        constexpr Vector3 Center() const { return (Minimum + Maximum) * 0.5f; }
        constexpr Vector3 Size() const { return IsEmpty() ? Vector3 {} : Maximum - Minimum; }

        constexpr void Include(Vector3 point)
        {
            Minimum = easyforge::Min(Minimum, point);
            Maximum = easyforge::Max(Maximum, point);
        }

        constexpr void Include(const BoundingBox& other)
        {
            if (!other.IsEmpty())
            {
                Include(other.Minimum);
                Include(other.Maximum);
            }
        }

        // Points on the surface count as inside.
        constexpr bool Contains(Vector3 point) const
        {
            return point.X >= Minimum.X && point.X <= Maximum.X && point.Y >= Minimum.Y && point.Y <= Maximum.Y &&
                   point.Z >= Minimum.Z && point.Z <= Maximum.Z;
        }

        // Boxes that touch count as intersecting.
        constexpr bool Intersects(const BoundingBox& other) const
        {
            return !IsEmpty() && !other.IsEmpty() && Minimum.X <= other.Maximum.X && other.Minimum.X <= Maximum.X &&
                   Minimum.Y <= other.Maximum.Y && other.Minimum.Y <= Maximum.Y && Minimum.Z <= other.Maximum.Z &&
                   other.Minimum.Z <= Maximum.Z;
        }

        constexpr bool operator==(const BoundingBox&) const = default;
    };
}
