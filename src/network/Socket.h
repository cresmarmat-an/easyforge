#pragma once

// The platform's UDP sockets, behind one small interface. Windows uses Winsock;
// other platforms get theirs in their stages.

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace easyforge::internal::networking
{
    // An IPv4 address and port. Host is in the usual reading order, so
    // 127.0.0.1 is 0x7F000001.
    struct SocketAddress
    {
        std::uint32_t Host = 0;
        std::uint16_t Port = 0;

        bool IsLoopback() const { return (Host >> 24) == 127; }

        // "192.168.1.20:50312".
        std::string Text() const
        {
            return HostText() + ":" + std::to_string(Port);
        }

        std::string HostText() const
        {
            return std::to_string(Host >> 24) + "." + std::to_string((Host >> 16) & 255) + "." +
                   std::to_string((Host >> 8) & 255) + "." + std::to_string(Host & 255);
        }

        auto operator<=>(const SocketAddress&) const = default;
    };

    inline constexpr std::uint32_t LoopbackHost = 0x7F000001;
    inline constexpr std::uint32_t BroadcastHost = 0xFFFFFFFF;

    // A socket that never blocks: Receive returns at once when nothing is waiting.
    class UdpSocket
    {
    public:
        virtual ~UdpSocket() = default;

        // Sending can fail without saying so, as UDP can lose any packet.
        virtual void Send(const SocketAddress& to, std::span<const std::uint8_t> bytes) = 0;

        // The size of the packet put in `buffer`, or nothing when none is waiting.
        virtual std::optional<std::size_t> Receive(SocketAddress& from, std::span<std::uint8_t> buffer) = 0;

        // Returns when a packet arrives or the time is up.
        virtual void Wait(int milliseconds) = 0;

        virtual std::uint16_t Port() const = 0;
    };

    struct SocketSettings
    {
        // 0 takes any free port.
        std::uint16_t Port = 0;

        // Bound to the loopback address, so only this computer can reach it.
        bool ThisComputerOnly = false;

        bool Broadcast = false;
    };

    // Nothing, with `error` set, when the socket cannot be opened.
    std::unique_ptr<UdpSocket> OpenSocket(const SocketSettings& settings, std::string& error);

    // Looks up a name such as "localhost" or "192.168.1.20" as an IPv4 address.
    std::optional<SocketAddress> FindAddress(std::string_view name, std::uint16_t port, std::string& error);
}
