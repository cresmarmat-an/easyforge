#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(ElementsAreHandles)
{
    ui::Label status("Ready");
    ui::Label copy = status;
    copy.Text = "Saved";
    EASYFORGE_EXPECT_EQUAL(status.Text.Get(), std::string("Saved"));
    EASYFORGE_EXPECT(copy == status);

    ui::Element any = status;
    EASYFORGE_EXPECT_EQUAL(any.Kind(), std::string_view("Label"));
    EASYFORGE_EXPECT(any.Is<ui::Label>());
    EASYFORGE_EXPECT(!any.Is<ui::Button>());
    EASYFORGE_EXPECT_EQUAL(any.As<ui::Label>().Text.Get(), std::string("Saved"));
    EASYFORGE_EXPECT(!any.As<ui::Button>());

    // Assigning another element makes every property follow it.
    ui::Label other("Other");
    copy = other;
    EASYFORGE_EXPECT_EQUAL(copy.Text.Get(), std::string("Other"));
    EASYFORGE_EXPECT_EQUAL(status.Text.Get(), std::string("Saved"));

    ui::Element nothing;
    EASYFORGE_EXPECT(!nothing);
    EASYFORGE_EXPECT(nothing.Kind().empty());
    EASYFORGE_EXPECT(nothing.Width.Get() == ui::Fit);
}

EASYFORGE_TEST(SettingsBecomeProperties)
{
    ui::Button save("Save", { .Name = "Save", .Width = 120, .Padding = ui::Insets { 4, 2 },
                                .Style = ui::ButtonStyle::Accent, .Tooltip = "Saves the file" });
    EASYFORGE_EXPECT_EQUAL(save.Name.Get(), std::string("Save"));
    EASYFORGE_EXPECT(save.Width.Get() == ui::Size(120));
    EASYFORGE_EXPECT(save.Padding.Get() == ui::Insets(4, 2));
    EASYFORGE_EXPECT(save.Style.Get() == ui::ButtonStyle::Accent);
    EASYFORGE_EXPECT_EQUAL(save.Tooltip.Get(), std::string("Saves the file"));
    EASYFORGE_EXPECT(save.Visible.Get());
    EASYFORGE_EXPECT_EQUAL(save.Opacity.Get(), 1.0f);

    save.Width = ui::Fill;
    EASYFORGE_EXPECT(save.Width.Get() == ui::Fill);
    save.Width = ui::Percent(30);
    EASYFORGE_EXPECT(save.Width.Get() == ui::Percent(30));
    save.Background = Color::Hex("#FF0000");
    EASYFORGE_EXPECT(save.Background.Get()->Type == ui::Background::Kind::Color);
}

EASYFORGE_TEST(TheInterfaceIsATable)
{
    ui::Button save("Save", { .Name = "Save", .Style = ui::ButtonStyle::Accent });
    ui::Label title("Notes", { .FontSize = 32 });
    Screen screen(200, 200, ui::Column({ .Name = "Page", .Gap = 8, .Children = { title, save } }));

    Table table = screen.Root.Data();
    Node page = table.Find("Page");
    EASYFORGE_REQUIRE(page);
    EASYFORGE_EXPECT_EQUAL(page.TypeName(), std::string("Column"));
    EASYFORGE_EXPECT_EQUAL(page.ChildCount(), std::size_t { 2 });
    Node button = table.Find("Page/Save");
    EASYFORGE_REQUIRE(button);
    EASYFORGE_EXPECT_EQUAL(button.TypeName(), std::string("Button"));
    EASYFORGE_EXPECT_EQUAL(button["Text"].As<std::string>(), std::string("Save"));
    EASYFORGE_EXPECT_EQUAL(button["Style"].As<std::string>(), std::string("accent"));

    // Only what was chosen is stored; the rest are the kind's defaults.
    EASYFORGE_EXPECT_EQUAL(page.Properties(), (std::vector<std::string> { "Gap" }));
    EASYFORGE_EXPECT_EQUAL(page["Gap"].As<float>(), 8.0f);

    // Changes to elements are changes to the table.
    std::uint64_t version = table.Version();
    save.Text = "Save all";
    std::vector<Change> changes = table.ChangesSince(version);
    EASYFORGE_REQUIRE(changes.size() == 1);
    EASYFORGE_EXPECT_EQUAL(changes[0].Property, std::string("Text"));
    EASYFORGE_EXPECT_EQUAL(changes[0].New, DataValue("Save all"));
}

