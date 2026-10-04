#pragma once

// The packet format both ends share.
//
// Every packet starts with "EFN1" and a kind. Data packets then carry the
// connection's token (the client's salt and the server's salt combined), the
// packet's sequence number, the newest sequence received from the other end,
// and 32 bits for the packets received before that one. After the header come
// message entries:
//
//     flags u8: channel in bits 0 and 1, bit 2 set for a fragment
//     identifier: u16 on the reliable channels, u32 on the unreliable one
//     fragment index u16 and count u16, for fragments
//     length u16, then the bytes
//
// Each message's bytes start with its kind (MessageKind): a message carries its
// name and body, a request an identifier, name, and body, and a reply the
// request's identifier and either a body or the error text.

#include <cstddef>
#include <cstdint>

namespace easyforge::internal::networking
{
    inline constexpr std::uint8_t Magic[4] = { 'E', 'F', 'N', '1' };

    enum class PacketKind : std::uint8_t
    {
        ConnectRequest = 1,
        ConnectAccept,
        ConnectRefuse,
        Data,
        Disconnect,
        Discover,
        DiscoverReply,
    };

    enum class Channel : std::uint8_t
    {
        Reliable = 0,
        Unreliable = 1,
        ReliableUnordered = 2,
    };

    enum class RefuseReason : std::uint8_t
    {
        Full = 1,
    };

    enum class DisconnectReason : std::uint8_t
    {
        // The program at the other end closed the connection.
        Closed = 0,

        // The server stopped.
        Stopped = 1,
    };

    enum class MessageKind : std::uint8_t
    {
        Message = 0,
        Request,
        Reply,
        ReplyError,
    };

    // The largest packet sent, which fits the smallest common path without
    // being split by the network.
    inline constexpr std::size_t PacketLimit = 1200;
    inline constexpr std::size_t DataHeaderSize = 21;

    // Messages larger than this go in fragments.
    inline constexpr std::size_t FragmentSize = 1000;
    inline constexpr std::size_t MessageLimit = 4u << 20;
    inline constexpr std::size_t FragmentLimit = (MessageLimit + FragmentSize - 1) / FragmentSize;

    // Reliable entries in flight on one channel, counted from the oldest not
    // yet acknowledged.
    inline constexpr std::uint16_t Window = 1024;

    inline constexpr double KeepAliveSeconds = 0.25;
    inline constexpr double ConnectRetrySeconds = 0.1;
    inline constexpr double AckDelaySeconds = 0.008;
    inline constexpr double MinimumResendSeconds = 0.05;
    inline constexpr int PacketsBeforeAck = 16;

    // Bytes a second sent on one connection, and how many may go at once.
    inline constexpr double SendRate = 1024.0 * 1024.0;
    inline constexpr double SendBurst = 64.0 * 1024.0;

    inline constexpr std::size_t UnreliableQueueLimit = 256;
}
