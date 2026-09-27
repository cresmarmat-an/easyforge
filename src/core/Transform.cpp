#include <easyforge/core/Transform.h>

namespace easyforge
{
    Matrix4 Transform::ToMatrix() const
    {
        Matrix4 result = Matrix4::Rotation(Rotation);
        result.Columns[0] *= Scale.X;
        result.Columns[1] *= Scale.Y;
        result.Columns[2] *= Scale.Z;
        result.Columns[3] = { Position.X, Position.Y, Position.Z, 1.0f };
        return result;
    }

    Transform Combine(const Transform& parent, const Transform& child)
    {
        return {
            parent.ApplyToPoint(child.Position),
            parent.Rotation * child.Rotation,
            parent.Scale * child.Scale,
        };
    }
}
