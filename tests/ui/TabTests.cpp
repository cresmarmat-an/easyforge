#include <string>
#include <vector>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(TabsShowOnePageAtATime)
{
    std::vector<std::string> changes;
    int hidden = 0;
    int pressed = 0;
    ui::Display general("General", { .Children = { ui::Button("Apply", { .OnClick = [&] { ++pressed; } }) } });
    general.OnHidden = [&] { ++hidden; };
    ui::TextField name({ .Width = 200 });
    ui::Tabs tabs({
        .OnChange = [&](const std::string& page) { changes.push_back(page); },
        .Children = { general, ui::Display("Advanced", { .Children = { name } }), ui::Display("About") },
    });
    Screen screen(400, 300, tabs);
    screen.Frame();

    EASYFORGE_EXPECT_EQUAL(tabs.Current.Get(), std::string("General"));
    EASYFORGE_EXPECT_EQUAL(tabs.Frame().Width, 400.0f);

    // The page sits below the strip of tabs.
    float strip = Font::Load(EASYFORGE_TEST_FONT).LineHeight(20.0f) + 16.0f;
    EASYFORGE_EXPECT_EQUAL(general.Frame().Y, strip);

    // Pages not shown take no clicks: this one goes to the button on the page
    // shown, which lies under the field.
    screen.Click(name.Frame().Center());
    EASYFORGE_EXPECT(!name.IsFocused());
    EASYFORGE_EXPECT_EQUAL(pressed, 1);

    // The tabs are laid out left to right, each as wide as its text.
    Font font = Font::Load(EASYFORGE_TEST_FONT);
    float first = font.Measure("General", 20.0f).X + 28.0f;
    float second = font.Measure("Advanced", 20.0f).X + 28.0f;
    screen.Click({ first + second * 0.5f, strip * 0.5f });
    EASYFORGE_EXPECT_EQUAL(tabs.Current.Get(), std::string("Advanced"));
    EASYFORGE_EXPECT_EQUAL(changes.size(), std::size_t(1));
    EASYFORGE_EXPECT_EQUAL(hidden, 1);
    EASYFORGE_EXPECT(tabs.IsFocused());

    // Clicking a page's empty space does not take the keyboard to the tabs.
    screen.Frame();
    screen.Click({ 300.0f, 250.0f });
    EASYFORGE_EXPECT(!tabs.IsFocused());
    screen.Click(name.Frame().Center());
    EASYFORGE_EXPECT(name.IsFocused());

    // The arrows move between tabs while the tabs have the keyboard, and not
    // while something in a page has it.
    name.Focus();
    screen.Key(Key::Left);
    EASYFORGE_EXPECT_EQUAL(tabs.Current.Get(), std::string("Advanced"));
    tabs.Focus();
    screen.Key(Key::Right);
    EASYFORGE_EXPECT_EQUAL(tabs.Current.Get(), std::string("About"));
    screen.Key(Key::Right);
    EASYFORGE_EXPECT_EQUAL(tabs.Current.Get(), std::string("About"));
    screen.Key(Key::Home);
    EASYFORGE_EXPECT_EQUAL(tabs.Current.Get(), std::string("General"));

    // Only the page shown gets events.
    screen.Frame();
    ui::Element apply = general.Children()[0];
    screen.Click(apply.Frame().Center());
    EASYFORGE_EXPECT_EQUAL(pressed, 2);
    tabs.Current = "About";
    screen.Frame();
    screen.Click(apply.Frame().Center());
    EASYFORGE_EXPECT_EQUAL(pressed, 2);

    // Moving away from a page with the keyboard in it gives the keyboard to the tabs.
    tabs.Current = "Advanced";
    name.Focus();
    tabs.Current = "General";
    EASYFORGE_EXPECT(tabs.IsFocused());
    EASYFORGE_EXPECT_EQUAL(changes.back(), std::string("General"));
}
