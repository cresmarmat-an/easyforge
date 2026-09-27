#pragma once

#include <easyforge/core/Scalar.h>

namespace easyforge
{
    struct Vector2
    {
        float X = 0.0f;
        float Y = 0.0f;

        constexpr Vector2 operator-() const { return { -X, -Y }; }
        constexpr Vector2 operator+(Vector2 other) const { return { X + other.X, Y + other.Y }; }
        constexpr Vector2 operator-(Vector2 other) const { return { X - other.X, Y - other.Y }; }
        constexpr Vector2 operator*(Vector2 other) const { return { X * other.X, Y * other.Y }; }
        constexpr Vector2 operator/(Vector2 other) const { return { X / other.X, Y / other.Y }; }
        constexpr Vector2 operator*(float scale) const { return { X * scale, Y * scale }; }
        constexpr Vector2 operator/(float scale) const { return { X / scale, Y / scale }; }

        constexpr Vector2& operator+=(Vector2 other) { return *this = *this + other; }
        constexpr Vector2& operator-=(Vector2 other) { return *this = *this - other; }
        constexpr Vector2& operator*=(Vector2 other) { return *this = *this * other; }
        constexpr Vector2& operator/=(Vector2 other) { return *this = *this / other; }
        constexpr Vector2& operator*=(float scale) { return *this = *this * scale; }
        constexpr Vector2& operator/=(float scale) { return *this = *this / scale; }

        constexpr bool operator==(const Vector2&) const = default;
    };

    struct Vector3
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;

        constexpr Vector2 XY() const { return { X, Y }; }

        constexpr Vector3 operator-() const { return { -X, -Y, -Z }; }
        constexpr Vector3 operator+(Vector3 other) const { return { X + other.X, Y + other.Y, Z + other.Z }; }
        constexpr Vector3 operator-(Vector3 other) const { return { X - other.X, Y - other.Y, Z - other.Z }; }
        constexpr Vector3 operator*(Vector3 other) const { return { X * other.X, Y * other.Y, Z * other.Z }; }
        constexpr Vector3 operator/(Vector3 other) const { return { X / other.X, Y / other.Y, Z / other.Z }; }
        constexpr Vector3 operator*(float scale) const { return { X * scale, Y * scale, Z * scale }; }
        constexpr Vector3 operator/(float scale) const { return { X / scale, Y / scale, Z / scale }; }

        constexpr Vector3& operator+=(Vector3 other) { return *this = *this + other; }
        constexpr Vector3& operator-=(Vector3 other) { return *this = *this - other; }
        constexpr Vector3& operator*=(Vector3 other) { return *this = *this * other; }
        constexpr Vector3& operator/=(Vector3 other) { return *this = *this / other; }
        constexpr Vector3& operator*=(float scale) { return *this = *this * scale; }
        constexpr Vector3& operator/=(float scale) { return *this = *this / scale; }

