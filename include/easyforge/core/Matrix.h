#pragma once

#include <easyforge/core/Quaternion.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    struct Matrix4;

    // A 3 by 3 matrix stored as three columns. The default value is the identity matrix.
    struct Matrix3
    {
        Vector3 Columns[3] = {
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        };

        static constexpr Matrix3 Identity() { return {}; }

        static constexpr Matrix3 Scale(Vector3 scale)
        {
            return { {
                { scale.X, 0.0f, 0.0f },
                { 0.0f, scale.Y, 0.0f },
                { 0.0f, 0.0f, scale.Z },
            } };
        }

        static Matrix3 Rotation(Quaternion rotation);

        // The top-left 3 by 3 part: the rotation and scale without the position.
        static Matrix3 FromMatrix4(const Matrix4& matrix);

        constexpr Vector3 operator*(Vector3 vector) const
        {
            return Columns[0] * vector.X + Columns[1] * vector.Y + Columns[2] * vector.Z;
        }

        constexpr Matrix3 operator*(const Matrix3& other) const
        {
            return { {
                *this * other.Columns[0],
                *this * other.Columns[1],
                *this * other.Columns[2],
            } };
        }

        constexpr bool operator==(const Matrix3&) const = default;
    };

    // A 4 by 4 matrix stored as four columns, used with column vectors: the matrix
    // is on the left, and `second * first` applies `first` and then `second`.
    // The default value is the identity matrix.
    //
    // Projections follow the convention of Direct3D 12, Vulkan, Metal, and WebGPU:
    // the camera looks down -Z, and depth goes from 0 at the near plane to 1 at the
    // far plane.
    struct Matrix4
    {
        Vector4 Columns[4] = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f },
        };

        static constexpr Matrix4 Identity() { return {}; }

        static constexpr Matrix4 Translation(Vector3 offset)
        {
            Matrix4 result;
            result.Columns[3] = { offset.X, offset.Y, offset.Z, 1.0f };
            return result;
        }

        static constexpr Matrix4 Scale(Vector3 scale)
        {
            Matrix4 result;
            result.Columns[0].X = scale.X;
            result.Columns[1].Y = scale.Y;
            result.Columns[2].Z = scale.Z;
            return result;
        }

        static Matrix4 Rotation(Quaternion rotation);

        // `fieldOfViewY` is the vertical angle the camera sees, in radians.
        static Matrix4 Perspective(float fieldOfViewY, float aspectRatio, float nearDistance, float farDistance);

        static Matrix4 Orthographic(
            float left, float right, float bottom, float top, float nearDistance, float farDistance);

        // A view matrix: moves the world so that `eye` is at the origin looking down -Z toward `target`.
        static Matrix4 LookAt(Vector3 eye, Vector3 target, Vector3 up = { 0.0f, 1.0f, 0.0f });

        constexpr Vector4 operator*(Vector4 vector) const
        {
            return Columns[0] * vector.X + Columns[1] * vector.Y + Columns[2] * vector.Z + Columns[3] * vector.W;
        }

        constexpr Matrix4 operator*(const Matrix4& other) const
        {
            return { {
                *this * other.Columns[0],
                *this * other.Columns[1],
                *this * other.Columns[2],
                *this * other.Columns[3],
            } };
        }

        // Applies the matrix to a point, including its position. After a
        // projection the result is divided by W, giving normalized coordinates.
        constexpr Vector3 TransformPoint(Vector3 point) const
        {
            Vector4 result = *this * Vector4 { point.X, point.Y, point.Z, 1.0f };
            if (result.W != 0.0f && result.W != 1.0f)
            {
                return result.XYZ() / result.W;
            }
            return result.XYZ();
        }

        // Applies the matrix to a direction, which is not moved by the position part.
        constexpr Vector3 TransformDirection(Vector3 direction) const
        {
            return (*this * Vector4 { direction.X, direction.Y, direction.Z, 0.0f }).XYZ();
        }

        constexpr bool operator==(const Matrix4&) const = default;
    };

    constexpr Matrix3 Transpose(const Matrix3& matrix)
    {
        const Vector3* columns = matrix.Columns;
        return { {
            { columns[0].X, columns[1].X, columns[2].X },
            { columns[0].Y, columns[1].Y, columns[2].Y },
            { columns[0].Z, columns[1].Z, columns[2].Z },
        } };
    }

    constexpr Matrix4 Transpose(const Matrix4& matrix)
    {
        const Vector4* columns = matrix.Columns;
        return { {
            { columns[0].X, columns[1].X, columns[2].X, columns[3].X },
            { columns[0].Y, columns[1].Y, columns[2].Y, columns[3].Y },
            { columns[0].Z, columns[1].Z, columns[2].Z, columns[3].Z },
            { columns[0].W, columns[1].W, columns[2].W, columns[3].W },
        } };
    }

    float Determinant(const Matrix3& matrix);
    float Determinant(const Matrix4& matrix);

    // The matrix that undoes this one. A matrix whose determinant is zero has no
    // inverse; for one of those the result is the identity matrix, so check
    // Determinant first when that can happen.
    Matrix3 Inverse(const Matrix3& matrix);
    Matrix4 Inverse(const Matrix4& matrix);

    inline bool NearlyEqual(const Matrix3& first, const Matrix3& second, float tolerance = 0.00001f)
    {
        for (int column = 0; column < 3; ++column)
        {
            if (!NearlyEqual(first.Columns[column], second.Columns[column], tolerance))
            {
                return false;
            }
        }
        return true;
    }

    inline bool NearlyEqual(const Matrix4& first, const Matrix4& second, float tolerance = 0.00001f)
    {
        for (int column = 0; column < 4; ++column)
        {
            if (!NearlyEqual(first.Columns[column], second.Columns[column], tolerance))
            {
                return false;
            }
        }
        return true;
    }
}
