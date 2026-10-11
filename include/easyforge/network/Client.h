#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <easyforge/core/Host.h>
#include <easyforge/core/Property.h>
#include <easyforge/core/Result.h>
#include <easyforge/network/Connection.h>
#include <easyforge/network/Export.h>
#include <easyforge/network/Message.h>

namespace easyforge
{
    struct ClientSettings
    {
        // A name such as "localhost" or an address such as "192.168.1.20".
        std::string Address = "127.0.0.1";
        std::uint16_t Port = 7777;

        // Runs the network on a thread of its own, so Update is not needed;
        // handlers then run on that thread.
        bool Threaded = false;

        NetworkConditions Conditions {};

        // Seconds without hearing from the server before the connection closes,
        // and to wait for the server to accept.
        float Timeout = 5.0f;
        float ConnectTimeout = 5.0f;

        // The largest message, request, or answer this end sends or takes, in
        // bytes, from 1 KB to 60 MB. The server closes the connection when it
        // is sent a larger one than its own limit, so they should agree.
        std::size_t MessageLimit = 4u << 20;

        // The most the connection may hold in large messages still arriving,
        // at least twice MessageLimit. A server that sends more is left.
        std::size_t UnfinishedLimit = 16u << 20;

        // The server's NetworkIdentity::PublicKey(). When given, the client
        // connects only to a server that proves it holds that identity, so
        // nobody can stand in for it. Empty connects to whichever server
        // answers at the address.
        std::string ServerKey {};
    };

    // Connects to a server and talks with it, the same ways it talks back.
    //
    //     Client client = Client::New({ .Address = "127.0.0.1", .Port = 7777 });
    //     client.Send("Chat", { { "Text", "hello" } });
    //     client.Request("GetScore", {}, [](const Reply& reply) {
    //         if (reply)
    //         {
    //             int score = reply.Message["Score"];
    //         }
    //     });
    //     client.Update();   // each frame
    //
    // Client is a handle: copies share the client, which disconnects when the
    // last copy goes. Every function can be called from any thread; messages
    // sent before the connection is made wait for it.
    class EASYFORGE_NETWORK_EXPORT Client
    {
    public:
        // A client that is not connecting. It tests as false.
        Client();

        // Starts connecting. When the address cannot be understood, the client
        // tests as false and Error() says why; whether the server answers is
        // told by OnConnected or OnDisconnected.
        [[nodiscard]] static Client New(const ClientSettings& settings = {});

        // A client that follows a host, such as a window, the same way a
        // server can: handlers run in the host's frames, and Update is not needed.
        [[nodiscard]] static Client New(std::shared_ptr<Host> host, const ClientSettings& settings = {});

        Client(const Client& other);
        Client& operator=(const Client& other);
        ~Client();

        explicit operator bool() const;
        std::string Error() const;

        bool IsConnected() const;

        // The connection to the server. It tests as false until the server
        // accepts, and what is sent through it before then waits; once the
        // connection closes, it stays closed.
        Connection Server() const;

        void OnMessage(std::string_view name, std::function<void(const Message&)> handler) const;
        void OnRequest(std::string_view name, std::function<Result<Message>(const Message&)> handler) const;

        void Send(std::string_view name, const Message& message = {}, Delivery delivery = Delivery::Reliable) const;
        void Request(std::string_view name, const Message& message, std::function<void(const Reply&)> callback,
            const RequestSettings& settings = {}) const;

        // Seconds for a packet to reach the server and come back, smoothed.
        float RoundTrip() const;

        // Call once a frame, unless the client is threaded.
        void Update() const;

        void Disconnect() const;

        std::size_t AfterUpdate(std::function<void()> handler) const;
        void RemoveAfterUpdate(std::size_t handler) const;

        // The trouble put into what the client sends, which can be changed
        // while it runs.
        Property<NetworkConditions> Conditions;

        Property<std::function<void()>> OnConnected;

        // With why: "disconnected" after Disconnect, "timed out", "the server
        // stopped", "the server closed the connection", "the server is full", or
        // "the server did not answer".
        Property<std::function<void(std::string)>> OnDisconnected;

    private:
        explicit Client(std::shared_ptr<internal::networking::Endpoint> state);
        void RebindProperties();

        std::shared_ptr<internal::networking::Endpoint> State;
    };
}
