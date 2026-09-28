#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>
#include <easyforge/window.h>

#include <cstdlib>

using namespace easyforge;

// Drawing into a real window, which only runs when the window library is built.

EASYFORGE_TEST(RendererDrawsIntoAWindow)
{
    Window window = Window::New({ .Title = "graphics test", .Width = 100, .Height = 80, .Visible = false });
    EASYFORGE_REQUIRE(window);
    Renderer renderer = Renderer::New(window, { .Adapter = GraphicsAdapter::Software });
    EASYFORGE_REQUIRE(renderer);
    EASYFORGE_EXPECT_EQUAL(renderer.PixelSize(), window.PixelSize());
    EASYFORGE_EXPECT_EQUAL(renderer.Scale(), window.Scale());

    // A window has one renderer.
    Renderer second = Renderer::New(window, { .Adapter = GraphicsAdapter::Software });
    EASYFORGE_EXPECT(!second);
    EASYFORGE_EXPECT(second.Error().find("already has a renderer") != std::string::npos);

    Canvas canvas = renderer.BeginFrame(Color::Hex("#224466"));
    canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 10, 10 }, .Color = Color::White });
    renderer.EndFrame();
    ImageData shown = renderer.Capture();
    EASYFORGE_REQUIRE(shown);
    EASYFORGE_EXPECT_EQUAL(shown.Width, static_cast<int>(window.PixelSize().X));
    EASYFORGE_EXPECT_EQUAL(shown.ColorAt(50, 50).ToHex(), std::string("#224466"));
    EASYFORGE_EXPECT_EQUAL(shown.ColorAt(1, 1).ToHex(), std::string("#FFFFFF"));

    // The next frame follows the window's new size.
    window.Size = { 120, 90 };
    renderer.BeginFrame(Color::Hex("#224466"));
    renderer.EndFrame();
    EASYFORGE_EXPECT_EQUAL(renderer.Capture().Width, static_cast<int>(window.PixelSize().X));

    // Frames run by the window draw through OnFrame as usual.
    int frames = 0;
    window.OnFrame = [&](float) {
        Canvas frame = renderer.BeginFrame(Color::Hex("#446622"));
        frame.Circle({ 20, 20 }, 5);
        renderer.EndFrame();
        if (++frames == 3)
        {
            window.Close();
        }
    };
    window.Run();
    EASYFORGE_EXPECT_EQUAL(frames, 3);

    // Once the window is closed, drawing does nothing and fails nothing.
    renderer.BeginFrame();
    renderer.EndFrame();
}

EASYFORGE_TEST(ReleasingTheRendererFreesTheWindow)
{
    Window window = Window::New({ .Width = 64, .Height = 64, .Visible = false });
    {
        Renderer first = Renderer::New(window, { .Adapter = GraphicsAdapter::Software });
        EASYFORGE_EXPECT(first);
    }
    Renderer second = Renderer::New(window, { .Adapter = GraphicsAdapter::Software });
    EASYFORGE_EXPECT(second);
    window.Close();
}

EASYFORGE_TEST(TransparentWindowsKeepAlpha)
{
    // A see-through window is shown through DirectComposition, and keeps the
    // alpha of what is drawn.
    Window window = Window::New({ .Width = 64, .Height = 64, .Visible = false, .Transparent = true });
    Renderer renderer = Renderer::New(window, { .Adapter = GraphicsAdapter::Software });
    EASYFORGE_REQUIRE(renderer);
    Canvas canvas = renderer.BeginFrame(Color::Transparent);
    canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 32, 64 }, .Color = Color::Hex("#FF0000").WithAlpha(0.5f) });
    renderer.EndFrame();
    ImageData shown = renderer.Capture();
    EASYFORGE_REQUIRE(shown);
    EASYFORGE_EXPECT(std::abs(static_cast<int>(shown.Pixels[3]) - 128) <= 1);
    EASYFORGE_EXPECT_EQUAL(shown.ColorAt(48, 32).Alpha, 0.0f);
    window.Close();
}
