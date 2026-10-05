#pragma once

// The packet format both ends share.
//
// Every packet starts with "EFN2" and a kind. Data packets then carry the
// connection's token (the client's salt and the server's salt combined), the
// sender's clock in milliseconds since its connection began (wrapping after 49
// days), the packet's sequence number, the newest sequence received from the
// other end, and 32 bits for the packets received before that one. After the
// header come message entries:
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
    // The wire format's own revision, which changes when packets change shape.
    // It is not the library's version.
    inline constexpr std::uint8_t Magic[4] = { 'E', 'F', 'N', '2' };

    enum class PacketKind : std::uint8_t
    {
        ConnectRequest = 1,
        ConnectAccept,
        ConnectRefuse,
        Data,
        Disconnect,
        Discover,
        DiscoverReply,

        // A data packet sent only because the connection was quiet. Like a
        // packet with messages, and unlike an empty one, it asks for an
        // acknowledgement, which keeps the round trip measured.
        KeepAlive,
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
    inline constexpr std::size_t DataHeaderSize = 25;

    // How long the lowest clock difference seen is remembered. Clocks of two
    // computers drift apart slowly and routes change, so old lows are let go.
    inline constexpr double ClockWindowSeconds = 4.0;

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
    // Packets with messages received before an acknowledgement goes back at
    // once, as with TCP and QUIC. More would make a small window wait out the
    // acknowledgement delay every round trip.
    inline constexpr int PacketsBeforeAck = 2;

    // Congestion control: the bytes that may be on their way at once start at
    // ten packets, as with TCP and QUIC, and stay between two packets and as
    // many as the packet records can follow.
    inline constexpr double InitialWindow = 10.0 * PacketLimit;
    inline constexpr double MinimumWindow = 2.0 * PacketLimit;
    inline constexpr double MaximumWindow = (Window / 2) * static_cast<double>(PacketLimit);
    inline constexpr std::size_t MostPacketsInFlight = Window / 2;

    // Packets go out at this many windows per round trip, spread over it
    // rather than all at once, with room for the window to grow.
    inline constexpr double PacingGain = 1.25;

    // A packet counts as lost once this many sent after it have arrived, or
    // once it is late by a share of the round trip after a later one arrived.
    inline constexpr int LossPacketThreshold = 3;
    inline constexpr double LossTimeShare = 1.125;

    inline constexpr std::size_t UnreliableQueueLimit = 256;
}
