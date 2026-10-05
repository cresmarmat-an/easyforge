#include <string>

#include <easyforge/core/Testing.h>
#include <easyforge/data.h>

#include "Damage.h"

using namespace easyforge;
using namespace easyforge::testing;

// Damaged .tree text is read without crashing or hanging, and whatever a
// damaged text gives, writing it out and reading it back gives the same.

namespace
{
    std::string Sample()
    {
        Table game = Table::New();
        game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f }, { "Angry", false } });
        Node player = game.Add("Player");
        player["Health"] = 100;
        player["Position"] = Vector2 { 10, 20 };
        player["Facing"] = Vector3 { 0, 1, 0 };
        player["Name"] = "Ari \"the quick\"";
        player["Alive"] = true;
        Node sword = player.Add("Sword");
        sword["Damage"] = 12.5;
        sword["Glow"] = Color::Hex("#66CCFF80");
        Node goblin = game.Add("Goblin", "Enemy", { { "Health", 90 } });
        goblin.Add("Loot")["Coins"] = -3;
        return game.ToText();
    }
}

EASYFORGE_TEST(DamagedTreeTextNeverCrashes)
{
    Damage(
        Sample(),
        [](const std::string& damaged) {
            Table table = Table::FromText(damaged, "damaged.tree");
            if (table)
            {
                std::string written = table.ToText();
                Table again = Table::FromText(written, "again.tree");
                EASYFORGE_EXPECT(again);
                EASYFORGE_EXPECT_EQUAL(again.ToText(), written);
            }
        },
        11, 1500);
}

EASYFORGE_TEST(DamagedValueTextNeverCrashes)
{
    std::uint64_t seed = 20;
    for (const char* text : { "100", "-4.5e3", "\"Ari \\\"the quick\\\"\"", "10, 20", "1, 2, 3", "#66CCFF80", "true", "nothing" })
    {
        Damage(
            std::string(text),
            [](const std::string& damaged) {
                bool valid = false;
                DataValue value = DataValue::FromText(damaged, &valid);
                if (valid)
                {
                    // A value that was read writes back as text that reads the same.
                    bool again = false;
                    EASYFORGE_EXPECT(DataValue::FromText(value.ToText(), &again) == value);
                    EASYFORGE_EXPECT(again);
                }
            },
            seed++, 400);
    }
}
