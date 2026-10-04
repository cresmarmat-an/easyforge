#include <algorithm>
#include <vector>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(ButtonsClick)
{
    int clicks = 0;
    ui::Button button("OK", { .Width = 100, .Height = 40, .OnClick = [&] { ++clicks; } });
    Screen screen(200, 200, ui::Column({ .Padding = 10, .Alignment = ui::Alignment::Start, .Children = { button } }));
    screen.Frame();

    EASYFORGE_EXPECT(screen.Click({ 50, 30 }));
    EASYFORGE_EXPECT_EQUAL(clicks, 1);

    // Empty space in a column without a background lets events through.
    EASYFORGE_EXPECT(!screen.Click({ 150, 150 }));
    EASYFORGE_EXPECT_EQUAL(clicks, 1);

    // Letting go outside is not a click.
    screen.Press({ 50, 30 });
    EASYFORGE_EXPECT(button.IsPressed());
    screen.Move({ 150, 150 });
    screen.Release({ 150, 150 });
    EASYFORGE_EXPECT(!button.IsPressed());
    EASYFORGE_EXPECT_EQUAL(clicks, 1);

    // A disabled button takes the click, and does nothing with it.
    button.Enabled = false;
    EASYFORGE_EXPECT(screen.Click({ 50, 30 }));
    EASYFORGE_EXPECT_EQUAL(clicks, 1);
    button.Enabled = true;

    button.Click();
    EASYFORGE_EXPECT_EQUAL(clicks, 2);
}

EASYFORGE_TEST(BoxesTakeTheirClicks)
{
    ui::Panel panel({ .Width = 100, .Height = 100 });
    ui::Label label("A");
    Screen screen(300, 300, ui::Row({ .Alignment = ui::Alignment::Start, .Children = { panel, label } }));
    screen.Frame();
    // A panel has the theme's surface behind it, so clicks on it stay with the
    // interface; a label has no box.
    EASYFORGE_EXPECT(screen.Click({ 50, 50 }));
    EASYFORGE_EXPECT(!screen.Click({ label.Frame().Center().X, label.Frame().Center().Y }));
}

EASYFORGE_TEST(DrawingAreasFollowThePointer)
{
    std::vector<Vector2> presses;
    std::vector<Vector2> moves;
    std::vector<Vector2> releases;
    ui::DrawingArea plain({ .Width = 100, .Height = 100 });
    ui::DrawingArea area({
        .Width = 100,
        .Height = 100,
        .Padding = 10,
        .OnPress = [&](Vector2 point) { presses.push_back(point); },
        .OnMove = [&](Vector2 point) { moves.push_back(point); },
        .OnRelease = [&](Vector2 point) { releases.push_back(point); },
    });
    Screen screen(300, 300, ui::Row({ .Padding = 20, .Alignment = ui::Alignment::Start, .Children = { plain, area } }));
    screen.Frame();

    // Without pointer functions an area lets clicks through.
    EASYFORGE_EXPECT(!screen.Click({ 70, 70 }));

    // Points count from inside the padding, as OnDraw's do, and the area keeps
    // the pointer while it is held.
    EASYFORGE_EXPECT(screen.Press({ 150, 60 }));
    screen.Move({ 170, 80 });
    screen.Move({ 290, 290 });
    screen.Release({ 290, 290 });
    EASYFORGE_REQUIRE(presses.size() == 1);
    EASYFORGE_EXPECT_EQUAL(presses[0], (Vector2 { 20, 30 }));
    EASYFORGE_REQUIRE(moves.size() >= 2);
    EASYFORGE_EXPECT(std::find(moves.begin(), moves.end(), Vector2 { 40, 50 }) != moves.end());
    EASYFORGE_EXPECT_EQUAL(moves.back(), (Vector2 { 160, 260 }));
    EASYFORGE_REQUIRE(releases.size() == 1);
    EASYFORGE_EXPECT_EQUAL(releases[0], (Vector2 { 160, 260 }));

    // Set later, as properties.
    int later = 0;
    plain.OnPress = [&](Vector2) { ++later; };
    EASYFORGE_EXPECT(screen.Click({ 70, 70 }));
    EASYFORGE_EXPECT_EQUAL(later, 1);
}

