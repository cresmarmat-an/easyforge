#include <algorithm>

#include <easyforge/core/Testing.h>
#include <easyforge/data.h>

using namespace easyforge;

namespace
{
    std::vector<std::string> NamesOf(const std::vector<Node>& nodes)
    {
        std::vector<std::string> names;
        for (const Node& node : nodes)
        {
            names.push_back(node.Name());
        }
        return names;
    }

    std::vector<std::string> Sorted(std::vector<std::string> names)
    {
        std::sort(names.begin(), names.end());
        return names;
    }
}

EASYFORGE_TEST(TablesHoldATreeOfNodes)
{
    Table game = Table::New();
    EASYFORGE_REQUIRE(game);

    Node player = game.Add("Player");
    player["Health"] = 100;
    player["Name"] = "Ari";
    player["Position"] = Vector2 { 10, 20 };
    Node sword = player.Add("Sword");
    sword["Damage"] = 12;
    Node shield = player.Add("Shield", { { "Defense", 4.5f } });
    Node camera = game.Add("Camera");

    int health = player["Health"];
    std::string name = player["Name"];
    Vector2 position = player["Position"];
    float defense = shield["Defense"];
    EASYFORGE_EXPECT_EQUAL(health, 100);
    EASYFORGE_EXPECT_EQUAL(name, std::string("Ari"));
    EASYFORGE_EXPECT_EQUAL(position, (Vector2 { 10, 20 }));
    EASYFORGE_EXPECT_EQUAL(defense, 4.5f);

    EASYFORGE_EXPECT_EQUAL(game.NodeCount(), std::size_t { 4 });
    EASYFORGE_EXPECT_EQUAL(NamesOf(game.TopNodes()), (std::vector<std::string> { "Player", "Camera" }));
    EASYFORGE_EXPECT_EQUAL(NamesOf(game.Nodes()), (std::vector<std::string> { "Player", "Sword", "Shield", "Camera" }));
    EASYFORGE_EXPECT_EQUAL(NamesOf(player.Children()), (std::vector<std::string> { "Sword", "Shield" }));
    EASYFORGE_EXPECT_EQUAL(player.ChildCount(), std::size_t { 2 });
    EASYFORGE_EXPECT(sword.Parent() == player);
    EASYFORGE_EXPECT(!player.Parent());
    EASYFORGE_EXPECT(player.Child("Shield") == shield);
    EASYFORGE_EXPECT(!player.Child("Bow"));
    EASYFORGE_EXPECT_EQUAL(sword.Path(), std::string("Player/Sword"));
    EASYFORGE_EXPECT(game.Find("Player/Sword") == sword);
    EASYFORGE_EXPECT(game.Find("Camera") == camera);
    EASYFORGE_EXPECT(!game.Find("Player/Bow"));
    EASYFORGE_EXPECT(game.FindByIdentifier(shield.Identifier()) == shield);

    // Properties are listed in the order they were first set.
    EASYFORGE_EXPECT_EQUAL(player.Properties(), (std::vector<std::string> { "Health", "Name", "Position" }));
    EASYFORGE_EXPECT(player.Has("Name"));
    EASYFORGE_EXPECT(!player.Has("Damage"));

    // A property that was never set reads as nothing.
    EASYFORGE_EXPECT(player["Speed"].Get().IsNothing());
    EASYFORGE_EXPECT_EQUAL(player["Speed"].As<int>(), 0);
    EASYFORGE_EXPECT(player["Health"].As<int>() > 50);
}

EASYFORGE_TEST(CellsWorkLikeVariables)
{
    Table game = Table::New();
    Node player = game.Add("Player");

    player["Health"] = 100;
    player["Health"] += 5;
    player["Health"] -= 20;
    EASYFORGE_EXPECT_EQUAL(player["Health"].Get(), DataValue(85));

    player["Speed"] = 1.5;
    player["Speed"] += 1;
    EASYFORGE_EXPECT_EQUAL(player["Speed"].Get(), DataValue(2.5));

    player["Position"] = Vector2 { 1, 2 };
    player["Position"] += Vector2 { 3, 4 };
    EASYFORGE_EXPECT_EQUAL(player["Position"].Get(), DataValue(Vector2 { 4, 6 }));

    // Adding to nothing starts from zero.
    player["Score"] += 10;
    EASYFORGE_EXPECT_EQUAL(player["Score"].Get(), DataValue(10));
    player["Velocity"] -= Vector2 { 1, 2 };
    EASYFORGE_EXPECT_EQUAL(player["Velocity"].Get(), DataValue(Vector2 { -1, -2 }));

    // Text, booleans, and colors cannot be added to.
    player["Name"] = "Ari";
    player["Name"] += 1;
    EASYFORGE_EXPECT_EQUAL(player["Name"].Get(), DataValue("Ari"));

    // One cell copied into another copies the value.
    player["Best"] = player["Score"];
    player["Score"] = 0;
    EASYFORGE_EXPECT_EQUAL(player["Best"].Get(), DataValue(10));

    player["Best"].Clear();
    EASYFORGE_EXPECT(!player.Has("Best"));

    // Assigning nothing clears too.
    player["Score"] = DataValue();
    EASYFORGE_EXPECT(!player["Score"].Exists());
}

