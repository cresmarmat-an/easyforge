#include <string>
#include <vector>

#include <easyforge/bridges/data_network.h>
#include <easyforge/core/Testing.h>

#include "../network/NetworkTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    Table MakeGame()
    {
        Table game = Table::New();
        game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });
        game.Add("Players");
        Node world = game.Add("World", { { "Gravity", Vector2 { 0.0f, -9.8f } } });
        world.Add("Goblin", "Enemy", { { "Health", 30 } });
        world.Add("Chest", { { "Gold", 12 }, { "Tint", Color::Hex("#FFCC00") } });
        return game;
    }
}

EASYFORGE_TEST(OneWaySharingCopiesTheServersTable)
{
    Table game = MakeGame();
    Server server = LocalServer();
    TableShare shared = Share(game, server, Sharing::OneWay);
    EASYFORGE_EXPECT(shared);
    EASYFORGE_EXPECT(shared.IsReady());
    EASYFORGE_EXPECT(shared.Mode() == Sharing::OneWay);

    // What the client had is replaced.
    Table copy = Table::New();
    copy.Add("Leftover");
    Client client = LocalClient(server);
    TableShare received = Share(copy, client);
    EASYFORGE_EXPECT(!received.IsReady());

    auto update = [&] {
        server.Update();
        client.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return received.IsReady(); }));
    EASYFORGE_EXPECT_EQUAL(copy.ToText(), game.ToText());
    EASYFORGE_EXPECT(!copy.Find("Leftover"));

    // Every kind of change follows.
    Node goblin = game.Find("World/Goblin");
    goblin["Health"] = 25;
    goblin.Rename("Hurt goblin");
    Node orc = game.Find("World").Add("Orc", "Enemy");
    orc["Weapon"] = "Axe";
    game.Find("World/Chest").MoveTo(game.Find("Players"));
    game.DefineType("Enemy", { { "Health", 120 }, { "Speed", 5.0f } });
    game.Find("World/Chest").Remove();
    game.Find("Players/Chest")["Gold"] = 20;
    game.Find("World")["Gravity"] = DataValue();
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return copy.ToText() == game.ToText(); }));
    EASYFORGE_EXPECT_EQUAL(copy.ToText(), game.ToText());
    EASYFORGE_EXPECT_EQUAL(copy.Find("World/Orc")["Health"].As<int>(), 120);

    // Removing a node with children, and an undone edit.
    game.BeginEdit("Clear the world");
    game.Find("World").Remove();
    game.EndEdit();
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return !copy.Find("World"); }));
    game.Undo();
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return copy.ToText() == game.ToText(); }));
    EASYFORGE_EXPECT_EQUAL(copy.ToText(), game.ToText());

    // A one-way client's own changes stay with it.
    copy.Find("Players")["Note"] = "only here";
    UpdateFor(update, 0.2);
    EASYFORGE_EXPECT(!game.Find("Players").Has("Note"));
}

