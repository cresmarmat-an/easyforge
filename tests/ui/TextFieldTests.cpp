#include <cmath>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    constexpr KeyModifiers Shift { .Shift = true };
    constexpr KeyModifiers Control { .Control = true };
}

EASYFORGE_TEST(TextFieldsTakeTyping)
{
    std::vector<std::string> changes;
    std::string submitted;
    ui::TextField field({ .OnChange = [&](const std::string& text) { changes.push_back(text); },
        .OnSubmit = [&](const std::string& text) { submitted = text; } });
    Screen screen(300, 100, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { field } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(field.Frame().Width, 200.0f);

    screen.Click(field.Frame().Center());
    EASYFORGE_EXPECT(field.IsFocused());
    EASYFORGE_EXPECT(screen.Type("AO"));
    screen.Type("X");
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AOX"));
    EASYFORGE_EXPECT_EQUAL(changes.back(), std::string("AOX"));

    // Keys the field uses do not reach anything else, even ones it ignores.
    EASYFORGE_EXPECT(screen.Key(Key::W));

    screen.Key(Key::Left);
    screen.Key(Key::Backspace);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AX"));
    screen.Key(Key::Delete);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("A"));
    screen.Key(Key::Home);
    screen.Type("Z");
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("ZA"));

    screen.Key(Key::Enter);
    EASYFORGE_EXPECT_EQUAL(submitted, std::string("ZA"));

    // Control characters are not typed.
    screen.Type("\t\x7F");
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("ZA"));
}

EASYFORGE_TEST(TextFieldsSelectCopyAndPaste)
{
    ui::TextField field({ .Text = "AO AO" });
    Screen screen(300, 100, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { field } }));
    screen.Frame();
    field.Focus();

    // Shift with the arrows selects; typing replaces the selection.
    screen.Key(Key::Left, Shift);
    screen.Key(Key::Left, Shift);
    screen.Type("X");
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AO X"));

    // Control moves by words.
    screen.Key(Key::Left, Control);
    screen.Key(Key::Backspace, Control);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("X"));

    field.Text = "AO";
    screen.Frame();
    screen.Key(Key::A, Control);
    screen.Key(Key::C, Control);
    screen.Key(Key::End);
    screen.Key(Key::V, Control);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AOAO"));
    screen.Key(Key::A, Control);
    screen.Key(Key::X, Control);
    EASYFORGE_EXPECT(field.Text.Get().empty());
    screen.Key(Key::V, Control);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AOAO"));

    // A double click selects a word.
    field.Text = "AO XY";
    screen.Frame();
    Rectangle box = field.Frame();
    Event press;
    press.Type = EventType::MouseButtonPressed;
    press.Position = { box.X + 10.0f, box.Center().Y };
    press.ClickCount = 2;
    screen.Root.HandleEvent(press);
    screen.Release(press.Position);
    screen.Type("Z");
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("Z XY"));
}

EASYFORGE_TEST(TextFieldsUndo)
{
    ui::TextField field;
    Screen screen(300, 100, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { field } }));
    screen.Frame();
    field.Focus();
    screen.Type("A");
    screen.Type("O");
    screen.Key(Key::Backspace);
    screen.Type("X");
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AX"));

    // Typing in a row undoes as one.
    screen.Key(Key::Z, Control);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("A"));
    screen.Key(Key::Z, Control);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AO"));
    screen.Key(Key::Z, Control);
    EASYFORGE_EXPECT(field.Text.Get().empty());
    screen.Key(Key::Y, Control);
    EASYFORGE_EXPECT_EQUAL(field.Text.Get(), std::string("AO"));
}

EASYFORGE_TEST(TextFieldsLimitAndHide)
{
    ui::TextField limited({ .MaximumLength = 3 });
    ui::TextField secret({ .Text = "AO", .Password = true });
    Screen screen(300, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { limited, secret } }));
    screen.Frame();
    limited.Focus();
    screen.Type("AOAO");
    EASYFORGE_EXPECT_EQUAL(limited.Text.Get(), std::string("AOA"));
    screen.Type("Z");
    EASYFORGE_EXPECT_EQUAL(limited.Text.Get(), std::string("AOA"));

    // Passwords do not copy.
    secret.Focus();
    screen.Key(Key::A, Control);
    screen.Key(Key::C, Control);
    limited.Text = "";
    limited.Focus();
    screen.Key(Key::V, Control);
    EASYFORGE_EXPECT(limited.Text.Get().empty());
}

