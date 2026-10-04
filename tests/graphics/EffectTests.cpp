#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>

#include <cstdlib>

using namespace easyforge;

namespace
{
    Renderer Offscreen(int width, int height, float scale = 1.0f)
    {
        return Renderer::New({ .Adapter = GraphicsAdapter::Software, .Width = width, .Height = height, .Scale = scale });
    }

    template <typename Draw>
    ImageData Picture(Renderer& renderer, Draw draw, Color background = Color::Black)
    {
        Canvas canvas = renderer.BeginFrame(background);
        draw(canvas);
        renderer.EndFrame();
        return renderer.Capture();
    }

    int Channel(const ImageData& image, int x, int y, int channel)
    {
        return image.Pixels[static_cast<std::size_t>(y) * image.Stride() + static_cast<std::size_t>(x) * 4 +
                            static_cast<std::size_t>(channel)];
    }

    bool Near(int value, int expected, int tolerance)
    {
        return std::abs(value - expected) <= tolerance;
    }

    // A small image in four colors: red corners, green edges, and a blue middle,
    // each two pixels wide, for slicing.
    ImageData Frame()
    {
        ImageData image;
        image.Width = 6;
        image.Height = 6;
        image.Pixels.resize(6 * 6 * 4);
        for (int y = 0; y < 6; ++y)
        {
            for (int x = 0; x < 6; ++x)
            {
                bool edgeX = x < 2 || x >= 4;
                bool edgeY = y < 2 || y >= 4;
                Color color = edgeX && edgeY ? Color { 1, 0, 0, 1 } : edgeX || edgeY ? Color { 0, 1, 0, 1 } : Color { 0, 0, 1, 1 };
                std::size_t at = static_cast<std::size_t>(y * 6 + x) * 4;
                image.Pixels[at + 0] = static_cast<std::uint8_t>(color.Red * 255.0f);
                image.Pixels[at + 1] = static_cast<std::uint8_t>(color.Green * 255.0f);
                image.Pixels[at + 2] = static_cast<std::uint8_t>(color.Blue * 255.0f);
                image.Pixels[at + 3] = 255;
            }
        }
        return image;
    }
}

EASYFORGE_TEST(GradientsRunAcrossShapes)
{
    Renderer renderer = Offscreen(101, 40);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 101, 20 },
            .Gradient = LinearGradient { .From = Color::Black, .To = Color::White, .Angle = 0 } });
        canvas.Rectangle({ .Position = { 0, 20 }, .Size = { 101, 20 },
            .Gradient = LinearGradient { .From = Color::Hex("#FF0000"), .To = Color::Hex("#0000FF"), .Angle = 180 } });
    });
    EASYFORGE_EXPECT(Channel(image, 0, 10, 0) < 6);
    EASYFORGE_EXPECT(Near(Channel(image, 50, 10, 0), 128, 4));
    EASYFORGE_EXPECT(Channel(image, 100, 10, 0) > 249);

    // 180 degrees runs from right to left.
    EASYFORGE_EXPECT(Channel(image, 100, 30, 0) > 249);
    EASYFORGE_EXPECT(Channel(image, 0, 30, 2) > 249);

    // Top to bottom, on a circle.
    Renderer round = Offscreen(40, 40);
    ImageData circle = Picture(round, [](Canvas& canvas) {
        canvas.Circle({ 20, 20 }, 20, { .Gradient = LinearGradient { .From = Color::White, .To = Color::Black } });
    });
    EASYFORGE_EXPECT(Channel(circle, 20, 2, 0) > Channel(circle, 20, 37, 0) + 150);
}

EASYFORGE_TEST(BlurSoftensEdges)
{
    Renderer renderer = Offscreen(120, 60);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 30, 10 }, .Size = { 60, 40 }, .Blur = 10 });
    });
    // Half covered on the edge, fully inside, and faded to nothing outside.
    EASYFORGE_EXPECT(Near(Channel(image, 30, 30, 0), 128, 12));
    EASYFORGE_EXPECT(Channel(image, 60, 30, 0) > 250);
    EASYFORGE_EXPECT(Channel(image, 15, 30, 0) < 6);
    EASYFORGE_EXPECT(Channel(image, 25, 30, 0) > 20);
    EASYFORGE_EXPECT(Channel(image, 25, 30, 0) < 128);

    // Without blur the edge is sharp.
    ImageData sharp = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 30, 10 }, .Size = { 60, 40 } });
    });
    EASYFORGE_EXPECT(Channel(sharp, 25, 30, 0) == 0);
}

