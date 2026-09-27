#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <array>

using namespace easyforge;

EASYFORGE_TEST(RectangleEdges)
{
    Rectangle area = { 10.0f, 20.0f, 100.0f, 50.0f };

    EASYFORGE_EXPECT_EQUAL(area.Left(), 10.0f);
    EASYFORGE_EXPECT_EQUAL(area.Right(), 110.0f);
    EASYFORGE_EXPECT_EQUAL(area.Top(), 20.0f);
    EASYFORGE_EXPECT_EQUAL(area.Bottom(), 70.0f);
    EASYFORGE_EXPECT_EQUAL(area.Center(), (Vector2 { 60.0f, 45.0f }));
    EASYFORGE_EXPECT_EQUAL(Rectangle::FromCorners({ 110.0f, 70.0f }, { 10.0f, 20.0f }), area);
}

EASYFORGE_TEST(RectangleContainsIncludesOnlyLeftAndTopEdges)
{
    Rectangle area = { 0.0f, 0.0f, 10.0f, 10.0f };

    EASYFORGE_EXPECT(area.Contains({ 0.0f, 0.0f }));
    EASYFORGE_EXPECT(area.Contains({ 5.0f, 9.99f }));
    EASYFORGE_EXPECT(!area.Contains({ 10.0f, 5.0f }));
    EASYFORGE_EXPECT(!area.Contains({ 5.0f, 10.0f }));
    EASYFORGE_EXPECT(!area.Contains({ -0.01f, 5.0f }));
}

EASYFORGE_TEST(RectangleIntersectionAndUnion)
{
    Rectangle first = { 0.0f, 0.0f, 10.0f, 10.0f };
    Rectangle second = { 5.0f, 5.0f, 10.0f, 10.0f };
    Rectangle touching = { 10.0f, 0.0f, 5.0f, 5.0f };

    EASYFORGE_EXPECT(first.Intersects(second));
    EASYFORGE_EXPECT(!first.Intersects(touching));
    EASYFORGE_EXPECT_EQUAL(Intersection(first, second), (Rectangle { 5.0f, 5.0f, 5.0f, 5.0f }));
    EASYFORGE_EXPECT(Intersection(first, touching).IsEmpty());
    EASYFORGE_EXPECT_EQUAL(Union(first, second), (Rectangle { 0.0f, 0.0f, 15.0f, 15.0f }));
    EASYFORGE_EXPECT_EQUAL(Union(Rectangle {}, second), second);
}

EASYFORGE_TEST(BoundingBoxGrowsFromEmpty)
{
    BoundingBox box;
    EASYFORGE_EXPECT(box.IsEmpty());
    EASYFORGE_EXPECT_EQUAL(box.Size(), Vector3 {});

    box.Include(Vector3 { 1.0f, 2.0f, 3.0f });
    EASYFORGE_EXPECT(!box.IsEmpty());
    EASYFORGE_EXPECT_EQUAL(box.Size(), Vector3 {});

    box.Include(Vector3 { -1.0f, 4.0f, 0.0f });
    EASYFORGE_EXPECT_EQUAL(box.Minimum, (Vector3 { -1.0f, 2.0f, 0.0f }));
    EASYFORGE_EXPECT_EQUAL(box.Maximum, (Vector3 { 1.0f, 4.0f, 3.0f }));
    EASYFORGE_EXPECT_EQUAL(box.Center(), (Vector3 { 0.0f, 3.0f, 1.5f }));
}

EASYFORGE_TEST(BoundingBoxTests)
{
    std::array<Vector3, 3> points = { {
        { 0.0f, 0.0f, 0.0f },
        { 2.0f, 2.0f, 2.0f },
        { 1.0f, -1.0f, 1.0f },
    } };
    BoundingBox box = BoundingBox::FromPoints(points);

    EASYFORGE_EXPECT(box.Contains({ 2.0f, 2.0f, 2.0f }));
    EASYFORGE_EXPECT(!box.Contains({ 2.1f, 0.0f, 0.0f }));
    EASYFORGE_EXPECT(box.Intersects(BoundingBox { { 2.0f, 2.0f, 2.0f }, { 3.0f, 3.0f, 3.0f } }));
    EASYFORGE_EXPECT(!box.Intersects(BoundingBox { { 2.5f, 0.0f, 0.0f }, { 3.0f, 1.0f, 1.0f } }));
    EASYFORGE_EXPECT(!box.Intersects(BoundingBox {}));
}
