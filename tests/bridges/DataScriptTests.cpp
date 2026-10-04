#include <string>
#include <vector>

#include <easyforge/bridges/data_script.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(ScriptsReadAndChangeTables)
{
    Table game = Table::New();
    Node player = game.Add("Player", { { "Health", 100 }, { "Position", Vector2 { 1, 2 } } });
    std::vector<std::string> printed;
    ScriptEngine scripts = ScriptEngine::New({ .Print = [&](std::string_view line) { printed.emplace_back(line); } });
    scripts.Define("game", ScriptObjectFor(game));

    Result<ScriptValue> ran = scripts.Run(R"(
constant player = game.Find("Player")
player.Health -= 10
player.Position = { X = 3, Y = 4 }
constant sword = player.Add("Sword")
sword.Damage = 12
variable names = []
for child in player.Children() then
    names.Add(child.Name)
end
print(player.Health, player.Position.X, names, game.Find("Player/Sword").Damage, player.Get("Name"))

game.BeginEdit("Hit")
player.Health = 1
game.EndEdit()
game.Undo()
print(player.Health, player.Missing, Type(player))
)",
        "data.script");
    EASYFORGE_REQUIRE(ran);
    EASYFORGE_REQUIRE(printed.size() == 2);
    EASYFORGE_EXPECT_EQUAL(printed[0], std::string("90 3 [\"Sword\"] 12 nothing"));
    EASYFORGE_EXPECT_EQUAL(printed[1], std::string("90 nothing Node"));

    // The table holds what the script wrote, with whole numbers kept whole.
    EASYFORGE_EXPECT(player.Get("Health").Type() == DataType::Integer);
    EASYFORGE_EXPECT_EQUAL(player.Get("Health").AsInteger(), std::int64_t(90));
    EASYFORGE_EXPECT_EQUAL(player.Get("Position").AsVector2(), (Vector2 { 3, 4 }));
    EASYFORGE_EXPECT(game.Find("Player/Sword").Get("Damage").Type() == DataType::Integer);
    EASYFORGE_EXPECT_EQUAL(game.Find("Player/Sword").Get("Damage").AsInteger(), std::int64_t(12));
    Result<ScriptValue> fraction = scripts.Run("game.Find(\"Player/Sword\").Damage = 12.5\n", "data.script");
    EASYFORGE_REQUIRE(fraction);
    EASYFORGE_EXPECT_EQUAL(game.Find("Player/Sword").Get("Damage").AsNumber(), 12.5);

    Result<ScriptValue> refused = scripts.Run("game.Find(\"Player\").Children = 1\n", "data.script");
    EASYFORGE_EXPECT(!refused);
}
