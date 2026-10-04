#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Property.h>
#include <easyforge/core/Result.h>
#include <easyforge/network/Connection.h>
#include <easyforge/network/Message.h>

namespace easyforge
{
    struct ServerSettings
    {
        // The UDP port to listen on; 0 takes any free one, which Port() gives.
        std::uint16_t Port = 7777;

        int MaximumClients = 16;

        // Shown to programs looking for servers on the local network.
        std::string Name = "easyforge";

        // Accepts only programs on this computer. The system firewall does not
        // ask about such a server.
        bool ThisComputerOnly = false;

        // Runs the network on a thread of its own, so Update is not needed;
        // handlers then run on that thread.
        bool Threaded = false;

        NetworkConditions Conditions;

        // Seconds without hearing from a client before it is dropped.
        float Timeout = 5.0f;
    };

    // Accepts clients, and talks with them: one-way messages, and requests that
    // get answers.
    //
    //     Server server = Server::New({ .Port = 7777, .MaximumClients = 16 });
    //     server.OnMessage("Chat", [&](Connection from, const Message& message) {
    //         server.SendToAll("Chat", message);
    //     });
    //     server.OnRequest("GetScore", [&](Connection from, const Message& request) {
    //         return Message { { "Score", scores[from.Index()] } };
    //     });
    //     server.Update();   // each frame
    //
    // Server is a handle: copies share the server, which stops, telling its
    // clients, when the last copy goes. Every function can be called from any
    // thread.
    class Server
    {
    public:
        // A server that is not running. It tests as false.
        Server();

        // Starts listening. When the port cannot be used, the server tests as
        // false and Error() says why.
        static Server New(const ServerSettings& settings = {});

        Server(const Server& other);
        Server& operator=(const Server& other);
        ~Server();

        explicit operator bool() const;
        std::string Error() const;

        // The handler for messages, or requests, of a name; a later handler for
        // the same name replaces the earlier one. A request handler answers with
        // a message, or a Failure the asker gets as the reply's error.
        void OnMessage(std::string_view name, std::function<void(Connection, const Message&)> handler) const;
        void OnRequest(std::string_view name, std::function<Result<Message>(Connection, const Message&)> handler) const;

        void SendToAll(std::string_view name, const Message& message = {}, Delivery delivery = Delivery::Reliable) const;

        std::vector<Connection> Connections() const;

        // The port the server listens on.
        std::uint16_t Port() const;

        // Receives, answers, resends, and runs the handlers for what arrived.
        // Call it once a frame, unless the server is threaded.
        void Update() const;

        // Closes every connection and stops listening.
        void Stop() const;

        // For code that works alongside the server, such as a bridge: runs the
        // handler at the end of every update, until removed by the number it
        // returns.
        std::size_t AfterUpdate(std::function<void()> handler) const;
        void RemoveAfterUpdate(std::size_t handler) const;

        // The trouble put into what the server sends, which can be changed
        // while it runs.
        Property<NetworkConditions> Conditions;

        Property<std::function<void(Connection)>> OnConnected;

        // With why: "disconnected", "timed out", or "the server stopped".
        Property<std::function<void(Connection, std::string)>> OnDisconnected;

    private:
        explicit Server(std::shared_ptr<internal::networking::Endpoint> state);
        void RebindProperties();

        std::shared_ptr<internal::networking::Endpoint> State;
    };

    struct FoundServer
    {
        std::string Name;
        std::string Address;
        std::uint16_t Port = 0;
        int Clients = 0;
        int MaximumClients = 0;
    };

    struct ServerSearchSettings
    {
        std::uint16_t Port = 7777;

        // How long to wait for answers.
        float Seconds = 0.5f;

        // Asks only this computer, not the local network.
        bool ThisComputerOnly = false;
    };

    // Asks every computer on the local network, this one included, for servers
    // listening on a port, and waits for their answers. A server is listed once
    // even when it answers on several networks.
    std::vector<FoundServer> FindServers(const ServerSearchSettings& settings = {});
}
