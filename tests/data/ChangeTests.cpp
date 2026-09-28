#include <easyforge/core/Testing.h>
#include <easyforge/data.h>

using namespace easyforge;

EASYFORGE_TEST(EveryChangeIsListed)
{
    Table game = Table::New();
    EASYFORGE_EXPECT_EQUAL(game.Version(), std::uint64_t { 0 });

    Node player = game.Add("Player");
    player["Health"] = 100;
    std::uint64_t seen = game.Version();

    Node sword = player.Add("Sword");
    player["Health"] = 90;
    player["Health"] = 90;   // the same value again is not a change
    player.Rename("Hero");
    Node chest = game.Add("Chest");
    sword.MoveTo(chest);
    player["Health"].Clear();
    game.DefineType("Enemy", { { "Health", 50 } });
    chest.Remove();

    std::vector<Change> changes = game.ChangesSince(seen);
    EASYFORGE_REQUIRE(changes.size() == 8);
    EASYFORGE_EXPECT_EQUAL(game.Version(), seen + 8);
    for (std::size_t index = 0; index < changes.size(); ++index)
    {
        EASYFORGE_EXPECT_EQUAL(changes[index].Version, seen + index + 1);
    }

    EASYFORGE_EXPECT(changes[0].Kind == ChangeKind::Added);
    EASYFORGE_EXPECT(changes[0].Parent == player);

    EASYFORGE_EXPECT(changes[1].Kind == ChangeKind::PropertySet);
    EASYFORGE_EXPECT(changes[1].Target == player);
    EASYFORGE_EXPECT_EQUAL(changes[1].Property, std::string("Health"));
    EASYFORGE_EXPECT_EQUAL(changes[1].Old, DataValue(100));
    EASYFORGE_EXPECT_EQUAL(changes[1].New, DataValue(90));

    EASYFORGE_EXPECT(changes[2].Kind == ChangeKind::Renamed);
    EASYFORGE_EXPECT_EQUAL(changes[2].Old, DataValue("Player"));
    EASYFORGE_EXPECT_EQUAL(changes[2].New, DataValue("Hero"));

    EASYFORGE_EXPECT(changes[3].Kind == ChangeKind::Added);
    EASYFORGE_EXPECT(!changes[3].Parent);

    EASYFORGE_EXPECT(changes[4].Kind == ChangeKind::Moved);
    EASYFORGE_EXPECT(changes[4].Target == sword);
    EASYFORGE_EXPECT(changes[4].OldParent == player);

    EASYFORGE_EXPECT(changes[5].Kind == ChangeKind::PropertySet);
    EASYFORGE_EXPECT_EQUAL(changes[5].Old, DataValue(90));
    EASYFORGE_EXPECT(changes[5].New.IsNothing());

    EASYFORGE_EXPECT(changes[6].Kind == ChangeKind::TypeDefined);
    EASYFORGE_EXPECT_EQUAL(changes[6].Property, std::string("Enemy"));

    // A removed node's handle in the list no longer reaches it.
    EASYFORGE_EXPECT(changes[7].Kind == ChangeKind::Removed);
    EASYFORGE_EXPECT(!changes[7].Target);
    EASYFORGE_EXPECT(!chest);
    EASYFORGE_EXPECT(!sword);

    EASYFORGE_EXPECT(game.ChangesSince(game.Version()).empty());
}

EASYFORGE_TEST(OldChangesAreForgottenPastTheLimit)
{
    Table game = Table::New({ .ChangeLimit = 3 });
    Node counter = game.Add("Counter");
    for (int value = 1; value <= 10; ++value)
    {
        counter["Value"] = value;
    }
    EASYFORGE_EXPECT_EQUAL(game.Version(), std::uint64_t { 11 });
    EASYFORGE_EXPECT_EQUAL(game.OldestVersion(), std::uint64_t { 9 });
    std::vector<Change> changes = game.ChangesSince(0);
    EASYFORGE_REQUIRE(changes.size() == 3);
    EASYFORGE_EXPECT_EQUAL(changes.back().New, DataValue(10));

    // A reader that last looked before the oldest version has missed changes.
    std::uint64_t lastLooked = 4;
    EASYFORGE_EXPECT(lastLooked + 1 < game.OldestVersion());
}

EASYFORGE_TEST(EditsAreUndoneAndRedone)
{
    Table game = Table::New();
    game.DefineType("Enemy", { { "Health", 100 } });
    Node goblin = game.Add("Goblin", "Enemy");
    EASYFORGE_EXPECT(!game.CanUndo());

    game.BeginEdit("Hit");
    goblin["Health"] = 90;
    game.EndEdit();

    EASYFORGE_EXPECT(game.CanUndo());
    EASYFORGE_EXPECT_EQUAL(game.UndoName(), std::string("Hit"));
    EASYFORGE_EXPECT(game.Undo());
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(goblin["Health"]), 100);
    EASYFORGE_EXPECT(goblin.Properties().empty());
    EASYFORGE_EXPECT(!game.CanUndo());
    EASYFORGE_EXPECT_EQUAL(game.RedoName(), std::string("Hit"));

    EASYFORGE_EXPECT(game.Redo());
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(goblin["Health"]), 90);
    EASYFORGE_EXPECT(!game.CanRedo());
    EASYFORGE_EXPECT(!game.Redo());
}