        constexpr bool operator==(const Vector3&) const = default;
    };

    struct Vector4
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        float W = 0.0f;

        constexpr Vector2 XY() const { return { X, Y }; }
        constexpr Vector3 XYZ() const { return { X, Y, Z }; }

        constexpr Vector4 operator-() const { return { -X, -Y, -Z, -W }; }
        constexpr Vector4 operator+(Vector4 other) const { return { X + other.X, Y + other.Y, Z + other.Z, W + other.W }; }
        constexpr Vector4 operator-(Vector4 other) const { return { X - other.X, Y - other.Y, Z - other.Z, W - other.W }; }
        constexpr Vector4 operator*(Vector4 other) const { return { X * other.X, Y * other.Y, Z * other.Z, W * other.W }; }
        constexpr Vector4 operator/(Vector4 other) const { return { X / other.X, Y / other.Y, Z / other.Z, W / other.W }; }
        constexpr Vector4 operator*(float scale) const { return { X * scale, Y * scale, Z * scale, W * scale }; }
        constexpr Vector4 operator/(float scale) const { return { X / scale, Y / scale, Z / scale, W / scale }; }

        constexpr Vector4& operator+=(Vector4 other) { return *this = *this + other; }
        constexpr Vector4& operator-=(Vector4 other) { return *this = *this - other; }
        constexpr Vector4& operator*=(Vector4 other) { return *this = *this * other; }
        constexpr Vector4& operator/=(Vector4 other) { return *this = *this / other; }
        constexpr Vector4& operator*=(float scale) { return *this = *this * scale; }
        constexpr Vector4& operator/=(float scale) { return *this = *this / scale; }

        constexpr bool operator==(const Vector4&) const = default;
    };

    constexpr Vector2 operator*(float scale, Vector2 vector) { return vector * scale; }
    constexpr Vector3 operator*(float scale, Vector3 vector) { return vector * scale; }
    constexpr Vector4 operator*(float scale, Vector4 vector) { return vector * scale; }

    constexpr float Dot(Vector2 first, Vector2 second)
    {
        return first.X * second.X + first.Y * second.Y;
    }

    constexpr float Dot(Vector3 first, Vector3 second)
    {
        return first.X * second.X + first.Y * second.Y + first.Z * second.Z;
    }

    constexpr float Dot(Vector4 first, Vector4 second)
    {
        return first.X * second.X + first.Y * second.Y + first.Z * second.Z + first.W * second.W;
    }

    // The Z part of the 3D cross product: positive when `second` is counterclockwise from `first`.
    constexpr float Cross(Vector2 first, Vector2 second)
    {
        return first.X * second.Y - first.Y * second.X;
    }

    // Right-handed: Cross(X, Y) is Z.
    constexpr Vector3 Cross(Vector3 first, Vector3 second)
    {
        return {
            first.Y * second.Z - first.Z * second.Y,
            first.Z * second.X - first.X * second.Z,
            first.X * second.Y - first.Y * second.X,
        };
    }

    constexpr float LengthSquared(Vector2 vector) { return Dot(vector, vector); }
    constexpr float LengthSquared(Vector3 vector) { return Dot(vector, vector); }
    constexpr float LengthSquared(Vector4 vector) { return Dot(vector, vector); }

    inline float Length(Vector2 vector) { return std::sqrt(LengthSquared(vector)); }
    inline float Length(Vector3 vector) { return std::sqrt(LengthSquared(vector)); }
    inline float Length(Vector4 vector) { return std::sqrt(LengthSquared(vector)); }

    inline float Distance(Vector2 first, Vector2 second) { return Length(second - first); }
    inline float Distance(Vector3 first, Vector3 second) { return Length(second - first); }

    // The same direction with length 1. A zero vector stays zero.
    inline Vector2 Normalize(Vector2 vector)
    {
        float length = Length(vector);
        return length > 0.0f ? vector / length : Vector2 {};
    }

    inline Vector3 Normalize(Vector3 vector)
    {
        float length = Length(vector);
        return length > 0.0f ? vector / length : Vector3 {};
    }

    inline Vector4 Normalize(Vector4 vector)
    {
        float length = Length(vector);
        return length > 0.0f ? vector / length : Vector4 {};
    }

    constexpr Vector2 Lerp(Vector2 from, Vector2 to, float amount) { return from + (to - from) * amount; }
    constexpr Vector3 Lerp(Vector3 from, Vector3 to, float amount) { return from + (to - from) * amount; }
    constexpr Vector4 Lerp(Vector4 from, Vector4 to, float amount) { return from + (to - from) * amount; }

    constexpr Vector2 Min(Vector2 first, Vector2 second)
    {
        return { Min(first.X, second.X), Min(first.Y, second.Y) };
    }

    constexpr Vector3 Min(Vector3 first, Vector3 second)
    {
        return { Min(first.X, second.X), Min(first.Y, second.Y), Min(first.Z, second.Z) };
    }

    constexpr Vector2 Max(Vector2 first, Vector2 second)
    {
        return { Max(first.X, second.X), Max(first.Y, second.Y) };
    }

    constexpr Vector3 Max(Vector3 first, Vector3 second)
    {
        return { Max(first.X, second.X), Max(first.Y, second.Y), Max(first.Z, second.Z) };
    }

    inline Vector2 Abs(Vector2 vector) { return { std::abs(vector.X), std::abs(vector.Y) }; }
    inline Vector3 Abs(Vector3 vector) { return { std::abs(vector.X), std::abs(vector.Y), std::abs(vector.Z) }; }

    // Turned a quarter turn counterclockwise.
    constexpr Vector2 Perpendicular(Vector2 vector)
    {
        return { -vector.Y, vector.X };
    }

    inline bool NearlyEqual(Vector2 first, Vector2 second, float tolerance = 0.00001f)
    {
        return NearlyEqual(first.X, second.X, tolerance) && NearlyEqual(first.Y, second.Y, tolerance);
    }

    inline bool NearlyEqual(Vector3 first, Vector3 second, float tolerance = 0.00001f)
    {
        return NearlyEqual(first.X, second.X, tolerance) && NearlyEqual(first.Y, second.Y, tolerance) &&
               NearlyEqual(first.Z, second.Z, tolerance);
    }

    inline bool NearlyEqual(Vector4 first, Vector4 second, float tolerance = 0.00001f)
    {
        return NearlyEqual(first.X, second.X, tolerance) && NearlyEqual(first.Y, second.Y, tolerance) &&
               NearlyEqual(first.Z, second.Z, tolerance) && NearlyEqual(first.W, second.W, tolerance);
    }
}
