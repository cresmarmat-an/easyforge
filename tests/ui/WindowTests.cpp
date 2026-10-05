#include "Helpers.h"

#include <chrono>
#include <cmath>
#include <thread>

#include <easyforge/data.h>
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

EASYFORGE_TEST(InterfacesAskForFramesOnlyWhileSomethingChanges)
{
    Window window = Window::New({ .Title = "ui test", .Width = 320, .Height = 240, .Visible = false });
    EASYFORGE_REQUIRE(window);
    ui::Root::Of(window).Theme = TestTheme();

    Table game = Table::New();
    Node player = game.Add("Player");
    player["Health"] = 100;
    ui::Label title("A");
    ui::Label health(ui::Bind(player["Health"], "{}"));
    ui::TextField field({ .Width = 120 });
    window.Content = ui::Column({ .Padding = 20, .Gap = 10, .Children = { title, health, field } });
    std::shared_ptr<View> view = window.Content.Get();

    auto settle = [] {
        // Frames keep coming for a moment after anything changes.
        Window::RunOneFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    };
    settle();
    EASYFORGE_EXPECT_EQUAL(health.Text.Get(), std::string("100"));

    // Still, and showing data: the data is checked a few times a second.
    float wait = view->SecondsUntilNextFrame();
    EASYFORGE_EXPECT(wait > 0.0f && wait <= 0.25f);

    // A change to the data is found without a frame, and asks for one.
    player["Health"] = 90;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EASYFORGE_EXPECT_EQUAL(view->SecondsUntilNextFrame(), 0.0f);
    settle();
    EASYFORGE_EXPECT_EQUAL(health.Text.Get(), std::string("90"));

    // Without data to follow, a still interface needs no frames at all.
    health.Text = "Gone";
    EASYFORGE_EXPECT_EQUAL(view->SecondsUntilNextFrame(), 0.0f);
    settle();
    EASYFORGE_EXPECT(std::isinf(view->SecondsUntilNextFrame()));

    // A change to an element asks for a frame at once.
    title.Text = "B";
    EASYFORGE_EXPECT_EQUAL(view->SecondsUntilNextFrame(), 0.0f);
    settle();

    // A text field with the keyboard blinks its caret, twice a second.
    field.Focus();
    settle();
    wait = view->SecondsUntilNextFrame();
    EASYFORGE_EXPECT(wait <= 0.5f);

    // An animation keeps frames coming until it ends.
    title.Opacity.AnimateTo(0.5f, { .Duration = 0.5f });
    Window::RunOneFrame();
    EASYFORGE_EXPECT_EQUAL(view->SecondsUntilNextFrame(), 0.0f);
    window.Close();
}