EASYFORGE_TEST(TabMovesTheKeyboard)
{
    int first = 0;
    int second = 0;
    ui::Button one("A", { .OnClick = [&] { ++first; } });
    ui::Button two("O", { .OnClick = [&] { ++second; } });
    ui::Button off("X", { .Enabled = false });
    Screen screen(300, 300, ui::Column({ .Children = { one, off, two } }));
    screen.Frame();

    EASYFORGE_EXPECT(screen.Key(Key::Tab));
    EASYFORGE_EXPECT(one.IsFocused());
    EASYFORGE_EXPECT(screen.Key(Key::Enter));
    EASYFORGE_EXPECT_EQUAL(first, 1);

    // Disabled elements are skipped.
    screen.Key(Key::Tab);
    EASYFORGE_EXPECT(two.IsFocused());
    screen.Key(Key::Space);
    EASYFORGE_EXPECT_EQUAL(second, 1);
    screen.Key(Key::Tab, { .Shift = true });
    EASYFORGE_EXPECT(one.IsFocused());
    EASYFORGE_EXPECT(screen.Root.Focused() == one);

    screen.Key(Key::Escape);
    EASYFORGE_EXPECT(!one.IsFocused());
    EASYFORGE_EXPECT(!screen.Key(Key::Enter));

    // Clicking gives the keyboard to what was clicked; clicking empty space
    // takes it away.
    screen.Click(two.Frame().Center());
    EASYFORGE_EXPECT(two.IsFocused());
    screen.Click({ 290, 290 });
    EASYFORGE_EXPECT(!two.IsFocused());
    two.Focus();
    EASYFORGE_EXPECT(two.IsFocused());
}

EASYFORGE_TEST(CheckboxesAndTogglesChange)
{
    std::vector<bool> changes;
    ui::Checkbox check("A", { .OnChange = [&](bool checked) { changes.push_back(checked); } });
    ui::Toggle toggle("O", { .Checked = true });
    Screen screen(300, 300, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { check, toggle } }));
    screen.Frame();

    screen.Click(check.Frame().Center());
    EASYFORGE_EXPECT(check.Checked.Get());
    check.Focus();
    screen.Key(Key::Space);
    EASYFORGE_EXPECT(!check.Checked.Get());
    check.Checked = true;
    EASYFORGE_EXPECT_EQUAL(changes, (std::vector<bool> { true, false, true }));

    // Setting the same value again is not a change.
    check.Checked = true;
    EASYFORGE_EXPECT_EQUAL(changes.size(), std::size_t { 3 });

    screen.Click(toggle.Frame().Center());
    EASYFORGE_EXPECT(!toggle.Checked.Get());
}

EASYFORGE_TEST(SlidersFollowThePointerAndKeys)
{
    std::vector<float> values;
    ui::Slider slider({ .Value = 2, .Maximum = 10, .Step = 1, .OnChange = [&](float value) { values.push_back(value); } });
    Screen screen(300, 100, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { slider } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(slider.Frame(), (Rectangle { 0, 0, 200, 20 }));

    // The track runs from 10 to 190: the middle is 5.
    screen.Move({ 100, 10 });
    screen.Press({ 100, 10 });
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 5.0f);
    screen.Move({ 300, 10 });
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 10.0f);
    screen.Release({ 300, 10 });
    screen.Move({ 0, 10 });
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 10.0f);

    screen.Key(Key::Left);
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 9.0f);
    screen.Key(Key::Home);
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 0.0f);

    // Values stay in range and on a step.
    slider.Value = 3.4f;
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 3.0f);
    slider.Value = 40.0f;
    EASYFORGE_EXPECT_EQUAL(slider.Value.Get(), 10.0f);
    EASYFORGE_EXPECT_EQUAL(values, (std::vector<float> { 5, 10, 9, 0, 3, 10 }));
}

