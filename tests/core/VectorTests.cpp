#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(VectorArithmetic)
{
    Vector2 first = { 1.0f, 2.0f };
    Vector2 second = { 3.0f, 5.0f };

    EASYFORGE_EXPECT_EQUAL(first + second, (Vector2 { 4.0f, 7.0f }));
    EASYFORGE_EXPECT_EQUAL(second - first, (Vector2 { 2.0f, 3.0f }));
    EASYFORGE_EXPECT_EQUAL(first * 2.0f, (Vector2 { 2.0f, 4.0f }));
    EASYFORGE_EXPECT_EQUAL(2.0f * first, (Vector2 { 2.0f, 4.0f }));
    EASYFORGE_EXPECT_EQUAL(first * second, (Vector2 { 3.0f, 10.0f }));
    EASYFORGE_EXPECT_EQUAL(-first, (Vector2 { -1.0f, -2.0f }));

    Vector3 moving = { 1.0f, 1.0f, 1.0f };
    moving += { 1.0f, 2.0f, 3.0f };
    moving *= 2.0f;
    EASYFORGE_EXPECT_EQUAL(moving, (Vector3 { 4.0f, 6.0f, 8.0f }));
}

EASYFORGE_TEST(VectorLengthAndDirection)
{
    EASYFORGE_EXPECT_EQUAL(Length(Vector2 { 3.0f, 4.0f }), 5.0f);
    EASYFORGE_EXPECT_EQUAL(LengthSquared(Vector3 { 1.0f, 2.0f, 2.0f }), 9.0f);
    EASYFORGE_EXPECT_EQUAL(Distance(Vector3 { 1.0f, 0.0f, 0.0f }, Vector3 { 1.0f, 3.0f, 4.0f }), 5.0f);
    EASYFORGE_EXPECT_NEAR(Normalize(Vector3 { 0.0f, 0.0f, 9.0f }), (Vector3 { 0.0f, 0.0f, 1.0f }), 0.00001f);
    EASYFORGE_EXPECT_EQUAL(Normalize(Vector2 {}), Vector2 {});
}

EASYFORGE_TEST(VectorProducts)
{
    Vector3 x = { 1.0f, 0.0f, 0.0f };
    Vector3 y = { 0.0f, 1.0f, 0.0f };
    Vector3 z = { 0.0f, 0.0f, 1.0f };

    EASYFORGE_EXPECT_EQUAL(Dot(x, y), 0.0f);
    EASYFORGE_EXPECT_EQUAL(Dot(Vector3 { 1.0f, 2.0f, 3.0f }, Vector3 { 4.0f, 5.0f, 6.0f }), 32.0f);
    EASYFORGE_EXPECT_EQUAL(Cross(x, y), z);
    EASYFORGE_EXPECT_EQUAL(Cross(y, z), x);
    EASYFORGE_EXPECT_EQUAL(Cross(Vector2 { 1.0f, 0.0f }, Vector2 { 0.0f, 1.0f }), 1.0f);
    EASYFORGE_EXPECT_EQUAL(Perpendicular(Vector2 { 1.0f, 0.0f }), (Vector2 { 0.0f, 1.0f }));
}

EASYFORGE_TEST(VectorBlendingAndLimits)
{
    EASYFORGE_EXPECT_EQUAL(Lerp(Vector2 { 0.0f, 10.0f }, Vector2 { 10.0f, 20.0f }, 0.5f), (Vector2 { 5.0f, 15.0f }));
    EASYFORGE_EXPECT_EQUAL(Min(Vector3 { 1.0f, 5.0f, 3.0f }, Vector3 { 4.0f, 2.0f, 6.0f }), (Vector3 { 1.0f, 2.0f, 3.0f }));
    EASYFORGE_EXPECT_EQUAL(Max(Vector3 { 1.0f, 5.0f, 3.0f }, Vector3 { 4.0f, 2.0f, 6.0f }), (Vector3 { 4.0f, 5.0f, 6.0f }));
    EASYFORGE_EXPECT_EQUAL(Abs(Vector2 { -1.0f, 2.0f }), (Vector2 { 1.0f, 2.0f }));
    EASYFORGE_EXPECT_EQUAL(Clamp(15, 0, 10), 10);
    EASYFORGE_EXPECT_EQUAL(Clamp(-0.5f, 0.0f, 1.0f), 0.0f);
    EASYFORGE_EXPECT_NEAR(Degrees(Radians(90.0f)), 90.0f, 0.0001f);
}

EASYFORGE_TEST(VectorFormatting)
{
    EASYFORGE_EXPECT_EQUAL(std::format("{}", Vector2 { 1.0f, 2.5f }), std::string("(1, 2.5)"));
    EASYFORGE_EXPECT_EQUAL(std::format("{:.1f}", Vector3 { 1.0f, 2.0f, 3.0f }), std::string("(1.0, 2.0, 3.0)"));
    EASYFORGE_EXPECT_EQUAL(std::format("{}", Vector4 { 1.0f, 2.0f, 3.0f, 4.0f }), std::string("(1, 2, 3, 4)"));
}
