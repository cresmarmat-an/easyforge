#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>

#include <cstdlib>

using namespace easyforge;

namespace
{
    // An image renderer on the software adapter, which draws the same pixels on
    // every computer.
    Renderer Offscreen(int width, int height, float scale = 1.0f)
    {
        return Renderer::New({ .Adapter = GraphicsAdapter::Software, .Width = width, .Height = height, .Scale = scale });
    }

    // Draws one frame on a black background and reads it back.
    template <typename Draw>
    ImageData Picture(Renderer& renderer, Draw draw, Color background = Color::Black)
    {
        Canvas canvas = renderer.BeginFrame(background);
        draw(canvas);
        renderer.EndFrame();
        return renderer.Capture();
    }

    std::string Pixel(const ImageData& image, int x, int y)
    {
        return image.ColorAt(x, y).ToHex();
    }

    int Channel(const ImageData& image, int x, int y, int channel)
    {
        return image.Pixels[static_cast<std::size_t>(y) * image.Stride() + static_cast<std::size_t>(x) * 4 + static_cast<std::size_t>(channel)];
    }
}

EASYFORGE_TEST(RendererClearsAndCaptures)
{
    Renderer renderer = Offscreen(64, 48);
    EASYFORGE_REQUIRE(renderer);
    EASYFORGE_EXPECT(renderer.Description().find("Direct3D 12") != std::string::npos);
    EASYFORGE_EXPECT_EQUAL(renderer.PixelSize(), Vector2(64, 48));

    ImageData image = Picture(renderer, [](Canvas&) {}, Color::Hex("#336699"));
    EASYFORGE_REQUIRE(image.Width == 64 && image.Height == 48);
    for (int y = 0; y < 48; y += 7)
    {
        for (int x = 0; x < 64; x += 5)
        {
            EASYFORGE_EXPECT_EQUAL(Pixel(image, x, y), std::string("#336699"));
        }
    }

    // A frame after it replaces it.
    image = Picture(renderer, [](Canvas&) {}, Color::Hex("#FF8000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 10), std::string("#FF8000"));
}

EASYFORGE_TEST(RectanglesCoverWholePixels)
{
    Renderer renderer = Offscreen(40, 30);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 10, 10 }, .Size = { 20, 10 }, .Color = Color::White });
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 10), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 29, 19), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 9, 10), std::string("#000000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 30, 15), std::string("#000000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 15, 20), std::string("#000000"));

    // Half a pixel in from the edge, a rectangle covers half of each edge pixel.
    image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 10.5f, 10 }, .Size = { 10, 10 }, .Color = Color::White });
    });
    EASYFORGE_EXPECT(std::abs(Channel(image, 10, 15, 0) - 128) <= 2);
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 11, 15), std::string("#FFFFFF"));
}

EASYFORGE_TEST(RectanglesTurnAboutTheirCenter)
{
    Renderer renderer = Offscreen(40, 40);
    // A bar 24 wide and 6 tall around (20, 20), turned a quarter: 6 wide and 24 tall.
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 8, 17 }, .Size = { 24, 6 }, .Rotation = 1.5707964f, .Color = Color::White });
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 10), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 29), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 20), std::string("#000000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 29, 20), std::string("#000000"));

    // An eighth of a turn, with a border: the border follows the turned edges.
    image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 10, 10 }, .Size = { 20, 20 }, .Rotation = 0.7853982f, .Color = Color::White,
            .BorderWidth = 2, .BorderColor = Color::Hex("#FF0000") });
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 20), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 11, 11), std::string("#000000"));   // the unturned corner is empty now
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 7), std::string("#FF0000"));    // just under the turned top corner, in the border
}

EASYFORGE_TEST(RoundedCornersAndBorders)
{
    Renderer renderer = Offscreen(40, 40);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 4, 4 }, .Size = { 32, 32 }, .Color = Color::White, .CornerRadius = 10,
            .BorderWidth = 3, .BorderColor = Color::Hex("#FF0000") });
    });
    // The very corner is cut away; the middle is the fill; along an edge is the border.
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 4, 4), std::string("#000000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 20), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 5), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 5, 20), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 8), std::string("#FFFFFF"));
    // The rounded corner's edge is smooth: the pixels it crosses are partly covered.
    int partial = 0;
    for (int y = 4; y < 14; ++y)
    {
        for (int x = 4; x < 14; ++x)
        {
            int value = Channel(image, x, y, 0);
            partial += value > 20 && value < 235 ? 1 : 0;
        }
    }
    EASYFORGE_EXPECT(partial >= 4);
}

