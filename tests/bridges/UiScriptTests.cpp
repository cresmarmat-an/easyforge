#include <string>

#include <easyforge/bridges/ui_script.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(ScriptsDriveInterfaces)
{
    ui::Label status("Ready", { .Name = "Status" });
    ui::Button play("Play", { .Name = "Play" });
    ui::Dropdown size({ .Name = "Size", .Options = { "Small", "Large" } });
    ui::Displays pages({ .Name = "Pages", .Children = { ui::Display("Menu"), ui::Display("Game") } });
    ui::Root root = ui::Root::New({ .Content = ui::Column({ .Children = { status, play, size, pages } }) });

    ScriptEngine scripts = ScriptEngine::New();
    ui::DefineInterface(scripts, root);
    Result<ScriptValue> ran = scripts.Run(R"(
constant pages = ui.Find("Pages")
variable clicks = 0

function OnPlayClicked() then
    clicks += 1
    ui.Find("Status").Text = "Playing {clicks}"
    pages.Show("Game", Fade(0.3))
end

ui.Find("Play").OnClick = OnPlayClicked
ui.Find("Size").Selected = 2
ui.Find("Size").OnChange = function(position) then
    ui.Find("Status").Text = "Size {position}"
end
return ui.Find("Size").SelectedText
)",
        "menu.script");
    EASYFORGE_REQUIRE(ran);
    EASYFORGE_EXPECT_EQUAL(ran->AsText(), std::string("Large"));
    EASYFORGE_EXPECT_EQUAL(size.Selected.Get(), 1);

    play.Click();
    EASYFORGE_EXPECT_EQUAL(status.Text.Get(), std::string("Playing 1"));
    EASYFORGE_EXPECT_EQUAL(pages.Current.Get(), std::string("Game"));

    size.Selected = 0;
    EASYFORGE_EXPECT_EQUAL(status.Text.Get(), std::string("Size 1"));

    Result<ScriptValue> missing = scripts.Run("ui.Find(\"Pages\").Show(\"Nowhere\")\n", "menu.script");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(missing.Error().find("there is no display named Nowhere") != std::string::npos);
    EASYFORGE_EXPECT(scripts.Run("return ui.Find(\"Nobody\")\n", "menu.script")->IsNothing());
}