EASYFORGE_TEST(ScrollingMovesTheContent)
{
    std::vector<ui::Element> rows;
    std::vector<ui::Spacer> spacers;
    for (int index = 0; index < 4; ++index)
    {
        ui::Spacer row;
        row.Height = 60;
        spacers.push_back(row);
        rows.push_back(row);
    }
    ui::Scroll scroll({ .Width = 200, .Height = 100, .Children = rows });
    Screen screen(200, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { scroll } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(scroll.Frame().Height, 100.0f);
    EASYFORGE_EXPECT_EQUAL(spacers[1].Frame().Y, 60.0f);

    // One notch toward the person scrolls down, smoothly.
    EASYFORGE_EXPECT(screen.Wheel({ 50, 50 }, { 0, -1 }));
    screen.Run(0.5f);
    EASYFORGE_EXPECT_EQUAL(scroll.Position.Get(), 48.0f);
    EASYFORGE_EXPECT_EQUAL(spacers[1].Frame().Y, 12.0f);

    // It stops at the end: 240 points of content in a 100 point view.
    for (int notch = 0; notch < 10; ++notch)
    {
        screen.Wheel({ 50, 50 }, { 0, -1 });
    }
    screen.Run(0.5f);
    EASYFORGE_EXPECT_EQUAL(scroll.Position.Get(), 140.0f);

    // At the end the scroll stays put; the wheel was still over the interface.
    EASYFORGE_EXPECT(screen.Wheel({ 50, 50 }, { 0, -1 }));
    screen.Run(0.2f);
    EASYFORGE_EXPECT_EQUAL(scroll.Position.Get(), 140.0f);
    scroll.Position = 0;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(spacers[0].Frame().Y, 0.0f);

    scroll.ScrollIntoView(spacers[3]);
    screen.Run(0.5f);
    EASYFORGE_EXPECT_EQUAL(scroll.Position.Get(), 140.0f);
}

EASYFORGE_TEST(HoverAndTooltips)
{
    ui::Button button("A", { .Width = 80, .Height = 30, .Tooltip = "Adds one" });
    Screen screen(300, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { button } }));
    screen.Frame();
    screen.Move({ 40, 15 });
    EASYFORGE_EXPECT(button.IsHovered());

    // The tooltip shows after the pointer rests.
    ImageData before = screen.Picture();
    screen.Run(1.0f);
    ImageData after = screen.Picture();
    ui::Theme theme = TestTheme();
    EASYFORGE_EXPECT(!NearColor(before.ColorAt(55, 50), theme.Tooltip));
    EASYFORGE_EXPECT(NearColor(after.ColorAt(55, 50), theme.Tooltip));

    screen.Send(EventType::MouseLeft);
    EASYFORGE_EXPECT(!button.IsHovered());
}

EASYFORGE_TEST(ElementsAddedByCallbacksWork)
{
    ui::Column items({ .Gap = 6 });
    ui::TextField entry({ .Width = 200 });
    entry.OnSubmit = [items, entry](const std::string& text) {
        items.Add(ui::Checkbox(text));
        entry.Text = "";
    };
    Screen screen(400, 300, ui::Column({ .Padding = 20, .Gap = 10, .Children = { entry, ui::Scroll({ .Height = ui::Fill, .Children = { items } }) } }));
    screen.Frame();
    entry.Focus();
    screen.Type("AO");
    screen.Key(Key::Enter);
    screen.Frame();
    std::vector<ui::Element> added = items.Children();
    EASYFORGE_REQUIRE(added.size() == 1);
    ui::Checkbox check = added[0].As<ui::Checkbox>();
    EASYFORGE_REQUIRE(check);
    screen.Click(check.Frame().Center());
    screen.Frame();
    EASYFORGE_EXPECT(check.Checked.Get());
    EASYFORGE_EXPECT(check.IsFocused());
    EASYFORGE_EXPECT(!entry.IsFocused());
}

