#include <cmath>
#include <string>
#include <vector>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    // The height of a row in a popup list, with the test theme's font.
    float RowHeight()
    {
        return Font::Load(EASYFORGE_TEST_FONT).LineHeight(20.0f) + 10.0f;
    }
}

EASYFORGE_TEST(DropdownsChooseFromTheirList)
{
    std::vector<int> changes;
    ui::Dropdown size({
        .Options = { "Small", "Medium", "Large" },
        .Placeholder = "Size",
        .OnChange = [&](int row) { changes.push_back(row); },
    });
    Screen screen(400, 400, ui::Column({ .Padding = 10, .Alignment = ui::Alignment::Start, .Children = { size } }));
    screen.Frame();

    Rectangle frame = size.Frame();
    EASYFORGE_EXPECT_EQUAL(frame.Width, 200.0f);
    EASYFORGE_EXPECT_EQUAL(size.SelectedText(), std::string());

    // The list opens below, with its rows starting inside its padding.
    float row = RowHeight();
    auto rowAt = [&](int index) {
        return Vector2 { frame.X + 20.0f, frame.Bottom() + 8.0f + row * (static_cast<float>(index) + 0.5f) };
    };
    screen.Click(frame.Center());
    screen.Frame();
    EASYFORGE_EXPECT(size.IsFocused());
    screen.Move(rowAt(1));
    screen.Click(rowAt(1));
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 1);
    EASYFORGE_EXPECT_EQUAL(size.SelectedText(), std::string("Medium"));
    EASYFORGE_EXPECT_EQUAL(changes.size(), std::size_t(1));

    // Choosing closes it, and the keyboard stays with the dropdown.
    screen.Frame();
    EASYFORGE_EXPECT(size.IsFocused());
    EASYFORGE_EXPECT(!screen.Click(rowAt(2)));
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 1);

    // Closed, Up and Down choose the option before or after.
    size.Focus();
    screen.Key(Key::Down);
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 2);
    screen.Key(Key::Down);
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 2);
    EASYFORGE_EXPECT_EQUAL(changes.size(), std::size_t(2));
    screen.Key(Key::Up);
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 1);

    // Enter opens it on the chosen option; the arrows move and Enter chooses.
    screen.Key(Key::Enter);
    screen.Frame();
    screen.Key(Key::Up);
    screen.Key(Key::Enter);
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 0);

    // Escape closes it without choosing.
    screen.Key(Key::Enter);
    screen.Frame();
    screen.Key(Key::Down);
    screen.Key(Key::Escape);
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 0);
    EASYFORGE_EXPECT(size.IsFocused());

    // A click elsewhere closes it and does nothing else.
    screen.Click(frame.Center());
    screen.Frame();
    EASYFORGE_EXPECT(screen.Click({ 390.0f, 390.0f }));
    screen.Frame();
    EASYFORGE_EXPECT(!screen.Click(rowAt(2)));
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 0);

    // Clicking the dropdown while it is open closes it.
    screen.Click(frame.Center());
    screen.Frame();
    screen.Click(frame.Center());
    screen.Frame();
    EASYFORGE_EXPECT(!screen.Click(rowAt(2)));

    // The program choosing is reported too.
    size.Selected = 2;
    EASYFORGE_EXPECT_EQUAL(changes.back(), 2);
    EASYFORGE_EXPECT_EQUAL(size.SelectedText(), std::string("Large"));
}

EASYFORGE_TEST(MenusRunTheirItems)
{
    std::vector<std::string> ran;
    ui::Menu file("File", {
        .Items = {
            { .Text = "Open", .OnClick = [&] { ran.push_back("Open"); }, .Shortcut = "Ctrl+O" },
            { .Separator = true },
            { .Text = "Print", .OnClick = [&] { ran.push_back("Print"); }, .Enabled = false },
            { .Text = "Quit", .OnClick = [&] { ran.push_back("Quit"); } },
        },
    });
    Screen screen(400, 400, ui::Row({ .Padding = 10, .Alignment = ui::Alignment::Start, .Children = { file } }));
    screen.Frame();

    Rectangle frame = file.Frame();
    float row = RowHeight();
    float top = frame.Bottom() + 8.0f;
    float x = frame.X + 20.0f;

    screen.Click(frame.Center());
    screen.Frame();
    EASYFORGE_EXPECT(file.IsOpen());

    // A disabled item and a separator do nothing, and the menu stays open.
    screen.Click({ x, top + row + 9.0f + row * 0.5f });
    screen.Click({ x, top + row + 4.5f });
    EASYFORGE_EXPECT(ran.empty());
    EASYFORGE_EXPECT(file.IsOpen());

    screen.Click({ x, top + row * 2.0f + 9.0f + row * 0.5f });
    EASYFORGE_EXPECT_EQUAL(ran.size(), std::size_t(1));
    EASYFORGE_EXPECT_EQUAL(ran.back(), std::string("Quit"));
    EASYFORGE_EXPECT(!file.IsOpen());

    // Down opens it on the first item, and the arrows skip what cannot be chosen.
    file.Focus();
    screen.Key(Key::Down);
    screen.Frame();
    EASYFORGE_EXPECT(file.IsOpen());
    screen.Key(Key::Down);
    screen.Key(Key::Enter);
    EASYFORGE_EXPECT_EQUAL(ran.back(), std::string("Quit"));
    EASYFORGE_EXPECT(!file.IsOpen());

    file.Open();
    screen.Frame();
    screen.Key(Key::Home);
    screen.Key(Key::Space);
    EASYFORGE_EXPECT_EQUAL(ran.back(), std::string("Open"));

    file.Open();
    EASYFORGE_EXPECT(file.IsOpen());
    file.Close();
    EASYFORGE_EXPECT(!file.IsOpen());
}

