#pragma once

#include <easyforge/core/Vector.h>

namespace easyforge
{
    struct Matrix3;

    // A rotation. The default value is no rotation.
    //
    // Angles are in radians. A positive angle turns counterclockwise when the axis
    // points toward you, so turning +X by a quarter turn around +Y gives -Z.
    struct Quaternion
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        float W = 1.0f;

        static constexpr Quaternion Identity() { return {}; }

        static Quaternion FromAxisAngle(Vector3 axis, float angle);

        // Turns around X (pitch), Y (yaw), and Z (roll). Roll is applied first,
        // then pitch, then yaw, which is the usual order for a camera or a character.
        static Quaternion FromAngles(float pitch, float yaw, float roll);

        // The rotation that turns the forward direction (-Z) toward `forward`, with
        // its top as close to `up` as possible.
        static Quaternion LookRotation(Vector3 forward, Vector3 up = { 0.0f, 1.0f, 0.0f });

        // The rotation held by a matrix with no scale.
        static Quaternion FromMatrix(const Matrix3& rotation);

        // `first * second` turns by `second`, then by `first`.
        constexpr Quaternion operator*(Quaternion other) const
        {
            return {
                W * other.X + X * other.W + Y * other.Z - Z * other.Y,
                W * other.Y - X * other.Z + Y * other.W + Z * other.X,
                W * other.Z + X * other.Y - Y * other.X + Z * other.W,
                W * other.W - X * other.X - Y * other.Y - Z * other.Z,
            };
        }

        constexpr Vector3 Rotate(Vector3 vector) const
        {
            Vector3 axis = { X, Y, Z };
            Vector3 twice = Cross(axis, vector) * 2.0f;
            return vector + twice * W + Cross(axis, twice);
        }

        constexpr bool operator==(const Quaternion&) const = default;
    };

    constexpr float Dot(Quaternion first, Quaternion second)
    {
        return first.X * second.X + first.Y * second.Y + first.Z * second.Z + first.W * second.W;
    }

    inline float Length(Quaternion rotation)
    {
        return std::sqrt(Dot(rotation, rotation));
    }

    // Rotations built from the functions above already have length 1. Normalize
    // after combining many of them, to remove the drift that rounding adds.
    inline Quaternion Normalize(Quaternion rotation)
    {
        float length = Length(rotation);
        if (length <= 0.0f)
        {
            return {};
        }
        return { rotation.X / length, rotation.Y / length, rotation.Z / length, rotation.W / length };
    }

    constexpr Quaternion Conjugate(Quaternion rotation)
    {
        return { -rotation.X, -rotation.Y, -rotation.Z, rotation.W };
    }

    // The opposite rotation.
    inline Quaternion Inverse(Quaternion rotation)
    {
        float lengthSquared = Dot(rotation, rotation);
        if (lengthSquared <= 0.0f)
        {
            return {};
        }
        Quaternion conjugate = Conjugate(rotation);
        return {
            conjugate.X / lengthSquared,
            conjugate.Y / lengthSquared,
            conjugate.Z / lengthSquared,
            conjugate.W / lengthSquared,
        };
    }

    // Turns from `from` (amount 0) to `to` (amount 1) at a steady speed, the short way around.
    Quaternion Slerp(Quaternion from, Quaternion to, float amount);

    // Compares the four numbers. The same rotation can also be written with every
    // number negated; use Dot(first, second) near 1 or -1 to accept both.
    inline bool NearlyEqual(Quaternion first, Quaternion second, float tolerance = 0.00001f)
    {
        return NearlyEqual(first.X, second.X, tolerance) && NearlyEqual(first.Y, second.Y, tolerance) &&
               NearlyEqual(first.Z, second.Z, tolerance) && NearlyEqual(first.W, second.W, tolerance);
    }
}
