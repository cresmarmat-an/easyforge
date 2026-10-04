#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "NetworkTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(ThreadedEndsNeedNoUpdates)
{
    Server server = LocalServer({ .Threaded = true });
    EASYFORGE_REQUIRE(server);
    std::atomic<int> serverHeard = 0;
    server.OnMessage("Ping", [&](Connection from, const Message& message) {
        ++serverHeard;
        from.Send("Pong", message);
    });
    server.OnRequest("Echo", [](Connection, const Message& request) { return request; });

    Client client = LocalClient(server, { .Threaded = true });
    std::atomic<int> clientHeard = 0;
    std::mutex lock;
    std::string echoed;
    client.OnMessage("Pong", [&](const Message&) { ++clientHeard; });

    for (int count = 0; count < 20; ++count)
    {
        client.Send("Ping", { { "Count", count } });
    }
    client.Request("Echo", { { "Text", "back" } }, [&](const Reply& reply) {
        std::scoped_lock guard(lock);
        echoed = reply.Message["Text"].AsText();
    });

    // Update does nothing on a threaded end; the threads do the work.
    client.Update();
    EASYFORGE_EXPECT(WaitUntil([&] {
        std::scoped_lock guard(lock);
        return serverHeard == 20 && clientHeard == 20 && !echoed.empty();
    }));
    EASYFORGE_EXPECT_EQUAL(echoed, std::string("back"));
    EASYFORGE_EXPECT(client.IsConnected());

    // Stopping from the program's thread while the network thread runs.
    std::atomic<bool> told = false;
    client.OnDisconnected = [&](std::string) { told = true; };
    server.Stop();
    EASYFORGE_EXPECT(WaitUntil([&] { return told.load(); }));
}

EASYFORGE_TEST(ThreadedHandlersMayHoldTheirServer)
{
    // The handler holds the last copy of the server once `server` lets go, so
    // the server ends on its own thread after Stop.
    std::atomic<bool> stopped = false;
    std::uint16_t port = 0;
    {
        Server server = LocalServer({ .Threaded = true });
        port = server.Port();
        server.OnMessage("Stop", [server, &stopped](Connection, const Message&) {
            server.Stop();
            stopped = true;
        });
        Client client = Client::New({ .Port = port });
        client.Send("Stop");
        EASYFORGE_EXPECT(UpdateUntil([&] { client.Update(); }, [&] { return stopped.load(); }));
    }

    // The port is free again once the server is gone.
    Server again;
    EASYFORGE_EXPECT(WaitUntil(
        [&] {
            again = Server::New({ .Port = port, .ThisComputerOnly = true });
            return static_cast<bool>(again);
        },
        2.0));
}

EASYFORGE_TEST(ServersCanBeFound)
{
    // The server must answer while FindServers waits, so it runs on its own thread.
    Server server = LocalServer({ .MaximumClients = 8, .Name = "Test room", .Threaded = true });
    EASYFORGE_REQUIRE(server);
    Client client = LocalClient(server);
    EASYFORGE_REQUIRE(UpdateUntil([&] { client.Update(); }, [&] { return client.IsConnected(); }));

    std::vector<FoundServer> found = FindServers({ .Port = server.Port(), .Seconds = 0.3f, .ThisComputerOnly = true });
    EASYFORGE_REQUIRE(found.size() == 1);
    EASYFORGE_EXPECT_EQUAL(found[0].Name, std::string("Test room"));
    EASYFORGE_EXPECT_EQUAL(found[0].Address, std::string("127.0.0.1"));
    EASYFORGE_EXPECT_EQUAL(found[0].Port, server.Port());
    EASYFORGE_EXPECT_EQUAL(found[0].Clients, 1);
    EASYFORGE_EXPECT_EQUAL(found[0].MaximumClients, 8);

    // Nothing answers on a port no server uses.
    server.Stop();
    std::vector<FoundServer> none = FindServers({ .Port = server.Port(), .Seconds = 0.1f, .ThisComputerOnly = true });
    EASYFORGE_EXPECT(none.empty());
}