EASYFORGE_TEST(SlicedImagesKeepTheirCorners)
{
    Texture frame = Texture::FromImage(Frame(), { .Smooth = false });
    Renderer renderer = Offscreen(40, 30);
    ImageData image = Picture(renderer, [&](Canvas& canvas) {
        canvas.Image(frame, { .Position = { 0, 0 }, .Size = { 40, 30 }, .Slice = 2 });
    });
    auto color = [&](int x, int y) { return image.ColorAt(x, y).ToHex(); };
    EASYFORGE_EXPECT_EQUAL(color(0, 0), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(color(1, 1), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(color(38, 28), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(color(20, 0), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(color(0, 15), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(color(3, 3), std::string("#0000FF"));
    EASYFORGE_EXPECT_EQUAL(color(20, 15), std::string("#0000FF"));
}

EASYFORGE_TEST(BlurBehindBlursWhatIsUnder)
{
    Renderer renderer = Offscreen(100, 60);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        // White on the left half, black on the right, and a frosted panel across
        // the middle with a line drawn on top of it.
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 50, 60 } });
        canvas.BlurBehind({ 20, 10, 60, 40 }, 8);
        canvas.Rectangle({ .Position = { 20, 29 }, .Size = { 60, 2 }, .Color = Color::Hex("#FF0000") });
    });
    // The edge between white and black is soft inside the panel.
    int middle = Channel(image, 50, 20, 1);
    EASYFORGE_EXPECT(Near(middle, 128, 20));
    EASYFORGE_EXPECT(Channel(image, 45, 20, 1) > middle);
    EASYFORGE_EXPECT(Channel(image, 55, 20, 1) < middle);

    // Outside the panel it stays sharp.
    EASYFORGE_EXPECT(Channel(image, 49, 5, 1) > 250);
    EASYFORGE_EXPECT(Channel(image, 50, 5, 1) < 5);

    // What is drawn afterwards covers the blur.
    EASYFORGE_EXPECT(Channel(image, 50, 30, 0) > 250);
    EASYFORGE_EXPECT(Channel(image, 50, 30, 1) < 5);
}

EASYFORGE_TEST(BlurBehindWorksInLayersAndAtScale)
{
    // Two pixels a point. A white strip reaches two points into the blurred area.
    Renderer renderer = Offscreen(200, 120, 2.0f);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.BeginLayer({ 0, 0, 100, 60 });
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 22, 60 } });
        canvas.BlurBehind({ 20, 10, 60, 40 }, 8, 6);
        canvas.EndLayer({ .Opacity = 1.0f });
    });
    // Inside the area, the strip's edge is blurred into the black beside it.
    EASYFORGE_EXPECT(Channel(image, 42, 60, 1) < 230);
    EASYFORGE_EXPECT(Channel(image, 42, 60, 1) > 60);
    EASYFORGE_EXPECT(Channel(image, 52, 60, 1) > 20);

    // The rounded corner leaves the corner of the area as it was: white.
    EASYFORGE_EXPECT(Channel(image, 41, 21, 1) > 250);
}

EASYFORGE_TEST(HolesBlurStrengthAndSlicesAtFractions)
{
    // A shape with a hole leaves the hole undrawn, even where it is soft.
    Renderer renderer = Offscreen(100, 60);
    ImageData holed = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 10, 10 }, .Size = { 80, 40 }, .Color = Color::White, .Blur = 8,
            .Hole = easyforge::Rectangle { 20, 20, 60, 20 }, .HoleCornerRadius = 4 });
    });
    EASYFORGE_EXPECT(Channel(holed, 50, 30, 0) < 3);
    EASYFORGE_EXPECT(Channel(holed, 14, 30, 0) > 60);

    // A blur at half strength is between the sharp picture and the full blur.
    ImageData half = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 50, 60 } });
        canvas.BlurBehind({ 20, 10, 60, 40 }, 8, 0.0f, 0.5f);
    });
    int edge = Channel(half, 46, 30, 1);
    EASYFORGE_EXPECT(edge < 252 && edge > 160);

    // Sliced pieces at a fractional position meet without a see-through seam.
    Texture frame = Texture::FromImage(Frame(), { .Smooth = false });
    Renderer sliced = Offscreen(60, 40);
    ImageData image = Picture(sliced, [&](Canvas& canvas) {
        canvas.Image(frame, { .Position = { 0.5f, 0.5f }, .Size = { 40.3f, 30.6f }, .Slice = 2 });
    }, Color::White);
    for (int x = 4; x < 38; ++x)
    {
        EASYFORGE_EXPECT(image.ColorAt(x, 15).Red < 0.05f);
    }

    // The clip area follows clips and transforms.
    Renderer clipped = Offscreen(100, 100);
    Picture(clipped, [](Canvas& canvas) {
        canvas.PushClip({ 10, 20, 30, 40 });
        canvas.PushTransform({ 10, 0 }, 2.0f);
        easyforge::Rectangle area = canvas.ClipArea();
        EASYFORGE_EXPECT_EQUAL(area.X, 0.0f);
        EASYFORGE_EXPECT_EQUAL(area.Y, 10.0f);
        EASYFORGE_EXPECT_EQUAL(area.Width, 15.0f);
        EASYFORGE_EXPECT_EQUAL(area.Height, 20.0f);
        canvas.PopTransform();
        canvas.PopClip();
    });

    // A layer far larger than any picture is cut down to what shows.
    Renderer large = Offscreen(100, 100);
    ImageData big = Picture(large, [](Canvas& canvas) {
        canvas.BeginLayer({ 0, -20000, 100, 40000 });
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 100, 100 }, .Color = Color::Hex("#00FF00") });
        canvas.EndLayer({ .Opacity = 0.5f });
    });
    EASYFORGE_EXPECT(Near(Channel(big, 50, 50, 1), 128, 8));
}