EASYFORGE_TEST(DialogsTakeTheKeyboardAndPointer)
{
    int behind = 0;
    int kept = 0;
    int closed = 0;
    ui::Button under("Behind", { .Width = 380, .Height = 380, .OnClick = [&] { ++behind; } });
    ui::Dialog dialog;
    ui::Button keep("Keep", { .OnClick = [&] { ++kept; } });
    ui::Button close("Close", { .OnClick = [&] { dialog.Close(); } });
    dialog = ui::Dialog({
        .Width = 240,
        .Title = "Delete?",
        .OnClosed = [&] { ++closed; },
        .Children = { ui::Label("Sure?") },
        .Buttons = { keep, close },
    });
    Screen screen(400, 400, ui::Column({ .Padding = 10, .Children = { under } }));
    screen.Frame();
    Color before = screen.Picture().ColorAt(3, 3);

    under.Focus();
    dialog.Open(screen.Root);
    screen.Frame();
    EASYFORGE_EXPECT(dialog.IsOpen());
    EASYFORGE_EXPECT_EQUAL(dialog.Frame().Width, 240.0f);
    EASYFORGE_EXPECT(std::abs(dialog.Frame().Center().X - 200.0f) < 1.0f);
    EASYFORGE_EXPECT(std::abs(dialog.Frame().Center().Y - 200.0f) < 1.0f);

    // The keyboard moves to its first button, and Tab stays inside it.
    EASYFORGE_EXPECT(keep.IsFocused());
    screen.Key(Key::Tab);
    EASYFORGE_EXPECT(close.IsFocused());
    screen.Key(Key::Tab);
    EASYFORGE_EXPECT(keep.IsFocused());
    screen.Key(Key::Enter);
    EASYFORGE_EXPECT_EQUAL(kept, 1);

    // Clicks outside go nowhere, and the rest is dimmed.
    EASYFORGE_EXPECT(screen.Click({ 15.0f, 15.0f }));
    EASYFORGE_EXPECT_EQUAL(behind, 0);
    EASYFORGE_EXPECT(dialog.IsOpen());
    Color dimmed = screen.Picture().ColorAt(3, 3);
    EASYFORGE_EXPECT(dimmed.Red < before.Red - 0.1f);

    // Escape closes it and gives the keyboard back.
    screen.Key(Key::Escape);
    EASYFORGE_EXPECT(!dialog.IsOpen());
    EASYFORGE_EXPECT_EQUAL(closed, 1);
    EASYFORGE_EXPECT(under.IsFocused());

    // Its own button can close it too.
    dialog.Open(screen.Root);
    screen.Frame();
    screen.Click(close.Frame().Center());
    EASYFORGE_EXPECT(!dialog.IsOpen());
    EASYFORGE_EXPECT_EQUAL(closed, 2);
    screen.Frame();
    screen.Click({ 15.0f, 15.0f });
    EASYFORGE_EXPECT_EQUAL(behind, 1);

    // The title can change while it is open.
    dialog.Title = "Delete the note?";
    EASYFORGE_EXPECT_EQUAL(dialog.Title.Get(), std::string("Delete the note?"));
}

EASYFORGE_TEST(DialogsCanCloseOnOutsideClicks)
{
    ui::Dialog dialog({ .ClosesOnOutsideClick = true, .Children = { ui::Label("Hello") } });
    Screen screen(400, 400, ui::Column({ .Children = { ui::Label("Behind") } }));
    screen.Frame();
    dialog.Open(screen.Root);
    screen.Frame();
    EASYFORGE_EXPECT(screen.Click(dialog.Frame().Center()));
    EASYFORGE_EXPECT(dialog.IsOpen());
    screen.Click({ 5.0f, 395.0f });
    EASYFORGE_EXPECT(!dialog.IsOpen());
}
