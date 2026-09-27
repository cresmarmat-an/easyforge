#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

namespace
{
    // q and -q are the same rotation.
    bool SameRotation(Quaternion first, Quaternion second)
    {
        return std::abs(Dot(first, second)) > 0.99999f;
    }
}

EASYFORGE_TEST(QuaternionDefaultDoesNotRotate)
{
    Vector3 point = { 1.0f, 2.0f, 3.0f };
    EASYFORGE_EXPECT_EQUAL(Quaternion {}.Rotate(point), point);
    EASYFORGE_EXPECT_EQUAL(Quaternion::Identity(), Quaternion {});
}

EASYFORGE_TEST(QuaternionAxisAngleIsCounterclockwise)
{
    Quaternion quarterAroundY = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi / 2.0f);
    EASYFORGE_EXPECT_NEAR(quarterAroundY.Rotate({ 1.0f, 0.0f, 0.0f }), (Vector3 { 0.0f, 0.0f, -1.0f }), 0.0001f);

    Quaternion quarterAroundZ = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, Pi / 2.0f);
    EASYFORGE_EXPECT_NEAR(quarterAroundZ.Rotate({ 1.0f, 0.0f, 0.0f }), (Vector3 { 0.0f, 1.0f, 0.0f }), 0.0001f);
}

EASYFORGE_TEST(QuaternionCombiningAppliesRightFirst)
{
    Quaternion aroundY = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi / 2.0f);
    Quaternion aroundZ = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, Pi / 2.0f);

    Vector3 point = { 1.0f, 0.0f, 0.0f };
    Vector3 stepByStep = aroundY.Rotate(aroundZ.Rotate(point));
    EASYFORGE_EXPECT_NEAR((aroundY * aroundZ).Rotate(point), stepByStep, 0.0001f);
}

EASYFORGE_TEST(QuaternionFromAnglesOrder)
{
    float pitch = 0.4f;
    float yaw = 1.2f;
    float roll = -0.7f;
    Quaternion combined = Quaternion::FromAngles(pitch, yaw, roll);

    Vector3 point = { 0.3f, -1.0f, 2.0f };
    Vector3 expected = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw)
                           .Rotate(Quaternion::FromAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch)
                                       .Rotate(Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, roll).Rotate(point)));
    EASYFORGE_EXPECT_NEAR(combined.Rotate(point), expected, 0.0001f);
}

EASYFORGE_TEST(QuaternionInverseUndoesTheRotation)
{
    Quaternion rotation = Quaternion::FromAngles(0.5f, -1.0f, 2.0f);
    Vector3 point = { 4.0f, -2.0f, 1.0f };
    EASYFORGE_EXPECT_NEAR(Inverse(rotation).Rotate(rotation.Rotate(point)), point, 0.0001f);
    EASYFORGE_EXPECT_NEAR(Length(rotation), 1.0f, 0.0001f);
}

EASYFORGE_TEST(QuaternionMatchesItsMatrix)
{
    Quaternion rotation = Quaternion::FromAngles(-0.8f, 2.5f, 0.3f);
    Vector3 point = { 1.0f, 2.0f, -3.0f };

    EASYFORGE_EXPECT_NEAR(Matrix3::Rotation(rotation) * point, rotation.Rotate(point), 0.0001f);
    EASYFORGE_EXPECT_NEAR(Matrix4::Rotation(rotation).TransformPoint(point), rotation.Rotate(point), 0.0001f);
    EASYFORGE_EXPECT(SameRotation(Quaternion::FromMatrix(Matrix3::Rotation(rotation)), rotation));

    // A half turn exercises the branches that do not use the trace.
    Quaternion halfTurn = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, Pi);
    EASYFORGE_EXPECT(SameRotation(Quaternion::FromMatrix(Matrix3::Rotation(halfTurn)), halfTurn));
}

EASYFORGE_TEST(QuaternionLookRotation)
{
    Vector3 forward = Normalize(Vector3 { 1.0f, 0.0f, -1.0f });
    Quaternion look = Quaternion::LookRotation(forward);

    EASYFORGE_EXPECT_NEAR(look.Rotate({ 0.0f, 0.0f, -1.0f }), forward, 0.0001f);
    EASYFORGE_EXPECT_NEAR(look.Rotate({ 0.0f, 1.0f, 0.0f }), (Vector3 { 0.0f, 1.0f, 0.0f }), 0.0001f);

    Quaternion straightUp = Quaternion::LookRotation({ 0.0f, 1.0f, 0.0f });
    EASYFORGE_EXPECT_NEAR(straightUp.Rotate({ 0.0f, 0.0f, -1.0f }), (Vector3 { 0.0f, 1.0f, 0.0f }), 0.0001f);
}

EASYFORGE_TEST(QuaternionSlerp)
{
    Quaternion from = {};
    Quaternion to = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi / 2.0f);
    Quaternion halfway = Slerp(from, to, 0.5f);

    EASYFORGE_EXPECT(SameRotation(halfway, Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi / 4.0f)));
    EASYFORGE_EXPECT(SameRotation(Slerp(from, to, 0.0f), from));
    EASYFORGE_EXPECT(SameRotation(Slerp(from, to, 1.0f), to));

    // The negated form of the target is the same rotation, and the path stays short.
    Quaternion negated = { -to.X, -to.Y, -to.Z, -to.W };
    EASYFORGE_EXPECT(SameRotation(Slerp(from, negated, 0.5f), halfway));
}
