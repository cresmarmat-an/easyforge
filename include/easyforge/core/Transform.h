#pragma once

#include <easyforge/core/Matrix.h>

namespace easyforge
{
    // Where something is, how it is turned, and how big it is. Applied in the
    // order scale, then rotation, then position.
    struct Transform
    {
        Vector3 Position = {};
        Quaternion Rotation = {};
        Vector3 Scale = { 1.0f, 1.0f, 1.0f };

        Matrix4 ToMatrix() const;

        constexpr Vector3 ApplyToPoint(Vector3 point) const
        {
            return Position + Rotation.Rotate(point * Scale);
        }

        // Scales and turns a direction without moving it.
        constexpr Vector3 ApplyToDirection(Vector3 direction) const
        {
            return Rotation.Rotate(direction * Scale);
        }

        constexpr bool operator==(const Transform&) const = default;
    };

    // `child` placed inside `parent`: the child's transform expressed in the space
    // the parent lives in. Exact unless the parent is scaled unevenly and the child
    // is rotated, which would need a shear that a Transform cannot hold; then the
    // result's scale is the closest a Transform can get.
    Transform Combine(const Transform& parent, const Transform& child);
}