EASYFORGE_TEST(TextFieldsShowPlaceholdersAndCarets)
{
    ui::TextField field({ .Placeholder = "AAAA" });
    Screen screen(300, 100, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { field } }));
    ImageData empty = screen.Picture();
    ui::Theme theme = TestTheme();
    bool muted = false;
    for (int x = 8; x < 60 && !muted; ++x)
    {
        for (int y = 4; y < 30 && !muted; ++y)
        {
            muted = NearColor(empty.ColorAt(x, y), theme.MutedText, 0.05f);
        }
    }
    EASYFORGE_EXPECT(muted);

    // Focused, the border takes the accent color.
    field.Focus();
    ImageData focused = screen.Picture();
    EASYFORGE_EXPECT(NearColor(focused.ColorAt(100, 0), theme.Accent, 0.1f));
}

EASYFORGE_TEST(TextAreasTakeSeveralLines)
{
    std::string changed;
    ui::TextArea area({ .Width = 200, .Height = 100, .OnChange = [&](const std::string& text) { changed = text; } });
    Screen screen(300, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { area } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(area.Frame().Width, 200.0f);
    EASYFORGE_EXPECT_EQUAL(area.Frame().Height, 100.0f);

    screen.Click(area.Frame().Center());
    EASYFORGE_EXPECT(area.IsFocused());
    screen.Type("AOX");
    screen.Key(Key::Enter);
    screen.Type("XOA");
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AOX\nXOA"));
    EASYFORGE_EXPECT_EQUAL(changed, std::string("AOX\nXOA"));

    // Up and Down keep the column; Home and End stay on the line.
    screen.Key(Key::Up);
    screen.Type("Z");
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AOXZ\nXOA"));
    screen.Key(Key::Down);
    screen.Key(Key::Home);
    screen.Type("Z");
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AOXZ\nZXOA"));
    screen.Key(Key::Up);
    screen.Key(Key::End);
    screen.Type("O");
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AOXZO\nZXOA"));

    // Up from the first line goes to the start, and Down from the last to the end.
    screen.Key(Key::Up);
    screen.Type("A");
    screen.Key(Key::Down);
    screen.Key(Key::Down);
    screen.Type("X");
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AAOXZO\nZXOAX"));

    // Selections run across lines, and pasted line breaks are kept.
    screen.Key(Key::A, Control);
    screen.Key(Key::C, Control);
    screen.Key(Key::End, Control);
    screen.Key(Key::Enter);
    screen.Key(Key::V, Control);
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AAOXZO\nZXOAX\nAAOXZO\nZXOAX"));

    // Backspace at the start of a line joins it to the line before.
    screen.Key(Key::Home, Control);
    screen.Key(Key::Down);
    screen.Key(Key::Home);
    screen.Key(Key::Backspace);
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AAOXZOZXOAX\nAAOXZO\nZXOAX"));
    screen.Key(Key::Z, Control);
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), std::string("AAOXZO\nZXOAX\nAAOXZO\nZXOAX"));
}

EASYFORGE_TEST(TextAreasWrapAndScroll)
{
    std::string text = "AOAOAOAOAOAOAOAOAOAOAOAOAOAOAOAO";
    ui::TextArea area({ .Width = 120, .Text = text });
    area.Height = ui::Fit;
    Screen screen(300, 400, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { area } }));
    screen.Frame();
    float line = Font::Load(EASYFORGE_TEST_FONT).LineHeight(20.0f);

    // A word wider than the area breaks between characters; without wrapping it
    // stays on one line.
    EASYFORGE_EXPECT(area.Frame().Height > line * 2.0f + 12.0f);
    area.Wrap = false;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(area.Frame().Height, line + 12.0f);
    area.Wrap = true;

    // Lines break between words where they can.
    area.Text = "AO AO AO AO AO AO AO AO AO AO";
    screen.Frame();
    float wrapped = area.Frame().Height;
    EASYFORGE_EXPECT(wrapped > line * 2.0f + 12.0f);

    // With a fixed height, the wheel scrolls through the lines.
    std::string many;
    for (int index = 0; index < 20; ++index)
    {
        many += index == 0 ? "A" : "\nA";
    }
    area.Text = many;
    area.Height = 100;
    screen.Frame();
    area.Focus();
    screen.Key(Key::Home, Control);
    screen.Frame();
    EASYFORGE_EXPECT(screen.Wheel(area.Frame().Center(), { 0.0f, -1.0f }));
    screen.Frame();
    Rectangle frame = area.Frame();
    screen.Click({ frame.X + 8.0f, frame.Y + 6.0f + line * 0.5f });
    screen.Type("X");
    std::size_t lineScrolledTo = static_cast<std::size_t>(std::floor((48.0f + line * 0.5f) / line));
    std::string expected = many;
    expected.insert(lineScrolledTo * 2, "X");
    EASYFORGE_EXPECT_EQUAL(area.Text.Get(), expected);
}
