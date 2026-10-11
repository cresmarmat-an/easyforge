#pragma once

// The packet format both ends share.
//
// Every packet starts with "EFN3" and a kind.
//
// Connecting: the client sends ConnectRequest, padded to ConnectRequestSize, so
// the server never answers with more than it was sent. The server answers with
// ConnectChallenge, holding a cookie it can check later without keeping
// anything: when it runs out, and an HMAC of that and the client's address.
// The client sends ConnectResponse with the cookie and its ECDH public key; the
// server checks the cookie, agrees on a secret, and sends ConnectAccept with
// its own public key, signed when it has an identity. Both ends derive one
// AES-256-GCM key for each direction from the secret with HKDF.
//
// Data, KeepAlive, and Disconnect packets carry the connection's token (the
// client's salt and the server's salt combined) and a 64-bit counter, which
// count from 1 in each direction and never repeat; the rest is sealed with the
// direction's key, the counter as the nonce, and the token and counter as
// associated data, followed by the tag. A packet that fails to open, or whose
// counter was seen already or is too far behind the newest, is dropped.
//
// Sealed, a data packet holds the sender's clock in milliseconds since its
// connection began (wrapping after 49 days), the packet's sequence number, the
// newest sequence received from the other end, and 32 bits for the packets
// received before that one. After that come message entries:
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
    inline constexpr std::uint8_t Magic[4] = { 'E', 'F', 'N', '3' };

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

        // The server's answer to ConnectRequest, and the client's answer to it.
        ConnectChallenge,
        ConnectResponse,
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

    // What sealing adds to a data packet: its counter and the tag.
    inline constexpr std::size_t SealOverhead = 8 + 16;

    // The most a data packet holds before it is sealed.
    inline constexpr std::size_t PlainPacketLimit = PacketLimit - SealOverhead;

    // A data packet's header before sealing: the magic and kind, the token, the
    // clock, the sequence, and the acknowledgements.
    inline constexpr std::size_t DataHeaderSize = 25;

    // The part of a sealed packet that stays readable: magic, kind, token, and
    // counter. It is authenticated along with what is sealed.
    inline constexpr std::size_t SealedHeaderSize = 4 + 1 + 8 + 8;

    // Connect requests and requests for servers on the local network are at
    // least this large, more than any answer to them, so a forged sender
    // address cannot turn a server into an amplifier.
    inline constexpr std::size_t ConnectRequestSize = PacketLimit;
    inline constexpr std::size_t DiscoverRequestSize = 320;

    // A cookie: when it runs out, in milliseconds of the server's clock, then
    // the HMAC that proves the server made it for this address.
    inline constexpr std::size_t CookieSize = 8 + 16;
    inline constexpr double CookieSeconds = 10.0;

    // Connect attempts each address may make a second, and how many at once,
    // since agreeing on keys costs the server time.
    inline constexpr double ConnectAttemptsPerSecond = 10.0;
    inline constexpr double ConnectAttemptBurst = 20.0;
    inline constexpr std::size_t MostAddressesTracked = 4096;

    // Packet counters accepted behind the newest one seen.
    inline constexpr std::size_t ReplayWindowSize = 1024;

    // The most memory one connection may hold in messages arriving in pieces
    // out of order, unless the settings say otherwise. A connection that sends
    // more is closed.
    inline constexpr std::size_t AssemblyLimit = 16u << 20;

    // How long the lowest clock difference seen is remembered. Clocks of two
    // computers drift apart slowly and routes change, so old lows are let go.
    inline constexpr double ClockWindowSeconds = 4.0;

    // Messages larger than this go in fragments.
    inline constexpr std::size_t FragmentSize = 1000;

    // The largest message unless the settings say otherwise, and the range the
    // settings may choose from: a message's fragments are counted in 16 bits.
    inline constexpr std::size_t MessageLimit = 4u << 20;
    inline constexpr std::size_t SmallestMessageLimit = 1u << 10;
    inline constexpr std::size_t LargestMessageLimit = 60u << 20;

    constexpr std::size_t FragmentsFor(std::size_t bytes)
    {
        return (bytes + FragmentSize - 1) / FragmentSize;
    }

    inline constexpr std::size_t FragmentLimit = FragmentsFor(MessageLimit);
    static_assert(FragmentsFor(LargestMessageLimit) <= 65535);

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
