#pragma once

// The engine under Server and Client: the socket, the handshake, the
// connections, requests and their callbacks, simulated network trouble, and
// the optional thread.
//
// Everything is guarded by one mutex. Work that runs the program's handlers is
// collected as events while the mutex is held, and run after it is released,
// so a handler can call back into the server or client freely.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <easyforge/core/Random.h>
#include <easyforge/network/Client.h>
#include <easyforge/network/Server.h>

#include "Peer.h"
#include "Socket.h"

namespace easyforge::internal::networking
{
    using MessageHandler = std::function<void(Connection, const Message&)>;
    using RequestHandler = std::function<Result<Message>(Connection, const Message&)>;

    class Endpoint
    {
    public:
        enum class Role
        {
            Server,
            Client,
        };

        static std::shared_ptr<Endpoint> StartServer(const ServerSettings& settings);
        static std::shared_ptr<Endpoint> StartClient(const ClientSettings& settings);

        Endpoint(const Endpoint&) = delete;
        Endpoint& operator=(const Endpoint&) = delete;
        ~Endpoint();

        std::string Error() const;
        std::uint16_t Port() const;

        void SetMessageHandler(std::string_view name, MessageHandler handler);
        void SetRequestHandler(std::string_view name, RequestHandler handler);

        // Does nothing on a threaded endpoint, whose thread updates it.
        void Update();

        // Closes every connection, telling the other ends, and runs the handlers
        // for it before returning. A server stops listening; a client is done.
        void Stop();

        std::size_t AddAfterUpdate(std::function<void()> handler);
        void RemoveAfterUpdate(std::size_t handler);

        // Connections, by slot and the generation the handle was given.
        bool IsConnected(int slot, std::uint32_t generation) const;
        std::string AddressOf(int slot, std::uint32_t generation) const;
        float RoundTripOf(int slot, std::uint32_t generation) const;
        void Send(int slot, std::uint32_t generation, std::string_view name, const Message& message, Delivery delivery);
        void Request(int slot, std::uint32_t generation, std::string_view name, const Message& message,
            std::function<void(const Reply&)> callback, const RequestSettings& settings);
        void Disconnect(int slot, std::uint32_t generation);

        void SendToAll(std::string_view name, const Message& message, Delivery delivery);
        std::vector<Connection> Connections() const;

        // A client's single connection: its generation counts attempts, and the
        // handle exists only while connected.
        std::uint32_t ClientGeneration() const;
        bool ClientIsConnected() const;
        Connection ServerConnection() const;

    private:
        struct Slot
        {
            bool Used = false;
            bool Connected = false;
            std::uint32_t Generation = 0;
            SocketAddress Address;
            std::uint64_t ClientSalt = 0;
            std::uint64_t ServerSalt = 0;
            double Started = 0.0;
            double LastConnectRequest = -1.0;
            std::unique_ptr<Peer> Link;
            std::map<std::string, std::uint32_t, std::less<>> NewestUnreliable;

            std::uint64_t Token() const { return ClientSalt ^ ServerSalt; }
        };

        struct PendingRequest
        {
            int Slot = 0;
            std::uint32_t Generation = 0;
            double Deadline = 0.0;
            std::function<void(const Reply&)> Callback;
        };

        struct DelayedPacket
        {
            double Release = 0.0;
            SocketAddress To;
            std::vector<std::uint8_t> Bytes;
        };

        explicit Endpoint(Role role);

        static void Work(std::weak_ptr<Endpoint> endpoint);

        double Now() const;
        std::uint64_t NewSalt();
        void UpdateNow();
        void ReceivePackets(double now);
        void HandlePacket(double now, const SocketAddress& from, std::span<const std::uint8_t> bytes);
        void HandleConnectRequest(double now, const SocketAddress& from, std::uint64_t salt);
        void HandleDelivered(int slot, DeliveredMessage delivered);
        void CheckTimes(double now);
        void SendPackets(double now);
        void FlushSlot(int slot, double now);
        void SendAccept(int slot);
        void SendReply(int slot, std::uint32_t generation, std::uint32_t request, const Result<Message>& answer);

        // Sends through the simulated trouble.
        void SendRaw(const SocketAddress& to, std::vector<std::uint8_t> bytes);

        // Sends at once, past the simulated trouble, for the last word on a connection.
        void SendNow(const SocketAddress& to, std::span<const std::uint8_t> bytes);

        void Close(int slot, const std::string& reason, bool tellOtherEnd, DisconnectReason code);
        void ForgetHandlers();
        int SlotFor(const SocketAddress& from) const;
        Slot* FindSlot(int slot, std::uint32_t generation);
        const Slot* FindSlot(int slot, std::uint32_t generation) const;
        Connection MakeConnection(int slot) const;
        static void RunEvents(std::vector<std::function<void()>>& events);

        // Handlers behind the Server and Client properties, read and written
        // under the mutex.
        template <typename Value>
        Value Read(Value Endpoint::*member) const
        {
            std::scoped_lock lock(Lock);
            return this->*member;
        }

        template <typename Value>
        void Write(Value Endpoint::*member, const Value& value)
        {
            std::scoped_lock lock(Lock);
            this->*member = value;
        }

        mutable std::mutex Lock;
        Role Kind;
        std::weak_ptr<Endpoint> Self;
        std::string Problem;
        std::shared_ptr<UdpSocket> Socket;
        std::uint16_t BoundPort = 0;
        bool Running = false;
        bool Threaded = false;
        std::string Name;
        float Timeout = 5.0f;
        float ConnectTimeout = 5.0f;
        NetworkConditions Conditions;
        Random Chance;
        std::uint64_t Instance = 0;
        std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();

        std::vector<Slot> Slots;
        std::map<SocketAddress, int> SlotsByAddress;
        SocketAddress ServerAddress;

        std::map<std::string, std::shared_ptr<MessageHandler>, std::less<>> MessageHandlers;
        std::map<std::string, std::shared_ptr<RequestHandler>, std::less<>> RequestHandlers;
        std::map<std::uint32_t, PendingRequest> Requests;
        std::uint32_t NextRequest = 1;
        std::map<std::size_t, std::shared_ptr<std::function<void()>>> AfterUpdateHandlers;
        std::size_t NextAfterUpdate = 1;

        std::function<void(Connection)> ServerConnected;
        std::function<void(Connection, std::string)> ServerDisconnected;
        std::function<void()> ClientConnected;
        std::function<void(std::string)> ClientDisconnected;

        std::vector<DelayedPacket> Delayed;
        std::vector<std::function<void()>> Events;
        bool Updating = false;

        std::atomic<bool> Stopping = false;
        std::thread Worker;

        friend class easyforge::Server;
        friend class easyforge::Client;
    };
}
