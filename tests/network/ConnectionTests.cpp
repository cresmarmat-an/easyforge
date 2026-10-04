#include <string>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "NetworkTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(ClientsConnectAndTalk)
{
    Server server = LocalServer({ .Name = "Talk" });
    EASYFORGE_REQUIRE(server);
    EASYFORGE_EXPECT(server.Port() != 0);

    std::vector<Connection> joined;
    std::vector<std::string> heard;
    server.OnConnected = [&](Connection client) { joined.push_back(client); };
    server.OnMessage("Chat", [&](Connection from, const Message& message) {
        heard.push_back(message["Text"].AsText());
        from.Send("Chat", { { "Text", "welcome" } });
    });

    Client client = LocalClient(server);
    EASYFORGE_REQUIRE(client);
    bool connected = false;
    std::vector<std::string> answers;
    client.OnConnected = [&] { connected = true; };
    client.OnMessage("Chat", [&](const Message& message) { answers.push_back(message["Text"]); });

    // Sent before the connection is made, so it waits for it.
    client.Send("Chat", { { "Text", "hello" } });

    auto update = [&] {
        server.Update();
        client.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return answers.size() == 1; }));
    EASYFORGE_EXPECT(connected);
    EASYFORGE_EXPECT(client.IsConnected());
    EASYFORGE_EXPECT((heard == std::vector<std::string> { "hello" }));
    EASYFORGE_EXPECT_EQUAL(answers[0], std::string("welcome"));

    EASYFORGE_REQUIRE(joined.size() == 1);
    EASYFORGE_EXPECT(joined[0]);
    EASYFORGE_EXPECT_EQUAL(joined[0].Index(), 0);
    EASYFORGE_EXPECT(joined[0].Address().starts_with("127.0.0.1:"));
    EASYFORGE_EXPECT(server.Connections().size() == 1);
    EASYFORGE_EXPECT(server.Connections()[0] == joined[0]);

    Connection toServer = client.Server();
    EASYFORGE_EXPECT(toServer);
    EASYFORGE_EXPECT_EQUAL(toServer.Address(), "127.0.0.1:" + std::to_string(server.Port()));

    // The round trip is measured from acknowledged packets.
    UpdateFor(update, 0.3);
    EASYFORGE_EXPECT(client.RoundTrip() > 0.0f);
    EASYFORGE_EXPECT(client.RoundTrip() < 0.5f);
    EASYFORGE_EXPECT(joined[0].RoundTrip() > 0.0f);
}

EASYFORGE_TEST(ServersSendToEveryClient)
{
    Server server = LocalServer();
    EASYFORGE_REQUIRE(server);
    Client first = LocalClient(server);
    Client second = LocalClient(server);
    int firstHeard = 0;
    int secondHeard = 0;
    first.OnMessage("News", [&](const Message&) { ++firstHeard; });
    second.OnMessage("News", [&](const Message&) { ++secondHeard; });

    auto update = [&] {
        server.Update();
        first.Update();
        second.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return server.Connections().size() == 2; }));
    std::vector<Connection> connections = server.Connections();
    EASYFORGE_EXPECT(connections[0].Index() != connections[1].Index());

    server.SendToAll("News", { { "Headline", "two clients" } });
    EASYFORGE_EXPECT(UpdateUntil(update, [&] { return firstHeard == 1 && secondHeard == 1; }));
}

EASYFORGE_TEST(FullServersRefuseClients)
{
    Server server = LocalServer({ .MaximumClients = 1 });
    Client first = LocalClient(server);
    Client second = LocalClient(server);
    std::string reason;
    second.OnDisconnected = [&](std::string why) { reason = why; };

    auto update = [&] {
        server.Update();
        first.Update();
        second.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return first.IsConnected() && !reason.empty(); }));
    EASYFORGE_EXPECT_EQUAL(reason, std::string("the server is full"));
    EASYFORGE_EXPECT(!second.IsConnected());
    EASYFORGE_EXPECT(server.Connections().size() == 1);
}

EASYFORGE_TEST(MissingServersDoNotAnswer)
{
    // A port that was free a moment ago.
    std::uint16_t port = 0;
    {
        Server taken = LocalServer();
        port = taken.Port();
    }
    Client client = Client::New({ .Port = port, .ConnectTimeout = 0.3f });
    EASYFORGE_REQUIRE(client);
    std::string reason;
    client.OnDisconnected = [&](std::string why) { reason = why; };
    EASYFORGE_EXPECT(UpdateUntil([&] { client.Update(); }, [&] { return !reason.empty(); }));
    EASYFORGE_EXPECT_EQUAL(reason, std::string("the server did not answer"));
}

EASYFORGE_TEST(ProblemsStartingAreReported)
{
    Server first = LocalServer();
    EASYFORGE_REQUIRE(first);
    Server second = Server::New({ .Port = first.Port(), .ThisComputerOnly = true });
    EASYFORGE_EXPECT(!second);
    EASYFORGE_EXPECT(second.Error().find("in use") != std::string::npos);

    Client nowhere = Client::New({ .Address = "" });
    EASYFORGE_EXPECT(!nowhere);
    EASYFORGE_EXPECT(!nowhere.Error().empty());

    Server none;
    Client nobody;
    EASYFORGE_EXPECT(!none);
    EASYFORGE_EXPECT(!nobody);
    EASYFORGE_EXPECT(!nobody.IsConnected());
    EASYFORGE_EXPECT(!nobody.Server());
    none.Update();
    nobody.Send("Nothing");
}

