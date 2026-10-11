#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "NetworkTesting.h"
#include "TestHost.h"

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

EASYFORGE_TEST(AThreadedServerEndsWithItsLastHandle)
{
    // Its thread holds it while updating it, which once let it end later, on
    // that thread. Now the port is free the moment the handle is gone.
    std::uint16_t port = 0;
    for (int round = 0; round < 20; ++round)
    {
        Server server = Server::New({ .Port = port, .ThisComputerOnly = true, .Threaded = true });
        EASYFORGE_REQUIRE(server);
        port = server.Port();
        Client client = Client::New({ .Port = port, .Threaded = true });
        EASYFORGE_REQUIRE(client);
    }
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

EASYFORGE_TEST(ManyThreadsSendAndAskAtOnce)
{
    // Threaded ends, with several program threads per client sending reliable
    // messages and requests together: every message arrives exactly once and
    // every request is answered.
    Server server = LocalServer({ .Threaded = true });
    EASYFORGE_REQUIRE(server);
    std::mutex lock;
    std::vector<int> heard(3 * 4 * 200, 0);
    server.OnMessage("Tick", [&](Connection, const Message& message) {
        std::scoped_lock guard(lock);
        ++heard[static_cast<std::size_t>(message["Number"].As<int>())];
    });
    server.OnRequest("Double", [](Connection, const Message& request) { return Message { { "Value", request["Value"].As<int>() * 2 } }; });

    std::vector<Client> clients;
    for (int index = 0; index < 3; ++index)
    {
        clients.push_back(LocalClient(server, { .Threaded = true }));
    }
    EASYFORGE_REQUIRE(WaitUntil([&] {
        for (const Client& client : clients)
        {
            if (!client.IsConnected())
            {
                return false;
            }
        }
        return true;
    }));

    std::atomic<int> answered { 0 };
    std::atomic<int> wrong { 0 };
    std::vector<std::thread> senders;
    for (int client = 0; client < 3; ++client)
    {
        for (int thread = 0; thread < 4; ++thread)
        {
            senders.emplace_back([&, client, thread] {
                for (int index = 0; index < 200; ++index)
                {
                    int number = (client * 4 + thread) * 200 + index;
                    clients[static_cast<std::size_t>(client)].Send("Tick", { { "Number", number } });
                    if (index % 10 == 0)
                    {
                        clients[static_cast<std::size_t>(client)].Request("Double", { { "Value", number } }, [&, number](const Reply& reply) {
                            if (reply && reply.Message["Value"].As<int>() == number * 2)
                            {
                                ++answered;
                            }
                            else
                            {
                                ++wrong;
                            }
                        });
                    }
                }
            });
        }
    }
    for (std::thread& sender : senders)
    {
        sender.join();
    }
    EASYFORGE_EXPECT(WaitUntil(
        [&] {
            std::scoped_lock guard(lock);
            for (int count : heard)
            {
                if (count != 1)
                {
                    return false;
                }
            }
            return answered.load() == 3 * 4 * 20;
        },
        15.0));
    EASYFORGE_EXPECT_EQUAL(wrong.load(), 0);
    std::scoped_lock guard(lock);
    for (int count : heard)
    {
        EASYFORGE_EXPECT(count <= 1);
    }
}

EASYFORGE_TEST(ServersAndClientsCanFollowAHost)
{
    // The handlers run in the host's frames, on its thread, and the program
    // calls no Update. Messages wake a host that is waiting.
    auto host = std::make_shared<TestHost>();
    std::thread::id frameThread = std::this_thread::get_id();
    std::atomic<bool> elsewhere = false;
    int pongs = 0;
    {
        Server server = Server::New(host, { .Port = 0, .ThisComputerOnly = true });
        EASYFORGE_REQUIRE(server);
        server.OnMessage("Ping", [&](Connection from, const Message& message) {
            elsewhere = elsewhere || std::this_thread::get_id() != frameThread;
            from.Send("Pong", message);
        });
        Client client = Client::New(host, { .Address = "127.0.0.1", .Port = server.Port() });
        client.OnMessage("Pong", [&](const Message&) {
            elsewhere = elsewhere || std::this_thread::get_id() != frameThread;
            ++pongs;
        });
        EASYFORGE_EXPECT_EQUAL(host->Listeners.size(), std::size_t { 2 });

        EASYFORGE_REQUIRE(WaitUntil([&] {
            host->RunFrameIfWanted();
            return client.IsConnected();
        }));
        client.Send("Ping", { { "Count", 1 } });
        EASYFORGE_EXPECT(WaitUntil([&] {
            host->RunFrameIfWanted();
            return pongs == 1;
        }));
        EASYFORGE_EXPECT(!elsewhere.load());

        // A quiet connection keeps itself alive without asking for frames.
        int before = host->Requests;
        WaitUntil([&] {
            host->RunFrameIfWanted();
            return false;
        }, 0.6);
        EASYFORGE_EXPECT(host->Requests - before <= 1);
    }
    EASYFORGE_EXPECT(host->Listeners.empty());
}
