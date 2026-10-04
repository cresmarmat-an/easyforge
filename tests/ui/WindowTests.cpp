#include "Helpers.h"

#include <easyforge/window.h>

using namespace easyforge;
using namespace easyforge::testing;

// An interface in a real window, which only runs when the window library is built.

EASYFORGE_TEST(WindowsShowInterfaces)
{
    Window window = Window::New({ .Title = "ui test", .Width = 320, .Height = 240, .Visible = false });
    EASYFORGE_REQUIRE(window);
    ui::Root::Of(window).Theme = TestTheme();

    ui::Label title("A", { .Name = "Title" });
    ui::Button save("O", { .Name = "Save", .Width = 60 });
    ui::Column content({ .Padding = 20, .Gap = 10, .Alignment = ui::Alignment::Start, .Children = { title, save } });
    window.Content = content;

    int frames = 0;
    window.OnFrame = [&](float) {
        if (++frames < 3)
        {
            return;
        }
        // The content fills the window, and its elements are laid out.
        EASYFORGE_EXPECT_EQUAL(content.Frame().Size(), (Vector2 { 320, 240 }));
        EASYFORGE_EXPECT_EQUAL(title.Frame().Position(), (Vector2 { 20, 20 }));
        EASYFORGE_EXPECT(ui::Root::Of(window).Find<ui::Button>("Save") == save);
        EASYFORGE_EXPECT(ui::Root::Of(window).Content.Get() == content);

        // A second renderer is refused while the interface draws into the window.
        Renderer renderer = Renderer::New(window, { .Adapter = GraphicsAdapter::Software });
        EASYFORGE_EXPECT(!renderer);
        window.Close();
    };
    window.Run();
    EASYFORGE_EXPECT_EQUAL(frames, 3);

    // Once the window has closed, the elements leave its root and keep their settings.
    EASYFORGE_EXPECT(!ui::Root::Of(window).Find<ui::Button>("Save"));
    EASYFORGE_EXPECT_EQUAL(save.Name.Get(), std::string("Save"));
}

EASYFORGE_TEST(TitleBarsTellTheWindowWhatIsWhere)
{
    Window window = Window::New({ .Title = "ui test", .Width = 400, .Height = 200, .Visible = false });
    EASYFORGE_REQUIRE(window);
    ui::Root::Of(window).Theme = TestTheme();

    int clicks = 0;
    ui::Button menu("A", { .Width = 40, .OnClick = [&] { ++clicks; } });
    ui::WindowButtons buttons;
    std::shared_ptr<View> bar = ui::TitleBar({ .Height = 40, .Children = { menu, ui::Label("O"), ui::Spacer(), buttons } });
    window.TitleBar = bar;
    window.Content = ui::Column();

    EASYFORGE_EXPECT_EQUAL(bar->PreferredSize({ 400, 200 }).Y, 40.0f);
    int frames = 0;
    window.OnFrame = [&](float) {
        if (++frames < 2)
        {
            return;
        }
        // The window buttons are the last 138 points: three of 46.
        EASYFORGE_EXPECT_EQUAL(buttons.Frame(), (Rectangle { 262, 0, 138, 40 }));
        EASYFORGE_EXPECT(bar->HitTest({ 380, 20 }) == HitArea::CloseButton);
        EASYFORGE_EXPECT(bar->HitTest({ 330, 20 }) == HitArea::MaximizeButton);
        EASYFORGE_EXPECT(bar->HitTest({ 280, 20 }) == HitArea::MinimizeButton);

        // Controls are the window's content; the rest of the bar drags it.
        EASYFORGE_EXPECT(bar->HitTest(menu.Frame().Center()) == HitArea::Content);
        EASYFORGE_EXPECT(bar->HitTest({ 200, 20 }) == HitArea::Caption);

        // Below the bar is the content.
        EASYFORGE_EXPECT(bar->HitTest({ 200, 100 }) == HitArea::Content);
        window.Close();
    };
    window.Run();
    EASYFORGE_EXPECT_EQUAL(frames, 2);
}

EASYFORGE_TEST(TitleBarsThatChangeSizeMoveTheContent)
{
    Window window = Window::New({ .Title = "ui test", .Width = 400, .Height = 200, .Visible = false });
    EASYFORGE_REQUIRE(window);
    ui::Root::Of(window).Theme = TestTheme();

    ui::TitleBar bar({ .Height = 40 });
    ui::Column content;
    window.TitleBar = bar;
    window.Content = content;

    int frames = 0;
    window.OnFrame = [&](float) {
        ++frames;
        if (frames == 2)
        {
            EASYFORGE_EXPECT_EQUAL(content.Frame().Y, 40.0f);
            bar.Height = 60;
        }
        else if (frames == 5)
        {
            // The window places its views again once the bar asks for more room.
            EASYFORGE_EXPECT_EQUAL(bar.Frame().Height, 60.0f);
            EASYFORGE_EXPECT_EQUAL(content.Frame().Y, 60.0f);
            bar.Visible = false;
        }
        else if (frames == 8)
        {
            EASYFORGE_EXPECT_EQUAL(content.Frame().Y, 0.0f);
            window.Close();
        }
    };
    window.Run();
    EASYFORGE_EXPECT_EQUAL(frames, 8);
}
