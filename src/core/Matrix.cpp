#include <easyforge/core/Matrix.h>

#include <cmath>

namespace easyforge
{
    namespace
    {
        // The matrix as 16 numbers, column after column.
        struct Numbers
        {
            float Values[16];
        };

        Numbers Flatten(const Matrix4& matrix)
        {
            Numbers numbers {};
            for (int column = 0; column < 4; ++column)
            {
                numbers.Values[column * 4 + 0] = matrix.Columns[column].X;
                numbers.Values[column * 4 + 1] = matrix.Columns[column].Y;
                numbers.Values[column * 4 + 2] = matrix.Columns[column].Z;
                numbers.Values[column * 4 + 3] = matrix.Columns[column].W;
            }
            return numbers;
        }

        // The cofactor of every element, transposed: the inverse times the determinant.
        Numbers Adjugate(const Numbers& numbers)
        {
            const float* m = numbers.Values;
            Numbers result {};
            float* inverse = result.Values;

            inverse[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
                         m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
            inverse[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
                         m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
            inverse[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
                         m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
            inverse[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
                          m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
            inverse[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
                         m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
            inverse[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
                         m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
            inverse[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
                         m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
            inverse[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
                          m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
            inverse[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
                         m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
            inverse[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
                         m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
            inverse[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
                          m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
            inverse[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
                          m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
            inverse[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
                         m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
            inverse[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
                         m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
            inverse[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
                          m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
            inverse[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
                          m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

            return result;
        }

        float DeterminantFrom(const Numbers& numbers, const Numbers& adjugate)
        {
            const float* m = numbers.Values;
            const float* inverse = adjugate.Values;
            return m[0] * inverse[0] + m[1] * inverse[4] + m[2] * inverse[8] + m[3] * inverse[12];
        }

        // Element at a row and column, for the 3 by 3 formulas below.
        float At(const Matrix3& matrix, int row, int column)
        {
            const Vector3& values = matrix.Columns[column];
            return row == 0 ? values.X : (row == 1 ? values.Y : values.Z);
        }
    }

    Matrix3 Matrix3::Rotation(Quaternion rotation)
    {
        float x = rotation.X;
        float y = rotation.Y;
        float z = rotation.Z;
        float w = rotation.W;

        return { {
            { 1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + w * z), 2.0f * (x * z - w * y) },
            { 2.0f * (x * y - w * z), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + w * x) },
            { 2.0f * (x * z + w * y), 2.0f * (y * z - w * x), 1.0f - 2.0f * (x * x + y * y) },
        } };
    }

    Matrix3 Matrix3::FromMatrix4(const Matrix4& matrix)
    {
        return { {
            matrix.Columns[0].XYZ(),
            matrix.Columns[1].XYZ(),
            matrix.Columns[2].XYZ(),
        } };
    }

    Matrix4 Matrix4::Rotation(Quaternion rotation)
    {
        Matrix3 turned = Matrix3::Rotation(rotation);
        Matrix4 result;
        for (int column = 0; column < 3; ++column)
        {
            Vector3 values = turned.Columns[column];
            result.Columns[column] = { values.X, values.Y, values.Z, 0.0f };
        }
        return result;
    }

    Matrix4 Matrix4::Perspective(float fieldOfViewY, float aspectRatio, float nearDistance, float farDistance)
    {
        float focal = 1.0f / std::tan(fieldOfViewY * 0.5f);
        float depthScale = farDistance / (nearDistance - farDistance);

        return { {
            { focal / aspectRatio, 0.0f, 0.0f, 0.0f },
            { 0.0f, focal, 0.0f, 0.0f },
            { 0.0f, 0.0f, depthScale, -1.0f },
            { 0.0f, 0.0f, nearDistance * depthScale, 0.0f },
        } };
    }

    Matrix4 Matrix4::Orthographic(
        float left, float right, float bottom, float top, float nearDistance, float farDistance)
    {
        float width = right - left;
        float height = top - bottom;
        float depth = nearDistance - farDistance;

        return { {
            { 2.0f / width, 0.0f, 0.0f, 0.0f },
            { 0.0f, 2.0f / height, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f / depth, 0.0f },
            { -(right + left) / width, -(top + bottom) / height, nearDistance / depth, 1.0f },
        } };
    }

    Matrix4 Matrix4::LookAt(Vector3 eye, Vector3 target, Vector3 up)
    {
        Vector3 forward = Normalize(target - eye);
        Vector3 side = Normalize(Cross(forward, up));
        Vector3 top = Cross(side, forward);

        return { {
            { side.X, top.X, -forward.X, 0.0f },
            { side.Y, top.Y, -forward.Y, 0.0f },
            { side.Z, top.Z, -forward.Z, 0.0f },
            { -Dot(side, eye), -Dot(top, eye), Dot(forward, eye), 1.0f },
        } };
    }

    float Determinant(const Matrix3& matrix)
    {
        float a = At(matrix, 0, 0), b = At(matrix, 0, 1), c = At(matrix, 0, 2);
        float d = At(matrix, 1, 0), e = At(matrix, 1, 1), f = At(matrix, 1, 2);
        float g = At(matrix, 2, 0), h = At(matrix, 2, 1), i = At(matrix, 2, 2);
        return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    }

    float Determinant(const Matrix4& matrix)
    {
        Numbers numbers = Flatten(matrix);
        return DeterminantFrom(numbers, Adjugate(numbers));
    }

    Matrix3 Inverse(const Matrix3& matrix)
    {
        float determinant = Determinant(matrix);
        if (determinant == 0.0f)
        {
            return {};
        }

        float a = At(matrix, 0, 0), b = At(matrix, 0, 1), c = At(matrix, 0, 2);
        float d = At(matrix, 1, 0), e = At(matrix, 1, 1), f = At(matrix, 1, 2);
        float g = At(matrix, 2, 0), h = At(matrix, 2, 1), i = At(matrix, 2, 2);
        float scale = 1.0f / determinant;

        // Columns of the inverse, written from its rows.
        return { {
            { (e * i - f * h) * scale, -(d * i - f * g) * scale, (d * h - e * g) * scale },
            { -(b * i - c * h) * scale, (a * i - c * g) * scale, -(a * h - b * g) * scale },
            { (b * f - c * e) * scale, -(a * f - c * d) * scale, (a * e - b * d) * scale },
        } };
    }

    Matrix4 Inverse(const Matrix4& matrix)
    {
        Numbers numbers = Flatten(matrix);
        Numbers adjugate = Adjugate(numbers);
        float determinant = DeterminantFrom(numbers, adjugate);
        if (determinant == 0.0f)
        {
            return {};
        }

        float scale = 1.0f / determinant;
        Matrix4 result;
        for (int column = 0; column < 4; ++column)
        {
            const float* values = adjugate.Values + column * 4;
            result.Columns[column] = { values[0] * scale, values[1] * scale, values[2] * scale, values[3] * scale };
        }
        return result;
    }
}
