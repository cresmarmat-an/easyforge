#include <string>
#include <vector>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(ElementsDragOntoOthers)
{
    int clicks = 0;
    std::vector<std::string> dropped;
    ui::Button card("Card", { .Width = 100, .Height = 40, .OnClick = [&] { ++clicks; } });
    card.DragText = "card-1";
    ui::Panel bin({ .Width = 150, .Height = 150 });
    bin.OnDrop = [&](const std::string& text) { dropped.push_back(text); };
    ui::Label plain("Not a target");
    Screen screen(400, 300, ui::Row({ .Padding = 10, .Gap = 20, .Alignment = ui::Alignment::Start, .Children = { card, bin, plain } }));
    screen.Frame();

    // A short press is still a click.
    screen.Click(card.Frame().Center());
    EASYFORGE_EXPECT_EQUAL(clicks, 1);

    // Moving a few points with the button down picks the card up, without a click.
    Vector2 start = card.Frame().Center();
    screen.Move(start);
    screen.Press(start);
    screen.Move(start + Vector2 { 10.0f, 0.0f });
    EASYFORGE_EXPECT(!card.IsPressed());
    screen.Move(bin.Frame().Center());
    screen.Frame();
    screen.Release(bin.Frame().Center());
    EASYFORGE_EXPECT_EQUAL(clicks, 1);
    EASYFORGE_EXPECT_EQUAL(dropped.size(), std::size_t(1));
    EASYFORGE_EXPECT_EQUAL(dropped.back(), std::string("card-1"));

    // Letting go where nothing takes drops does nothing.
    screen.Press(start);
    screen.Move(start + Vector2 { 0.0f, 100.0f });
    screen.Release(start + Vector2 { 0.0f, 100.0f });
    EASYFORGE_EXPECT_EQUAL(dropped.size(), std::size_t(1));

    // Escape cancels a drag.
    screen.Press(start);
    screen.Move(bin.Frame().Center());
    screen.Key(Key::Escape);
    screen.Release(bin.Frame().Center());
    EASYFORGE_EXPECT_EQUAL(dropped.size(), std::size_t(1));
    EASYFORGE_EXPECT_EQUAL(clicks, 1);

    // While dragging, the target is outlined in the accent color.
    screen.Press(start);
    screen.Move(bin.Frame().Center());
    ImageData picture = screen.Picture();
    Rectangle frame = bin.Frame();
    Color ring = picture.ColorAt(static_cast<int>(frame.X - 3.0f), static_cast<int>(frame.Center().Y));
    EASYFORGE_EXPECT(NearColor(ring, ui::Theme::Light().Accent, 0.1f));
    screen.Release(bin.Frame().Center());
    EASYFORGE_EXPECT_EQUAL(dropped.size(), std::size_t(2));

    // Files from the system go to the element under them that takes them.
    std::vector<std::string> files;
    bin.OnFilesDropped = [&](const std::vector<std::string>& paths) { files = paths; };
    Event event;
    event.Type = EventType::FilesDropped;
    event.Position = bin.Frame().Center();
    event.Files = { "a.png", "b.png" };
    EASYFORGE_EXPECT(screen.Root.HandleEvent(event));
    EASYFORGE_EXPECT_EQUAL(files.size(), std::size_t(2));
    event.Position = { 395.0f, 295.0f };
    EASYFORGE_EXPECT(!screen.Root.HandleEvent(event));
}
