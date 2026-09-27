#include <easyforge/core/Quaternion.h>

#include <cmath>

#include <easyforge/core/Matrix.h>

namespace easyforge
{
    Quaternion Quaternion::FromAxisAngle(Vector3 axis, float angle)
    {
        Vector3 direction = Normalize(axis);
        float half = angle * 0.5f;
        float sine = std::sin(half);
        return { direction.X * sine, direction.Y * sine, direction.Z * sine, std::cos(half) };
    }

    Quaternion Quaternion::FromAngles(float pitch, float yaw, float roll)
    {
        Quaternion aroundX = FromAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
        Quaternion aroundY = FromAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
        Quaternion aroundZ = FromAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);
        return aroundY * aroundX * aroundZ;
    }

    Quaternion Quaternion::LookRotation(Vector3 forward, Vector3 up)
    {
        Vector3 front = Normalize(forward);
        if (LengthSquared(front) == 0.0f)
        {
            return {};
        }

        Vector3 side = Cross(front, up);
        if (LengthSquared(side) < 0.000001f)
        {
            // Looking straight along `up`; any side direction will do.
            side = Cross(front, std::abs(front.Y) < 0.9f ? Vector3 { 0.0f, 1.0f, 0.0f } : Vector3 { 1.0f, 0.0f, 0.0f });
        }
        side = Normalize(side);
        Vector3 top = Cross(side, front);

        // The columns send X to the side, Y to the top, and Z to the back, so -Z faces forward.
        return FromMatrix({ { side, top, -front } });
    }

    Quaternion Quaternion::FromMatrix(const Matrix3& rotation)
    {
        // Element at row r and column c is mRC.
        float m00 = rotation.Columns[0].X, m10 = rotation.Columns[0].Y, m20 = rotation.Columns[0].Z;
        float m01 = rotation.Columns[1].X, m11 = rotation.Columns[1].Y, m21 = rotation.Columns[1].Z;
        float m02 = rotation.Columns[2].X, m12 = rotation.Columns[2].Y, m22 = rotation.Columns[2].Z;

        float trace = m00 + m11 + m22;
        Quaternion result;
        if (trace > 0.0f)
        {
            float scale = std::sqrt(trace + 1.0f) * 2.0f;
            result = { (m21 - m12) / scale, (m02 - m20) / scale, (m10 - m01) / scale, 0.25f * scale };
        }
        else if (m00 > m11 && m00 > m22)
        {
            float scale = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            result = { 0.25f * scale, (m01 + m10) / scale, (m02 + m20) / scale, (m21 - m12) / scale };
        }
        else if (m11 > m22)
        {
            float scale = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            result = { (m01 + m10) / scale, 0.25f * scale, (m12 + m21) / scale, (m02 - m20) / scale };
        }
        else
        {
            float scale = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            result = { (m02 + m20) / scale, (m12 + m21) / scale, 0.25f * scale, (m10 - m01) / scale };
        }
        return Normalize(result);
    }

    Quaternion Slerp(Quaternion from, Quaternion to, float amount)
    {
        float cosine = Dot(from, to);

        // q and -q are the same rotation; flipping one takes the shorter way around.
        if (cosine < 0.0f)
        {
            to = { -to.X, -to.Y, -to.Z, -to.W };
            cosine = -cosine;
        }

        float fromWeight = 1.0f - amount;
        float toWeight = amount;

        // For nearly equal rotations the angle is too small to divide by, and a
        // straight blend is just as accurate.
        if (cosine < 0.9995f)
        {
            float angle = std::acos(cosine);
            float sine = std::sin(angle);
            fromWeight = std::sin((1.0f - amount) * angle) / sine;
            toWeight = std::sin(amount * angle) / sine;
        }

        return Normalize(Quaternion {
            from.X * fromWeight + to.X * toWeight,
            from.Y * fromWeight + to.Y * toWeight,
            from.Z * fromWeight + to.Z * toWeight,
            from.W * fromWeight + to.W * toWeight,
        });
    }
}
