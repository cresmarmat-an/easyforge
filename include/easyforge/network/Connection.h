#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <easyforge/network/Message.h>

namespace easyforge
{
    namespace internal::networking
    {
        class Endpoint;
    }

    enum class Delivery
    {
        // Arrives once, in the order it was sent.
        Reliable,

        // May be lost, and an older message that arrives after a newer one of
        // the same name is dropped, so the newest wins. For positions sent many
        // times a second. One too large for a single packet, over about 1000
        // bytes, is sent as ReliableUnordered.
        Unreliable,

        // Arrives once, but possibly before messages sent earlier.
        ReliableUnordered,
    };

    // Trouble to put on purpose into everything one end sends, for testing a
    // program on a poor network from a good one.
    struct NetworkConditions
    {
        // The share of packets lost, from 0 to 1.
        float Loss = 0.0f;

        // Seconds every packet is held back, and up to how many more or fewer at
        // random, which also puts some out of order.
        float Latency = 0.0f;
        float Jitter = 0.0f;
    };

    // The answer to a request. It tests as false when the request failed: the
    // other end refused it, it timed out, or the connection closed.
    struct Reply
    {
        easyforge::Message Message;
        std::string Error;

        explicit operator bool() const { return Error.empty(); }
    };

    struct RequestSettings
    {
        // Seconds to wait for the answer.
        float Timeout = 5.0f;
    };

    // The other end of a connection: a client, as a server sees it, or the
    // server, as a client sees it.
    //
    // Connection is a handle. Once the connection closes it tests as false and
    // does nothing; a new connection never takes over an old one's handles.
    class Connection
    {
    public:
        // A handle that refers to no connection.
        Connection() = default;

        explicit operator bool() const { return IsConnected(); }
        bool IsConnected() const;

        // On a server, a number from 0 below the most clients, the same for as
        // long as the client stays connected and free for another afterwards.
        // On a client, 0.
        int Index() const;

        // The other end's address and port, such as "192.168.1.20:50312".
        std::string Address() const;

        // Seconds for a packet to get there and back, smoothed.
        float RoundTrip() const;

        void Send(std::string_view name, const Message& message = {}, Delivery delivery = Delivery::Reliable) const;

        // Asks the other end, which answers with its OnRequest handler for the
        // name. The callback runs during a later Update, once.
        void Request(std::string_view name, const Message& message, std::function<void(const Reply&)> callback,
            const RequestSettings& settings = {}) const;

        void Disconnect() const;

        bool operator==(const Connection& other) const;

    private:
        Connection(std::weak_ptr<internal::networking::Endpoint> endpoint, int slot, std::uint32_t generation)
            : Endpoint(std::move(endpoint)), Slot(slot), Generation(generation)
        {
        }

        std::weak_ptr<internal::networking::Endpoint> Endpoint;
        int Slot = -1;
        std::uint32_t Generation = 0;

        friend class internal::networking::Endpoint;
    };
}