EASYFORGE_TEST(TypesGiveValuesToTheirNodes)
{
    Table game = Table::New();
    game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });
    Node goblin = game.Add("Goblin", "Enemy");
    Node troll = game.Add("Troll", "Enemy", { { "Health", 300 } });
    Node rock = game.Add("Rock");

    EASYFORGE_EXPECT_EQUAL(goblin.TypeName(), std::string("Enemy"));
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(goblin["Health"]), 100);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(troll["Health"]), 300);
    EASYFORGE_EXPECT_EQUAL(static_cast<float>(troll["Speed"]), 4.5f);
    EASYFORGE_EXPECT(goblin.Has("Speed"));

    // The node's own properties do not include its type's.
    EASYFORGE_EXPECT(goblin.Properties().empty());

    // Clearing the node's own value shows the type's again.
    goblin["Health"] = 50;
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(goblin["Health"]), 50);
    goblin["Health"].Clear();
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(goblin["Health"]), 100);

    // Redefining a type changes every node that reads from it.
    game.DefineType("Enemy", { { "Health", 120 } });
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(goblin["Health"]), 120);
    EASYFORGE_EXPECT(!goblin.Has("Speed"));
    EASYFORGE_EXPECT_EQUAL(game.TypeValues("Enemy").size(), std::size_t { 1 });
    EASYFORGE_EXPECT_EQUAL(game.TypeNames(), (std::vector<std::string> { "Enemy" }));

    rock["Health"] = 1;
    EASYFORGE_EXPECT_EQUAL(Sorted(NamesOf(game.NodesWith("Health"))), (std::vector<std::string> { "Goblin", "Rock", "Troll" }));
    EASYFORGE_EXPECT_EQUAL(Sorted(NamesOf(game.NodesOfType("Enemy"))), (std::vector<std::string> { "Goblin", "Troll" }));
    EASYFORGE_EXPECT(game.NodesWith("Speed").empty());
}

EASYFORGE_TEST(NodesMoveAndRename)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    Node chest = game.Add("Chest");
    Node sword = player.Add("Sword");
    Node gem = sword.Add("Gem");

    sword.MoveTo(chest);
    EASYFORGE_EXPECT(sword.Parent() == chest);
    EASYFORGE_EXPECT_EQUAL(gem.Path(), std::string("Chest/Sword/Gem"));
    EASYFORGE_EXPECT_EQUAL(player.ChildCount(), std::size_t { 0 });

    // No node means the top of the table.
    sword.MoveTo(Node());
    EASYFORGE_EXPECT_EQUAL(NamesOf(game.TopNodes()), (std::vector<std::string> { "Player", "Chest", "Sword" }));

    // A node cannot go inside itself.
    sword.MoveTo(gem);
    EASYFORGE_EXPECT(!sword.Parent());

    sword.Rename("Blade");
    EASYFORGE_EXPECT_EQUAL(sword.Name(), std::string("Blade"));
    EASYFORGE_EXPECT(game.Find("Blade/Gem") == gem);
}

EASYFORGE_TEST(RemovedNodesTakeTheirChildren)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    Node sword = player.Add("Sword");
    Node gem = sword.Add("Gem");
    Node camera = game.Add("Camera");

    sword.Remove();
    EASYFORGE_EXPECT(!sword);
    EASYFORGE_EXPECT(!gem);
    EASYFORGE_EXPECT(player);
    EASYFORGE_EXPECT_EQUAL(game.NodeCount(), std::size_t { 2 });
    EASYFORGE_EXPECT(sword.Name().empty());

    // A handle to a removed node does nothing.
    sword["Damage"] = 5;
    EASYFORGE_EXPECT(!sword.Add("Gem"));
    EASYFORGE_EXPECT(sword["Damage"].Get().IsNothing());

    // Its row is used again, and the old handles do not reach the new node.
    Node bow = player.Add("Bow");
    bow["Damage"] = 7;
    EASYFORGE_EXPECT(!sword);
    EASYFORGE_EXPECT(!gem);
    EASYFORGE_EXPECT(sword["Damage"].Get().IsNothing());
    EASYFORGE_EXPECT(!(bow == sword));
    EASYFORGE_EXPECT(bow.Identifier() != sword.Identifier());
    EASYFORGE_EXPECT(camera);
}

EASYFORGE_TEST(EmptyTablesTestFalse)
{
    Table nothing;
    EASYFORGE_EXPECT(!nothing);
    EASYFORGE_EXPECT(!nothing.Add("Player"));
    EASYFORGE_EXPECT(!Node());
}
