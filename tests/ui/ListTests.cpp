#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    float RowHeight()
    {
        return Font::Load(EASYFORGE_TEST_FONT).LineHeight(20.0f) + 10.0f;
    }
}

EASYFORGE_TEST(ListsChooseAndActivateRows)
{
    std::vector<std::string> items;
    for (int index = 0; index < 100; ++index)
    {
        items.push_back(std::format("Item {}", index));
    }
    int selected = -1;
    int activated = -1;
    ui::List list({
        .Items = items,
        .OnSelect = [&](int row) { selected = row; },
        .OnActivate = [&](int row) { activated = row; },
    });
    Screen screen(400, 400, ui::Column({ .Padding = 10, .Alignment = ui::Alignment::Start, .Children = { list } }));
    screen.Frame();

    Rectangle frame = list.Frame();
    EASYFORGE_EXPECT_EQUAL(frame.Width, 240.0f);
    EASYFORGE_EXPECT_EQUAL(frame.Height, 200.0f);
    float row = RowHeight();
    auto rowAt = [&](float index) { return Vector2 { frame.X + 30.0f, frame.Y + 4.0f + row * (index + 0.5f) }; };

    screen.Click(rowAt(2));
    EASYFORGE_EXPECT_EQUAL(selected, 2);
    EASYFORGE_EXPECT_EQUAL(list.Selected.Get(), 2);
    EASYFORGE_EXPECT_EQUAL(list.SelectedText(), std::string("Item 2"));
    EASYFORGE_EXPECT(list.IsFocused());

    screen.Key(Key::Down);
    EASYFORGE_EXPECT_EQUAL(selected, 3);

    // End goes to the last row and scrolls it into view at the bottom.
    screen.Key(Key::End);
    EASYFORGE_EXPECT_EQUAL(selected, 99);
    screen.Frame();
    screen.Click({ frame.X + 30.0f, frame.Bottom() - 4.0f - row * 1.5f });
    EASYFORGE_EXPECT_EQUAL(selected, 98);

    // Home scrolls back.
    screen.Key(Key::Home);
    EASYFORGE_EXPECT_EQUAL(selected, 0);
    screen.Frame();
    screen.Click(rowAt(1));
    EASYFORGE_EXPECT_EQUAL(selected, 1);

    // The wheel scrolls without choosing.
    EASYFORGE_EXPECT(screen.Wheel(frame.Center(), { 0.0f, -1.0f }));
    EASYFORGE_EXPECT_EQUAL(selected, 1);
    screen.Frame();
    screen.Click(rowAt(0));
    EASYFORGE_EXPECT_EQUAL(selected, static_cast<int>(std::floor((row * 0.5f + 48.0f) / row)));

    // Double-clicking and Enter activate.
    screen.Key(Key::Home);
    screen.Frame();
    screen.DoubleClick(rowAt(4));
    EASYFORGE_EXPECT_EQUAL(activated, 4);
    screen.Key(Key::Up);
    screen.Key(Key::Enter);
    EASYFORGE_EXPECT_EQUAL(activated, 3);

    // Items can change while it is shown.
    list.Items = std::vector<std::string> { "One", "Two" };
    list.Selected = 1;
    EASYFORGE_EXPECT_EQUAL(list.SelectedText(), std::string("Two"));
    screen.Frame();
    screen.Key(Key::End);
    EASYFORGE_EXPECT_EQUAL(selected, 1);
}

EASYFORGE_TEST(TreesFollowTheirTable)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    Node sword = player.Add("Sword");
    player.Add("Shield");
    Node world = game.Add("World");

    Node chosen;
    int activations = 0;
    ui::Tree tree({
        .Source = game,
        .OnSelect = [&](Node node) { chosen = node; },
        .OnActivate = [&](Node) { ++activations; },
    });
    Screen screen(400, 400, ui::Column({ .Padding = 10, .Alignment = ui::Alignment::Start, .Children = { tree } }));
    screen.Frame();

    Rectangle frame = tree.Frame();
    float row = RowHeight();
    auto rowAt = [&](float index) { return Vector2 { frame.X + 40.0f, frame.Y + 4.0f + row * (index + 0.5f) }; };

    // Top nodes start open: Player, Sword, Shield, World.
    EASYFORGE_EXPECT(tree.IsOpen(player));
    screen.Click(rowAt(1));
    EASYFORGE_EXPECT(chosen == sword);
    EASYFORGE_EXPECT(tree.Selected.Get() == sword);

    // Left goes to the parent, and again closes it; Right opens it, and again
    // goes to its first child.
    screen.Key(Key::Left);
    EASYFORGE_EXPECT(chosen == player);
    screen.Key(Key::Left);
    EASYFORGE_EXPECT(!tree.IsOpen(player));
    screen.Key(Key::Down);
    EASYFORGE_EXPECT(chosen == world);
    screen.Key(Key::Up);
    screen.Key(Key::Right);
    EASYFORGE_EXPECT(tree.IsOpen(player));
    screen.Key(Key::Right);
    EASYFORGE_EXPECT(chosen == sword);

    // Nodes added to the table appear, under open parents.
    Node castle = world.Add("Castle");
    screen.Frame();
    screen.Key(Key::End);
    EASYFORGE_EXPECT(chosen == castle);

    // Removed nodes go away.
    castle.Remove();
    screen.Frame();
    screen.Key(Key::End);
    EASYFORGE_EXPECT(chosen == world);

    // The arrow opens and closes without choosing.
    screen.Click({ frame.X + 12.0f, rowAt(0).Y });
    EASYFORGE_EXPECT(!tree.IsOpen(player));
    EASYFORGE_EXPECT(chosen == world);

    // Double-clicking opens and activates.
    screen.DoubleClick(rowAt(0));
    EASYFORGE_EXPECT(chosen == player);
    EASYFORGE_EXPECT(tree.IsOpen(player));
    EASYFORGE_EXPECT_EQUAL(activations, 1);

    // Opening a node from code opens everything above it.
    tree.CloseNode(player);
    tree.OpenNode(sword);
    EASYFORGE_EXPECT(tree.IsOpen(player));
    screen.Frame();
    screen.Click(rowAt(2));
    EASYFORGE_EXPECT_EQUAL(chosen.Name(), std::string("Shield"));
}
