#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(TransformDefaultChangesNothing)
{
    Transform transform;
    Vector3 point = { 1.0f, -2.0f, 3.0f };
    EASYFORGE_EXPECT_EQUAL(transform.ApplyToPoint(point), point);
    EASYFORGE_EXPECT_EQUAL(transform.ToMatrix(), Matrix4::Identity());
}

EASYFORGE_TEST(TransformOrderIsScaleRotationPosition)
{
    Transform transform = {
        .Position = { 10.0f, 0.0f, 0.0f },
        .Rotation = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, Pi / 2.0f),
        .Scale = { 2.0f, 2.0f, 2.0f },
    };

    // (1, 0, 0) is scaled to (2, 0, 0), turned to (0, 2, 0), and moved to (10, 2, 0).
    EASYFORGE_EXPECT_NEAR(transform.ApplyToPoint({ 1.0f, 0.0f, 0.0f }), (Vector3 { 10.0f, 2.0f, 0.0f }), 0.0001f);
    EASYFORGE_EXPECT_NEAR(transform.ApplyToDirection({ 1.0f, 0.0f, 0.0f }), (Vector3 { 0.0f, 2.0f, 0.0f }), 0.0001f);
}

EASYFORGE_TEST(TransformMatrixMatchesTransform)
{
    Transform transform = {
        .Position = { -3.0f, 4.0f, 1.0f },
        .Rotation = Quaternion::FromAngles(0.2f, -0.9f, 1.4f),
        .Scale = { 1.5f, 0.5f, 3.0f },
    };

    Vector3 point = { 0.5f, 2.0f, -1.0f };
    EASYFORGE_EXPECT_NEAR(transform.ToMatrix().TransformPoint(point), transform.ApplyToPoint(point), 0.0001f);
}

EASYFORGE_TEST(TransformCombine)
{
    Transform parent = {
        .Position = { 0.0f, 5.0f, 0.0f },
        .Rotation = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi / 2.0f),
        .Scale = { 2.0f, 2.0f, 2.0f },
    };
    Transform child = {
        .Position = { 1.0f, 0.0f, 0.0f },
        .Rotation = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi / 2.0f),
    };

    Transform combined = Combine(parent, child);
    Vector3 point = { 1.0f, 0.0f, 0.0f };
    EASYFORGE_EXPECT_NEAR(combined.ApplyToPoint(point), parent.ApplyToPoint(child.ApplyToPoint(point)), 0.0001f);
    EASYFORGE_EXPECT_NEAR(combined.Scale, (Vector3 { 2.0f, 2.0f, 2.0f }), 0.0001f);
}