EASYFORGE_TEST(DisconnectsAreReported)
{
    Server server = LocalServer();
    std::vector<std::string> serverReasons;
    server.OnDisconnected = [&](Connection, std::string why) { serverReasons.push_back(why); };

    Client leaving = LocalClient(server);
    Client first = LocalClient(server);
    Client second = LocalClient(server);
    std::string firstReason;
    std::string secondReason;
    first.OnDisconnected = [&](std::string why) { firstReason = why; };
    second.OnDisconnected = [&](std::string why) { secondReason = why; };

    auto update = [&] {
        server.Update();
        leaving.Update();
        first.Update();
        second.Update();
    };
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return server.Connections().size() == 3 && second.IsConnected(); }));
    Connection secondToServer = second.Server();

    // A client leaving.
    leaving.Disconnect();
    EASYFORGE_EXPECT(!leaving.IsConnected());
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return serverReasons.size() == 1; }));
    EASYFORGE_EXPECT_EQUAL(serverReasons[0], std::string("disconnected"));

    // The server dropping a client. Its own handler runs before Disconnect returns.
    std::vector<Connection> connections = server.Connections();
    EASYFORGE_REQUIRE(connections.size() == 2);
    connections[0].Disconnect();
    EASYFORGE_EXPECT(!connections[0]);
    EASYFORGE_EXPECT_EQUAL(serverReasons.size(), std::size_t { 2 });
    EASYFORGE_REQUIRE(UpdateUntil(update, [&] { return !firstReason.empty() || !secondReason.empty(); }));
    bool firstDropped = !firstReason.empty();
    EASYFORGE_EXPECT_EQUAL(firstDropped ? firstReason : secondReason, std::string("the server closed the connection"));

    // The server stopping.
    Client remaining = firstDropped ? second : first;
    std::string& remainingReason = firstDropped ? secondReason : firstReason;
    server.Stop();
    EASYFORGE_EXPECT_EQUAL(serverReasons.size(), std::size_t { 3 });
    EASYFORGE_EXPECT_EQUAL(serverReasons.back(), std::string("the server stopped"));
    EASYFORGE_EXPECT(server.Connections().empty());
    EASYFORGE_REQUIRE(UpdateUntil([&] { remaining.Update(); }, [&] { return !remainingReason.empty(); }));
    EASYFORGE_EXPECT_EQUAL(remainingReason, std::string("the server stopped"));
    EASYFORGE_EXPECT(!secondToServer);
}

EASYFORGE_TEST(LettingGoOfAServerStopsIt)
{
    Server server = LocalServer();
    Client client = LocalClient(server);
    std::string reason;
    client.OnDisconnected = [&](std::string why) { reason = why; };
    EASYFORGE_REQUIRE(UpdateUntil(
        [&] {
            server.Update();
            client.Update();
        },
        [&] { return client.IsConnected(); }));

    server = Server();
    EASYFORGE_EXPECT(UpdateUntil([&] { client.Update(); }, [&] { return !reason.empty(); }));
    EASYFORGE_EXPECT_EQUAL(reason, std::string("the server stopped"));
}

EASYFORGE_TEST(SilentConnectionsTimeOut)
{
    Server server = LocalServer({ .Timeout = 0.4f });
    Client client = LocalClient(server, { .Timeout = 0.4f });
    std::string clientReason;
    std::string serverReason;
    client.OnDisconnected = [&](std::string why) { clientReason = why; };
    server.OnDisconnected = [&](Connection, std::string why) { serverReason = why; };
    EASYFORGE_REQUIRE(UpdateUntil(
        [&] {
            server.Update();
            client.Update();
        },
        [&] { return client.IsConnected() && server.Connections().size() == 1; }));

    // The server stops updating, so the client hears nothing more.
    EASYFORGE_REQUIRE(UpdateUntil([&] { client.Update(); }, [&] { return !clientReason.empty(); }));
    EASYFORGE_EXPECT_EQUAL(clientReason, std::string("timed out"));

    // The client told the server as it gave up.
    EASYFORGE_REQUIRE(UpdateUntil([&] { server.Update(); }, [&] { return !serverReason.empty(); }));
    EASYFORGE_EXPECT(serverReason == "disconnected" || serverReason == "timed out");
}

EASYFORGE_TEST(OldHandlesStayClosed)
{
    Server server = LocalServer({ .MaximumClients = 1 });
    Client first = LocalClient(server);
    EASYFORGE_REQUIRE(UpdateUntil(
        [&] {
            server.Update();
            first.Update();
        },
        [&] { return server.Connections().size() == 1; }));
    Connection old = server.Connections()[0];
    first.Disconnect();
    EASYFORGE_REQUIRE(UpdateUntil([&] { server.Update(); }, [&] { return server.Connections().empty(); }));

    // The next client gets the same slot, but the old handle does not become it.
    Client second = LocalClient(server);
    EASYFORGE_REQUIRE(UpdateUntil(
        [&] {
            server.Update();
            second.Update();
        },
        [&] { return server.Connections().size() == 1; }));
    Connection current = server.Connections()[0];
    EASYFORGE_EXPECT_EQUAL(current.Index(), old.Index());
    EASYFORGE_EXPECT(!old);
    EASYFORGE_EXPECT(current);
    EASYFORGE_EXPECT(!(old == current));
    old.Send("Ignored");
    old.Disconnect();
    EASYFORGE_EXPECT(current);
}

EASYFORGE_TEST(AfterUpdateHandlersRunEachUpdate)
{
    Server server = LocalServer();
    int runs = 0;
    std::size_t handler = server.AfterUpdate([&] { ++runs; });
    server.Update();
    server.Update();
    EASYFORGE_EXPECT_EQUAL(runs, 2);
    server.RemoveAfterUpdate(handler);
    server.Update();
    EASYFORGE_EXPECT_EQUAL(runs, 2);
}
