#pragma once

// The engine under Server and Client: the socket, the handshake, the
// connections, requests and their callbacks, simulated network trouble, and
// the optional thread.
//
// Everything is guarded by one mutex. Work that runs the program's handlers is
// collected as events while the mutex is held, and run after it is released,
// so a handler can call back into the server or client freely.

#include <array>
#include <atomic>
#include <bitset>
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

#include "Crypto.h"
#include "Peer.h"
#include "Socket.h"

namespace easyforge::internal::networking
{
    // Which packet counters have arrived on a connection, so each is taken
    // once: the newest, and a window of the ones before it.
    class ReplayWindow
    {
    public:
        // Whether the counter could still be taken: not seen, not too old.
        bool IsFresh(std::uint64_t counter) const;

        // Takes the counter, once its packet has opened.
        void Take(std::uint64_t counter);

    private:
        std::uint64_t Newest = 0;
        std::bitset<ReplayWindowSize> Seen;
    };

    using Cookie = std::array<std::uint8_t, CookieSize>;

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

            // The handshake: the client's key pair until the keys are agreed,
            // and the server's cookie once it came.
            std::unique_ptr<KeyExchange> Exchange;
            Cookie Challenge {};
            bool HasChallenge = false;
            int ResponsesSent = 0;

            // The server's half of the accept, kept to send again if it is lost.
            PublicKey ClientPublic {};
            PublicKey ServerPublic {};
            Signature ServerSignature {};
            bool Signed = false;

            // One key each way. Packets sent are counted from 1, and the
            // counters received are taken once each.
            std::unique_ptr<Sealer> Outgoing;
            std::unique_ptr<Sealer> Incoming;
            std::uint64_t SentPackets = 0;
            ReplayWindow Received;

            std::uint64_t Token() const { return ClientSalt ^ ServerSalt; }
        };

        // Connect attempts an address has left, refilled with time.
        struct ConnectAllowance
        {
            double Attempts = ConnectAttemptBurst;
            double Updated = 0.0;
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

        // This endpoint's time as NetworkTime.
        double ToNetworkTime(double time) const;
        std::uint64_t NewSalt();
        void UpdateNow();
        void ReceivePackets(double now);
        void HandlePacket(double now, const SocketAddress& from, std::span<const std::uint8_t> bytes);
        void HandleConnectRequest(double now, const SocketAddress& from, std::uint64_t salt);
        void HandleConnectResponse(double now, const SocketAddress& from, std::uint64_t salt, const Cookie& cookie,
            const PublicKey& clientPublic);
        void HandleConnectAccept(double now, ByteReader& reader);

        // Cookies, made from the address and when they run out, so the server
        // keeps nothing for an address until it proves it receives there.
        Cookie MakeCookie(const SocketAddress& from, std::uint64_t salt, std::uint64_t expiry) const;
        bool CheckCookie(double now, const SocketAddress& from, std::uint64_t salt, const Cookie& cookie) const;
        bool AllowConnectAttempt(double now, const SocketAddress& from);

        // Sets the slot's keys from the agreed secret. False when the system
        // could not make them.
        bool SetKeys(Slot& slot, const SecretKey& shared);

        // Whatever the handshake signs: both salts and both public keys.
        static std::vector<std::uint8_t> AcceptTranscript(const Slot& slot);

        // A plain packet (magic, kind, token, then the rest) sealed for the
        // slot, or opened from it. Unseal is false for a packet that was
        // changed, sealed with another key, or seen before.
        std::vector<std::uint8_t> Seal(Slot& slot, std::span<const std::uint8_t> plain);
        bool Unseal(Slot& slot, std::span<const std::uint8_t> packet, std::vector<std::uint8_t>& body);
        std::vector<std::uint8_t> DisconnectPacket(Slot& slot, DisconnectReason code);
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

        // The server's: the secret its cookies are made with, its identity, and
        // the connect attempts each address has left.
        SecretKey CookieSecret {};
        std::shared_ptr<SigningKey> Identity;
        std::map<SocketAddress, ConnectAllowance> ConnectAllowances;

        // The client's: the server key it insists on, when it has one.
        bool PinsServer = false;
        PublicKey PinnedServer {};

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