EASYFORGE_TEST(UndoPutsTreesBackWhereTheyWere)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    Node sword = player.Add("Sword");
    Node gem = sword.Add("Gem");
    gem["Color"] = Color::Hex("#FF0000");
    Node shield = player.Add("Shield");
    std::string before = game.ToText();

    game.BeginEdit("Rearrange");
    sword.Remove();
    shield.Rename("Buckler");
    Node chest = game.Add("Chest");
    shield.MoveTo(chest);
    chest["Locked"] = true;
    game.EndEdit();
    EASYFORGE_EXPECT(!sword);
    EASYFORGE_EXPECT(!gem);

    EASYFORGE_EXPECT(game.Undo());
    EASYFORGE_EXPECT_EQUAL(game.ToText(), before);

    // The same handles reach the nodes again, and siblings keep their order.
    EASYFORGE_EXPECT(sword);
    EASYFORGE_EXPECT(gem);
    EASYFORGE_EXPECT(!chest);
    EASYFORGE_EXPECT(player.Child("Sword") == sword);
    EASYFORGE_EXPECT_EQUAL(gem["Color"].Get(), DataValue(Color::Hex("#FF0000")));

    EASYFORGE_EXPECT(game.Redo());
    EASYFORGE_EXPECT(!sword);
    EASYFORGE_EXPECT(chest);
    EASYFORGE_EXPECT_EQUAL(shield.Path(), std::string("Chest/Buckler"));
    EASYFORGE_EXPECT(static_cast<bool>(chest["Locked"]));
}

EASYFORGE_TEST(NestedEditsUndoAsOne)
{
    Table game = Table::New();
    Node player = game.Add("Player");

    game.BeginEdit("Level up");
    player["Level"] = 2;
    game.BeginEdit("Heal");
    player["Health"] = 100;
    game.EndEdit();
    EASYFORGE_EXPECT(!game.CanUndo());   // not while an edit is open
    game.EndEdit();

    EASYFORGE_EXPECT_EQUAL(game.UndoName(), std::string("Level up"));
    game.Undo();
    EASYFORGE_EXPECT(!player.Has("Level"));
    EASYFORGE_EXPECT(!player.Has("Health"));
    EASYFORGE_EXPECT(!game.CanUndo());

    // An edit with no changes is not kept.
    game.BeginEdit("Nothing");
    game.EndEdit();
    EASYFORGE_EXPECT(!game.CanUndo());
    EASYFORGE_EXPECT(game.CanRedo());
}

EASYFORGE_TEST(ChangesOutsideEditsCannotBeUndone)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    player["Health"] = 100;
    EASYFORGE_EXPECT(!game.CanUndo());
    EASYFORGE_EXPECT(!game.Undo());
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(player["Health"]), 100);
}

EASYFORGE_TEST(ANewEditForgetsWhatCouldBeRedone)
{
    Table game = Table::New();
    Node player = game.Add("Player");

    game.BeginEdit("First");
    player["Health"] = 1;
    game.EndEdit();
    game.Undo();
    EASYFORGE_EXPECT(game.CanRedo());

    game.BeginEdit("Second");
    player["Health"] = 2;
    game.EndEdit();
    EASYFORGE_EXPECT(!game.CanRedo());
    EASYFORGE_EXPECT_EQUAL(game.UndoName(), std::string("Second"));
}

EASYFORGE_TEST(UndoGoesBackAsFarAsTheLimit)
{
    Table game = Table::New({ .UndoLimit = 3 });
    Node counter = game.Add("Counter");
    for (int value = 1; value <= 5; ++value)
    {
        game.BeginEdit("Count");
        counter["Value"] = value;
        game.EndEdit();
    }
    int undone = 0;
    while (game.Undo())
    {
        ++undone;
    }
    EASYFORGE_EXPECT_EQUAL(undone, 3);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(counter["Value"]), 2);
}

EASYFORGE_TEST(UndoIsRecordedAsChanges)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    player["Health"] = 100;

    game.BeginEdit("Hit");
    player["Health"] = 90;
    Node bandage = player.Add("Bandage");
    game.EndEdit();

    std::uint64_t seen = game.Version();
    game.Undo();
    std::vector<Change> changes = game.ChangesSince(seen);
    EASYFORGE_REQUIRE(changes.size() == 2);

    // Undone in reverse: the bandage goes, then the health goes back.
    EASYFORGE_EXPECT(changes[0].Kind == ChangeKind::Removed);
    EASYFORGE_EXPECT(changes[0].Parent == player);
    EASYFORGE_EXPECT(changes[1].Kind == ChangeKind::PropertySet);
    EASYFORGE_EXPECT_EQUAL(changes[1].Old, DataValue(90));
    EASYFORGE_EXPECT_EQUAL(changes[1].New, DataValue(100));
    EASYFORGE_EXPECT(!bandage);
}

EASYFORGE_TEST(RowsHeldByUndoAreNotReused)
{
    Table game = Table::New({ .UndoLimit = 1 });
    Node player = game.Add("Player");
    Node sword = player.Add("Sword");

    game.BeginEdit("Drop");
    sword.Remove();
    game.EndEdit();

    // The sword's row is kept while undo can bring it back.
    Node bow = player.Add("Bow");
    EASYFORGE_EXPECT(!(bow == sword));
    game.Undo();
    EASYFORGE_EXPECT(sword);
    EASYFORGE_EXPECT(bow);

    // Once no edit can bring it back, the row is used again and the old handle
    // stays empty.
    game.Redo();
    game.BeginEdit("Other");
    player["Health"] = 1;
    game.EndEdit();
    Node axe = player.Add("Axe");
    EASYFORGE_EXPECT(axe);
    EASYFORGE_EXPECT(!sword);
    EASYFORGE_EXPECT_EQUAL(game.NodeCount(), std::size_t { 3 });
}