EASYFORGE_TEST(ElementsAreFoundByName)
{
    ui::Button save("Save", { .Name = "Save" });
    ui::Panel panel({ .Name = "Tools", .Children = { ui::Label("A", { .Name = "Title" }), save } });
    Screen screen(200, 200, ui::Column({ .Name = "Main", .Children = { panel } }));

    EASYFORGE_EXPECT(screen.Root.Find<ui::Button>("Save") == save);
    EASYFORGE_EXPECT(screen.Root.Find("Tools") == panel);
    EASYFORGE_EXPECT(!screen.Root.Find<ui::Label>("Save"));
    EASYFORGE_EXPECT(!screen.Root.Find("Missing"));
    EASYFORGE_EXPECT_EQUAL(panel.Find<ui::Label>("Title").Text.Get(), std::string("A"));

    // Renaming is found at once.
    save.Name = "Store";
    EASYFORGE_EXPECT(screen.Root.Find<ui::Button>("Store") == save);
    EASYFORGE_EXPECT(screen.Root.Data().Find("Main/Tools/Store"));
}

EASYFORGE_TEST(ElementsMoveBetweenContainers)
{
    ui::Label label("A", { .FontSize = 30 });
    ui::Column left({ .Children = { label } });
    ui::Column right;
    EASYFORGE_EXPECT(label.Parent() == left);

    right.Add(label);
    EASYFORGE_EXPECT(label.Parent() == right);
    EASYFORGE_EXPECT(left.Children().empty());
    EASYFORGE_EXPECT_EQUAL(label.FontSize.Get(), 30.0f);

    label.Remove();
    EASYFORGE_EXPECT(!label.Parent());
    EASYFORGE_EXPECT_EQUAL(label.FontSize.Get(), 30.0f);

    ui::Spacer first;
    ui::Spacer last;
    right.Add(last);
    right.Insert(0, first);
    right.Insert(1, label);
    std::vector<ui::Element> children = right.Children();
    EASYFORGE_REQUIRE(children.size() == 3);
    EASYFORGE_EXPECT(children[0] == first);
    EASYFORGE_EXPECT(children[1] == label);
    EASYFORGE_EXPECT(children[2] == last);

    right.Clear();
    EASYFORGE_EXPECT(right.Children().empty());

    // Elements that do not hold others refuse children, and nothing goes inside
    // itself.
    label.Add(ui::Spacer());
    EASYFORGE_EXPECT(label.Children().empty());
    right.Add(left);
    left.Add(right);
    EASYFORGE_EXPECT(right.Parent() != left);
}

EASYFORGE_TEST(ChildrenOutliveTheirParentWhenHeld)
{
    ui::Label kept("Kept", { .FontSize = 12 });
    {
        ui::Column column({ .Children = { kept, ui::Label("Dropped") } });
    }
    EASYFORGE_EXPECT(!kept.Parent());
    EASYFORGE_EXPECT_EQUAL(kept.Text.Get(), std::string("Kept"));
    EASYFORGE_EXPECT_EQUAL(kept.FontSize.Get(), 12.0f);

    ui::Column again({ .Children = { kept } });
    EASYFORGE_EXPECT(kept.Parent() == again);
}

EASYFORGE_TEST(ChangesAfterShowingLayOutAgain)
{
    ui::Spacer grows(20);
    ui::Spacer after(10);
    Screen screen(200, 200, ui::Column({ .Children = { grows, after } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(after.Frame().Y, 20.0f);
    grows.Height = 50;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(after.Frame().Y, 50.0f);

    // Replacing the root's content lets the old content go.
    ui::Spacer other(5);
    screen.Root.Content = ui::Column({ .Children = { other } });
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(other.Frame().Y, 0.0f);
    EASYFORGE_EXPECT_EQUAL(screen.Root.Data().TopNodes().size(), std::size_t { 1 });
}

EASYFORGE_TEST(SettingsGivenOnPurposeAreKept)
{
    // A title bar's own defaults give way to values given, even ones that match
    // another kind's defaults.
    ui::TitleBar bar({ .Height = ui::Fit, .Padding = 0, .Gap = 0, .Alignment = ui::Alignment::Stretch });
    EASYFORGE_EXPECT(bar.Height.Get() == ui::Fit);
    EASYFORGE_EXPECT_EQUAL(bar.Gap.Get(), 0.0f);
    EASYFORGE_EXPECT(bar.Alignment.Get() == ui::Alignment::Stretch);
    EASYFORGE_EXPECT_EQUAL(bar.Padding.Get().Left, 0.0f);
    ui::TitleBar plain;
    EASYFORGE_EXPECT(plain.Height.Get() == ui::Size(36.0f));
    EASYFORGE_EXPECT_EQUAL(plain.Gap.Get(), 8.0f);

    // A size left out is the kind's own; Fit given on purpose is Fit.
    ui::TextField sized;
    ui::TextField fitting({ .Width = ui::Fit });
    EASYFORGE_EXPECT(sized.Width.Get() == ui::Size(200.0f));
    EASYFORGE_EXPECT(fitting.Width.Get() == ui::Fit);

    // Assigning a size left out goes back to the kind's own.
    fitting.Width = ui::Size {};
    EASYFORGE_EXPECT(fitting.Width.Get() == ui::Size(200.0f));
}
