#include <easyforge/core/Testing.h>
#include <easyforge/data.h>

using namespace easyforge;

namespace
{
    Table Sample()
    {
        Table game = Table::New();
        game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });
        Node player = game.Add("Player");
        player["Health"] = 100;
        player["Position"] = Vector2 { 10, 20 };
        player["Name"] = "Ari";
        Node sword = player.Add("Sword");
        sword["Damage"] = 12;
        sword["Glow"] = Color::Hex("#66CCFF");
        game.Add("Goblin", "Enemy", { { "Health", 90 } });
        return game;
    }
}

EASYFORGE_TEST(TablesWriteReadableText)
{
    std::string expected = "type Enemy\n"
                           "    Health = 100\n"
                           "    Speed = 4.5\n"
                           "\n"
                           "Player\n"
                           "    Health = 100\n"
                           "    Position = 10, 20\n"
                           "    Name = \"Ari\"\n"
                           "    Sword\n"
                           "        Damage = 12\n"
                           "        Glow = #66CCFF\n"
                           "Goblin : Enemy\n"
                           "    Health = 90\n";
    EASYFORGE_EXPECT_EQUAL(Sample().ToText(), expected);
}

EASYFORGE_TEST(TablesAreSavedAndLoaded)
{
    Table game = Sample();
    std::string path = std::string(EASYFORGE_TEST_OUTPUT) + "save.tree";
    EASYFORGE_REQUIRE(game.Save(path));

    Table loaded = Table::Load(path);
    EASYFORGE_REQUIRE(loaded);
    EASYFORGE_EXPECT_EQUAL(loaded.ToText(), game.ToText());

    Node player = loaded.Find("Player");
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(player["Health"]), 100);
    EASYFORGE_EXPECT_EQUAL(static_cast<std::string>(player["Name"]), std::string("Ari"));
    EASYFORGE_EXPECT_EQUAL(static_cast<Vector2>(player["Position"]), (Vector2 { 10, 20 }));
    EASYFORGE_EXPECT_EQUAL(static_cast<float>(loaded.Find("Goblin")["Speed"]), 4.5f);
    EASYFORGE_EXPECT_EQUAL(loaded.Find("Goblin").TypeName(), std::string("Enemy"));

    // What was read is where the loaded table starts, not a list of changes.
    EASYFORGE_EXPECT_EQUAL(loaded.Version(), std::uint64_t { 0 });
    EASYFORGE_EXPECT(loaded.ChangesSince(0).empty());
    EASYFORGE_EXPECT(!loaded.CanUndo());
}

EASYFORGE_TEST(UnusualNamesAreQuoted)
{
    Table game = Table::New();
    Node odd = game.Add("key = value");
    odd.Add(" padded ");
    odd.Add("type");
    odd.Add("type Enemy");
    odd.Add("a--b");
    odd.Add("say \"hi\"");
    Node typed = game.Add("Time: now", "Kind: odd");
    typed["Colon: here"] = 1;
    typed["Equals = here"] = 2;

    std::string text = game.ToText();
    Table read = Table::FromText(text);
    EASYFORGE_REQUIRE(read);
    EASYFORGE_EXPECT_EQUAL(read.ToText(), text);
    EASYFORGE_EXPECT_EQUAL(read.TopNodes().front().Name(), std::string("key = value"));
    EASYFORGE_EXPECT_EQUAL(read.TopNodes().front().ChildCount(), std::size_t { 5 });
    Node readTyped = read.TopNodes().back();
    EASYFORGE_EXPECT_EQUAL(readTyped.Name(), std::string("Time: now"));
    EASYFORGE_EXPECT_EQUAL(readTyped.TypeName(), std::string("Kind: odd"));
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(readTyped["Equals = here"]), 2);
}

EASYFORGE_TEST(ReadingSkipsCommentsAndBlankLines)
{
    Table read = Table::FromText("-- the player\n"
                                 "\n"
                                 "Player   -- a comment after a node\n"
                                 "    Health = 100 -- and after a value\n"
                                 "\n"
                                 "    Motto = \"--not a comment\"\n"
                                 "\tSword\n"
                                 "        Damage = -12\n");
    EASYFORGE_REQUIRE(read);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(read.Find("Player")["Health"]), 100);
    EASYFORGE_EXPECT_EQUAL(static_cast<std::string>(read.Find("Player")["Motto"]), std::string("--not a comment"));
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(read.Find("Player/Sword")["Damage"]), -12);
}

EASYFORGE_TEST(ReadingReportsWhereTextIsWrong)
{
    struct Case
    {
        const char* Text;
        const char* Error;
    };
    Case cases[] = {
        { "Player\n  Health = 100\n", "level.tree:2: each level is four spaces deeper" },
        { "Player\n    Health = 1 2\n", "level.tree:2: '1 2' is not a value" },
        { "Health = 100\n", "level.tree:1: the property Health is not under a node" },
        { "Player\n        Sword\n", "level.tree:2: this node is indented deeper" },
        { "Player\n    = 5\n", "level.tree:2: a property needs a name" },
        { "type \n", "level.tree:1: a type needs a name" },
    };
    for (const Case& test : cases)
    {
        Table read = Table::FromText(test.Text, "level.tree");
        EASYFORGE_EXPECT(!read);
        EASYFORGE_EXPECT(read.Error().starts_with(test.Error));
        if (!read.Error().starts_with(test.Error))
        {
            testing::ReportFailure(__FILE__, __LINE__, "the error was: " + read.Error());
        }
    }

    Table missing = Table::Load(std::string(EASYFORGE_TEST_OUTPUT) + "missing.tree");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(missing.Error().ends_with("missing.tree: the file could not be opened"));
}

EASYFORGE_TEST(SavingOnlyWritesTheTreeAsItIsNow)
{
    Table game = Sample();
    game.BeginEdit("Remove the sword");
    game.Find("Player/Sword").Remove();
    game.EndEdit();

    Table read = Table::FromText(game.ToText());
    EASYFORGE_EXPECT(!read.Find("Player/Sword"));
    EASYFORGE_EXPECT_EQUAL(read.NodeCount(), std::size_t { 2 });

    game.Undo();
    EASYFORGE_EXPECT_EQUAL(game.ToText(), Sample().ToText());
}