EASYFORGE_TEST(ScrollingFollowsChangesAndLimits)
{
    // Pages a view wide can all be reached.
    std::vector<ui::Element> pages;
    for (int index = 0; index < 3; ++index)
    {
        pages.push_back(ui::Panel({ .Width = ui::Percent(100) }));
    }
    ui::Scroll sideways({ .Width = 300, .Height = 100, .Direction = ui::ScrollDirection::Horizontal, .Children = pages });
    Screen screen(400, 300, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { sideways } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(pages[1].Frame().X, 300.0f);
    sideways.ScrollTo(600);
    screen.Run(0.5f);
    EASYFORGE_EXPECT_EQUAL(sideways.Position.Get(), 600.0f);

    // Scrolling to the end right after adding reaches the new end.
    ui::Scroll log({ .Width = 200, .Height = 100 });
    for (int index = 0; index < 4; ++index)
    {
        log.Add(ui::Panel({ .Height = 40 }));
    }
    Screen logScreen(200, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { log } }));
    logScreen.Frame();
    log.Add(ui::Panel({ .Height = 40 }));
    log.ScrollTo(ui::Unlimited);
    logScreen.Run(0.5f);
    EASYFORGE_EXPECT_EQUAL(log.Position.Get(), 100.0f);

    // An element added just now can be scrolled to.
    ui::Panel added({ .Height = 40 });
    log.Add(added);
    log.ScrollTo(0);
    logScreen.Run(0.5f);
    log.ScrollIntoView(added);
    logScreen.Run(0.5f);
    EASYFORGE_EXPECT_EQUAL(log.Position.Get(), 140.0f);

    // A position set before the first layout is kept.
    ui::Scroll restored({ .Width = 200, .Height = 100 });
    for (int index = 0; index < 10; ++index)
    {
        restored.Add(ui::Panel({ .Height = 40 }));
    }
    restored.Position = 120;
    Screen restoredScreen(200, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { restored } }));
    restoredScreen.Run(0.3f);
    EASYFORGE_EXPECT_EQUAL(restored.Position.Get(), 120.0f);

    // The bar can be grabbed over a child that covers it.
    int clicks = 0;
    ui::Scroll buttons({ .Width = 200, .Height = 100 });
    for (int index = 0; index < 10; ++index)
    {
        buttons.Add(ui::Button("A", { .Height = 40, .OnClick = [&] { ++clicks; } }));
    }
    Screen buttonScreen(200, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { buttons } }));
    buttonScreen.Frame();
    buttonScreen.Move({ 195.0f, 5.0f });
    buttonScreen.Press({ 195.0f, 5.0f });
    buttonScreen.Move({ 195.0f, 60.0f });
    buttonScreen.Release({ 195.0f, 60.0f });
    buttonScreen.Run(0.3f);
    EASYFORGE_EXPECT_EQUAL(clicks, 0);
    EASYFORGE_EXPECT(buttons.Position.Get() > 0.0f);
}

EASYFORGE_TEST(KeysThatMoveTheKeyboardDoNotType)
{
    ui::TextField title;
    ui::Button add("A", { .OnClick = [&] { title.Text = ""; title.Focus(); } });
    Screen screen(300, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { add, title } }));
    screen.Frame();
    screen.Key(Key::Tab);
    EASYFORGE_EXPECT(add.IsFocused());

    // Space presses the button, which gives the field the keyboard; the space
    // the key types does not land in the field.
    screen.Key(Key::Space);
    screen.Type(" ");
    EASYFORGE_EXPECT(title.IsFocused());
    EASYFORGE_EXPECT_EQUAL(title.Text.Get(), std::string());
    screen.Type("A");
    EASYFORGE_EXPECT_EQUAL(title.Text.Get(), std::string("A"));

    // Hidden or removed elements lose the keyboard.
    title.Visible = false;
    screen.Frame();
    EASYFORGE_EXPECT(!title.IsFocused());
    EASYFORGE_EXPECT(!screen.Key(Key::W));
    title.Visible = true;
    title.Focus();
    EASYFORGE_EXPECT(title.IsFocused());
    title.Remove();
    EASYFORGE_EXPECT(!title.IsFocused());
}