EASYFORGE_TEST(TwoWaySharingFollowsOwnership)
{
    Table game = MakeGame();
    Server server = LocalServer();
    TableShare shared = Share(game, server, Sharing::TwoWay);

    Table first = Table::New();
    Table second = Table::New();
    Client firstClient = LocalClient(server);
    Client secondClient = LocalClient(server);
    TableShare firstShare = Share(first, firstClient);
    TableShare secondShare = Share(second, secondClient);
    auto update = [&] {
        server.Update();
        firstClient.Update();
        secondClient.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return firstShare.IsReady() && secondShare.IsReady(); }));
    EASYFORGE_EXPECT(firstShare.Mode() == Sharing::TwoWay);

    // A client adds its own node, under one of the server's.
    Node avatar = first.Find("Players").Add("Ari", { { "Health", 100 } });
    avatar.Add("Hat", { { "Color", Color::Hex("#3366FF") } });
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return second.Find("Players/Ari/Hat") && game.Find("Players/Ari/Hat"); }));
    EASYFORGE_EXPECT_EQUAL(second.ToText(), game.ToText());
    EASYFORGE_EXPECT_EQUAL(first.ToText(), game.ToText());

    Node onServer = game.Find("Players/Ari");
    Connection owner = shared.Owner(onServer);
    EASYFORGE_EXPECT(owner);
    EASYFORGE_EXPECT(!shared.Owner(game.Find("Players")));
    EASYFORGE_EXPECT_EQUAL(shared.NodesOwnedBy(owner).size(), std::size_t { 2 });

    // The owner changes it; everyone sees.
    avatar["Health"] = 80;
    avatar.Rename("Ari the brave");
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return second.Find("Players/Ari the brave")["Health"].As<int>() == 80; }));
    EASYFORGE_EXPECT_EQUAL(game.Find("Players/Ari the brave")["Health"].As<int>(), 80);

    // Another client may not; the server puts its copy back.
    Node notMine = second.Find("Players/Ari the brave");
    notMine["Health"] = 1;
    second.Find("World/Goblin")["Health"] = 0;
    EASYFORGE_EXPECT(UpdateUntil(update, [&] {
        return notMine["Health"].As<int>() == 80 && second.Find("World/Goblin")["Health"].As<int>() == 30;
    }));
    EASYFORGE_EXPECT_EQUAL(game.Find("Players/Ari the brave")["Health"].As<int>(), 80);
    second.Find("World/Chest").Remove();
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return static_cast<bool>(second.Find("World/Chest")); }));
    EASYFORGE_EXPECT_EQUAL(second.ToText(), game.ToText());

    // The server may change any node.
    onServer = game.Find("Players/Ari the brave");
    onServer["Health"] = 50;
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return avatar["Health"].As<int>() == 50; }));

    // When the owner leaves, the server can remove what it added.
    server.OnDisconnected = [&](Connection client, std::string) {
        for (Node node : shared.NodesOwnedBy(client))
        {
            node.Remove();
        }
    };
    firstClient.Disconnect();
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return !second.Find("Players/Ari the brave") && !game.Find("Players/Ari the brave"); }));
    EASYFORGE_EXPECT_EQUAL(second.ToText(), game.ToText());
    EASYFORGE_EXPECT(shared.NodesOwnedBy(owner).empty());
}

EASYFORGE_TEST(SharingKeepsUpOnAPoorNetwork)
{
    NetworkConditions poor { .Loss = 0.1f, .Latency = 0.05f, .Jitter = 0.02f };
    Table game = MakeGame();
    Server server = LocalServer({ .Conditions = poor });
    Share(game, server, Sharing::TwoWay);
    Table copy = Table::New();
    Client client = LocalClient(server, { .Conditions = poor });
    TableShare received = Share(copy, client);
    auto update = [&] {
        server.Update();
        client.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return received.IsReady(); }));

    Node score = copy.Find("Players").Add("Score");
    for (int frame = 0; frame < 120; ++frame)
    {
        game.Find("World/Goblin")["Health"] = frame;
        score["Points"] = frame * 10;
        if (frame % 20 == 0)
        {
            game.Find("World").Add("Coin " + std::to_string(frame), { { "Value", frame } });
        }
        update();
    }
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return copy.ToText() == game.ToText(); }, 20.0));
    EASYFORGE_EXPECT_EQUAL(copy.ToText(), game.ToText());
    EASYFORGE_EXPECT_EQUAL(game.Find("Players/Score")["Points"].As<int>(), 1190);
}

EASYFORGE_TEST(ClientsThatShareFirstAreAnswered)
{
    Table game = MakeGame();
    Server server = LocalServer();
    Table copy = Table::New();
    Client client = LocalClient(server);
    TableShare received = Share(copy, client);
    auto update = [&] {
        server.Update();
        client.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return client.IsConnected(); }));
    UpdateFor(update, 0.1);
    EASYFORGE_EXPECT(!received.IsReady());

    // The server starts sharing later, and the client asks again.
    Share(game, server);
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return received.IsReady(); }));
    EASYFORGE_EXPECT_EQUAL(copy.ToText(), game.ToText());
}

EASYFORGE_TEST(StoppedSharesHearNothingMore)
{
    Table game = MakeGame();
    Server server = LocalServer();
    Share(game, server);
    Table copy = Table::New();
    Client client = LocalClient(server);
    TableShare received = Share(copy, client);
    auto update = [&] {
        server.Update();
        client.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return received.IsReady(); }));

    received.Stop();
    EASYFORGE_EXPECT(!received);
    game.Find("World/Goblin")["Health"] = 5;
    UpdateFor(update, 0.2);
    EASYFORGE_EXPECT_EQUAL(copy.Find("World/Goblin")["Health"].As<int>(), 30);

    TableShare none;
    EASYFORGE_EXPECT(!none);
    EASYFORGE_EXPECT(!none.IsReady());
    none.Stop();
}
