#pragma once

#include <easyforge/core/Vector.h>

namespace easyforge
{
    // An area on a flat surface, from its corner at (X, Y). The names Top and
    // Bottom assume Y grows downward, as it does on screen.
    struct Rectangle
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;

        static constexpr Rectangle FromCorners(Vector2 first, Vector2 second)
        {
            Vector2 lowest = Min(first, second);
            Vector2 highest = Max(first, second);
            return { lowest.X, lowest.Y, highest.X - lowest.X, highest.Y - lowest.Y };
        }

        constexpr float Left() const { return X; }
        constexpr float Right() const { return X + Width; }
        constexpr float Top() const { return Y; }
        constexpr float Bottom() const { return Y + Height; }

        constexpr Vector2 Position() const { return { X, Y }; }
        constexpr Vector2 Size() const { return { Width, Height }; }
        constexpr Vector2 Center() const { return { X + Width * 0.5f, Y + Height * 0.5f }; }

        // True when the width or the height is zero or less.
        constexpr bool IsEmpty() const { return Width <= 0.0f || Height <= 0.0f; }

        // The left and top edges are inside; the right and bottom edges are not,
        // so two rectangles that touch never both contain the same point.
        constexpr bool Contains(Vector2 point) const
        {
            return point.X >= Left() && point.X < Right() && point.Y >= Top() && point.Y < Bottom();
        }

        // True when the two share some area. Rectangles that only touch do not intersect.
        constexpr bool Intersects(const Rectangle& other) const
        {
            return !IsEmpty() && !other.IsEmpty() && Left() < other.Right() && other.Left() < Right() &&
                   Top() < other.Bottom() && other.Top() < Bottom();
        }

        constexpr bool operator==(const Rectangle&) const = default;
    };

    // The area both share, or an empty rectangle when they share none.
    constexpr Rectangle Intersection(const Rectangle& first, const Rectangle& second)
    {
        if (!first.Intersects(second))
        {
            return {};
        }
        float left = Max(first.Left(), second.Left());
        float top = Max(first.Top(), second.Top());
        float right = Min(first.Right(), second.Right());
        float bottom = Min(first.Bottom(), second.Bottom());
        return { left, top, right - left, bottom - top };
    }

    // The smallest rectangle that covers both. An empty rectangle is ignored.
    constexpr Rectangle Union(const Rectangle& first, const Rectangle& second)
    {
        if (first.IsEmpty())
        {
            return second;
        }
        if (second.IsEmpty())
        {
            return first;
        }
        float left = Min(first.Left(), second.Left());
        float top = Min(first.Top(), second.Top());
        float right = Max(first.Right(), second.Right());
        float bottom = Max(first.Bottom(), second.Bottom());
        return { left, top, right - left, bottom - top };
    }
}