EASYFORGE_TEST(CirclesAndLines)
{
    Renderer renderer = Offscreen(40, 40);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Circle({ 20, 20 }, 10, { .Color = Color::Hex("#00FF00") });
        canvas.Line({ 0, 5 }, { 40, 5 }, { .Color = Color::White, .Width = 2 });
        canvas.Line({ 35, 0 }, { 35, 40 }, { .Color = Color::Hex("#0000FF"), .Width = 1 });
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 20), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 11), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 12, 12), std::string("#000000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 31), std::string("#000000"));

    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 4), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 5), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 6), std::string("#000000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 3), std::string("#000000"));

    // A one-point line centered on a pixel edge covers half of two pixels.
    EASYFORGE_EXPECT(std::abs(Channel(image, 34, 20, 2) - 128) <= 2);
    EASYFORGE_EXPECT(std::abs(Channel(image, 35, 20, 2) - 128) <= 2);
}

EASYFORGE_TEST(TransparencyBlends)
{
    Renderer renderer = Offscreen(20, 20);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 20, 20 }, .Color = Color::Hex("#FF0000") });
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 10, 20 }, .Color = Color::Hex("#0000FF").WithAlpha(0.5f) });
    });
    EASYFORGE_EXPECT(std::abs(Channel(image, 5, 5, 0) - 128) <= 1);
    EASYFORGE_EXPECT(std::abs(Channel(image, 5, 5, 2) - 128) <= 1);
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 15, 5), std::string("#FF0000"));

    // Captured images have straight alpha, as ImageData does.
    image = Picture(renderer, [](Canvas& canvas) {
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 20, 20 }, .Color = Color::Hex("#FF0000").WithAlpha(0.5f) });
    }, Color::Transparent);
    EASYFORGE_EXPECT(std::abs(Channel(image, 5, 5, 3) - 128) <= 1);
    EASYFORGE_EXPECT(Channel(image, 5, 5, 0) >= 254);
}

EASYFORGE_TEST(ImagesDrawTheirPixels)
{
    ImageData checker(2, 2);
    checker.SetColorAt(0, 0, Color::Hex("#FF0000"));
    checker.SetColorAt(1, 0, Color::Hex("#00FF00"));
    checker.SetColorAt(0, 1, Color::Hex("#0000FF"));
    checker.SetColorAt(1, 1, Color::White);
    Texture sharp = Texture::FromImage(checker, { .Smooth = false });
    EASYFORGE_REQUIRE(sharp);
    EASYFORGE_EXPECT_EQUAL(sharp.Width(), 2);

    Renderer renderer = Offscreen(20, 20);
    ImageData image = Picture(renderer, [&](Canvas& canvas) {
        canvas.Image(sharp, { .Position = { 2, 2 } });
        canvas.Image(sharp, { .Position = { 10, 10 }, .Size = { 8, 8 } });
        canvas.Image(sharp, { .Position = { 2, 12 }, .Source = { 1, 1, 1, 1 } });
    });
    // One pixel of the texture for each point, at its own size.
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 2, 2), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 3, 2), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 2, 3), std::string("#0000FF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 3, 3), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 4, 2), std::string("#000000"));
    // Stretched without smoothing, each texture pixel becomes a block.
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 10), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 13, 13), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 14, 10), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 17, 17), std::string("#FFFFFF"));
    // Part of the texture.
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 2, 12), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 3, 12), std::string("#000000"));

    // Updating a texture shows the new pixels in the next frame.
    ImageData yellow(2, 2);
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            yellow.SetColorAt(x, y, Color::Hex("#FFFF00"));
        }
    }
    sharp.Update(yellow);
    image = Picture(renderer, [&](Canvas& canvas) { canvas.Image(sharp, { .Position = { 2, 2 } }); });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 2, 2), std::string("#FFFF00"));

    // A tint's alpha fades the image.
    image = Picture(renderer, [&](Canvas& canvas) {
        canvas.Image(sharp, { .Position = { 2, 2 }, .Tint = Color::White.WithAlpha(0.5f) });
    });
    EASYFORGE_EXPECT(std::abs(Channel(image, 2, 2, 0) - 128) <= 1);
    EASYFORGE_EXPECT(std::abs(Channel(image, 2, 2, 1) - 128) <= 1);

    // Tinting multiplies every pixel.
    image = Picture(renderer, [&](Canvas& canvas) {
        canvas.Image(sharp, { .Position = { 2, 2 }, .Tint = Color::Hex("#FF0000") });
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 2, 2), std::string("#FF0000"));
}

