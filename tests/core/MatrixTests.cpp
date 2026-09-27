#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(MatrixDefaultIsIdentity)
{
    Matrix4 matrix;
    Vector3 point = { 1.0f, 2.0f, 3.0f };
    EASYFORGE_EXPECT_EQUAL(matrix.TransformPoint(point), point);
    EASYFORGE_EXPECT_EQUAL(matrix, Matrix4::Identity());
    EASYFORGE_EXPECT_EQUAL(Matrix3 {} * point, point);
}

EASYFORGE_TEST(MatrixTranslationAndScale)
{
    Matrix4 move = Matrix4::Translation({ 10.0f, 0.0f, -5.0f });
    Matrix4 grow = Matrix4::Scale({ 2.0f, 3.0f, 4.0f });

    EASYFORGE_EXPECT_EQUAL(move.TransformPoint({ 1.0f, 1.0f, 1.0f }), (Vector3 { 11.0f, 1.0f, -4.0f }));
    EASYFORGE_EXPECT_EQUAL(move.TransformDirection({ 1.0f, 1.0f, 1.0f }), (Vector3 { 1.0f, 1.0f, 1.0f }));

    // Scale first, then move.
    Matrix4 both = move * grow;
    EASYFORGE_EXPECT_EQUAL(both.TransformPoint({ 1.0f, 1.0f, 1.0f }), (Vector3 { 12.0f, 3.0f, -1.0f }));
}

EASYFORGE_TEST(MatrixInverseUndoesTheMatrix)
{
    Matrix4 matrix = Matrix4::Translation({ 3.0f, -2.0f, 7.0f }) *
                     Matrix4::Rotation(Quaternion::FromAngles(0.3f, 1.1f, -0.4f)) *
                     Matrix4::Scale({ 2.0f, 0.5f, 1.5f });

    EASYFORGE_EXPECT_NEAR(matrix * Inverse(matrix), Matrix4::Identity(), 0.0001f);
    EASYFORGE_EXPECT_NEAR(Inverse(matrix) * matrix, Matrix4::Identity(), 0.0001f);
    EASYFORGE_EXPECT_NEAR(Determinant(matrix), 2.0f * 0.5f * 1.5f, 0.0001f);

    Matrix3 upperPart = Matrix3::FromMatrix4(matrix);
    EASYFORGE_EXPECT_NEAR(upperPart * Inverse(upperPart), Matrix3::Identity(), 0.0001f);
}

EASYFORGE_TEST(MatrixWithoutInverseGivesIdentity)
{
    Matrix4 flat = Matrix4::Scale({ 1.0f, 0.0f, 1.0f });
    EASYFORGE_EXPECT_EQUAL(Determinant(flat), 0.0f);
    EASYFORGE_EXPECT_EQUAL(Inverse(flat), Matrix4::Identity());
}

EASYFORGE_TEST(MatrixTranspose)
{
    Matrix4 matrix = Matrix4::Translation({ 1.0f, 2.0f, 3.0f });
    Matrix4 turned = Transpose(matrix);
    EASYFORGE_EXPECT_EQUAL(turned.Columns[0].W, 1.0f);
    EASYFORGE_EXPECT_EQUAL(turned.Columns[1].W, 2.0f);
    EASYFORGE_EXPECT_EQUAL(turned.Columns[2].W, 3.0f);
    EASYFORGE_EXPECT_EQUAL(Transpose(turned), matrix);
}

EASYFORGE_TEST(MatrixPerspectiveDepthRange)
{
    Matrix4 projection = Matrix4::Perspective(Radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    EASYFORGE_EXPECT_NEAR(projection.TransformPoint({ 0.0f, 0.0f, -0.1f }).Z, 0.0f, 0.0001f);
    EASYFORGE_EXPECT_NEAR(projection.TransformPoint({ 0.0f, 0.0f, -100.0f }).Z, 1.0f, 0.0001f);

    // A point on the top edge of the view lands at Y = 1.
    float top = std::tan(Radians(30.0f)) * 10.0f;
    EASYFORGE_EXPECT_NEAR(projection.TransformPoint({ 0.0f, top, -10.0f }).Y, 1.0f, 0.0001f);
}

EASYFORGE_TEST(MatrixOrthographic)
{
    Matrix4 projection = Matrix4::Orthographic(0.0f, 800.0f, 600.0f, 0.0f, 0.0f, 1.0f);

    EASYFORGE_EXPECT_NEAR(projection.TransformPoint({ 0.0f, 0.0f, 0.0f }), (Vector3 { -1.0f, 1.0f, 0.0f }), 0.0001f);
    EASYFORGE_EXPECT_NEAR(projection.TransformPoint({ 800.0f, 600.0f, -1.0f }), (Vector3 { 1.0f, -1.0f, 1.0f }), 0.0001f);
}

EASYFORGE_TEST(MatrixLookAt)
{
    Vector3 eye = { 0.0f, 2.0f, 5.0f };
    Vector3 target = { 0.0f, 2.0f, 0.0f };
    Matrix4 view = Matrix4::LookAt(eye, target);

    EASYFORGE_EXPECT_NEAR(view.TransformPoint(eye), (Vector3 {}), 0.0001f);
    EASYFORGE_EXPECT_NEAR(view.TransformPoint(target), (Vector3 { 0.0f, 0.0f, -5.0f }), 0.0001f);
    EASYFORGE_EXPECT_NEAR(view.TransformPoint({ 1.0f, 2.0f, 0.0f }), (Vector3 { 1.0f, 0.0f, -5.0f }), 0.0001f);
}