EASYFORGE_TEST(ClipsAndTransforms)
{
    Renderer renderer = Offscreen(40, 40);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        canvas.PushClip({ 0, 0, 10, 10 });
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 20, 20 }, .Color = Color::White });
        canvas.PushClip({ 5, 5, 20, 20 });
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 40, 40 }, .Color = Color::Hex("#FF0000") });
        canvas.PopClip();
        canvas.PopClip();

        canvas.PushTransform({ 20, 20 }, 2.0f);
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 5, 5 }, .Color = Color::Hex("#00FF00") });
        canvas.PushTransform({ 5, 0 });
        canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 1, 1 }, .Color = Color::Hex("#0000FF") });
        canvas.PopTransform();
        canvas.PopTransform();
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 2, 2), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 7, 7), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 12, 2), std::string("#000000"));
    // Moved to (20, 20) and twice the size: 10 by 10 pixels.
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 20), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 29, 29), std::string("#00FF00"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 30, 25), std::string("#000000"));
    // Nested: (5, 0) inside the scaled transform lands at 20 + 5 * 2 = 30.
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 30, 20), std::string("#0000FF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 31, 21), std::string("#0000FF"));
}

EASYFORGE_TEST(PointsBecomePixelsAtTheScale)
{
    Renderer renderer = Offscreen(40, 40, 2.0f);
    EASYFORGE_EXPECT_EQUAL(renderer.Size(), Vector2(20, 20));
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        EASYFORGE_EXPECT_EQUAL(canvas.Size(), Vector2(20, 20));
        EASYFORGE_EXPECT_EQUAL(canvas.Scale(), 2.0f);
        canvas.Rectangle({ .Position = { 5, 5 }, .Size = { 5, 5 }, .Color = Color::White });
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 10, 10), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 19, 19), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 20, 20), std::string("#000000"));
}

EASYFORGE_TEST(ManyShapesInOneFrame)
{
    Renderer renderer = Offscreen(100, 100);
    ImageData image = Picture(renderer, [](Canvas& canvas) {
        for (int y = 0; y < 100; ++y)
        {
            for (int x = 0; x < 100; ++x)
            {
                canvas.Rectangle({ .Position = { static_cast<float>(x), static_cast<float>(y) }, .Size = { 1, 1 },
                    .Color = (x + y) % 2 == 0 ? Color::White : Color::Hex("#FF0000") });
            }
        }
    });
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 0, 0), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 1, 0), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 99, 99), std::string("#FFFFFF"));
    EASYFORGE_EXPECT_EQUAL(Pixel(image, 98, 99), std::string("#FF0000"));
}

EASYFORGE_TEST(RendererErrors)
{
    Renderer none;
    EASYFORGE_EXPECT(!none);
    Canvas nothing = none.BeginFrame();
    EASYFORGE_EXPECT(!nothing);
    nothing.Rectangle({ .Size = { 10, 10 } });
    none.EndFrame();
    EASYFORGE_EXPECT(!none.Capture());

    Renderer sizeless = Renderer::New({ .Adapter = GraphicsAdapter::Software });
    EASYFORGE_EXPECT(!sizeless);
    EASYFORGE_EXPECT(sizeless.Error().find("Width") != std::string::npos);

    Renderer hostless = Renderer::New(nullptr, { .Width = 10, .Height = 10 });
    EASYFORGE_EXPECT(!hostless);
}
