#pragma once

// One end of a connection's reliability: sequence numbers, acknowledgements,
// resending, ordering, fragments, the send budget, and the round trip. A peer
// does not touch sockets; it fills packets and reads the ones handed to it.

#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "Encoding.h"
#include "Protocol.h"

namespace easyforge::internal::networking
{
    struct DeliveredMessage
    {
        Channel Kind = Channel::Reliable;

        // Counts up with every unreliable message, so the newest can win.
        std::uint32_t Sequence = 0;

        std::vector<std::uint8_t> Bytes;
    };

    class Peer
    {
    public:
        explicit Peer(double now);

        // Queues a message. A message larger than a fragment is split; an
        // unreliable one that large goes on the unordered reliable channel.
        void Queue(Channel channel, std::vector<std::uint8_t> bytes);

        // Adds the packets due now to `packets`: waiting messages, resends, an
        // acknowledgement, or a keep-alive.
        void Flush(double now, std::uint64_t token, std::vector<std::vector<std::uint8_t>>& packets);

        // Reads a data packet whose header up to the token has been read, and
        // adds the messages it completes. False when the packet was damaged.
        bool Receive(double now, ByteReader& reader, std::vector<DeliveredMessage>& delivered);

        // True when enough packets arrived since the last one sent that the
        // other end should hear about them now.
        bool AckOverdue() const { return ReceivedSinceSent >= PacketsBeforeAck; }

        double LastReceived() const { return LastHeard; }

        // Counts the connection as heard from now, as when it is accepted.
        void Heard(double now) { LastHeard = now; }
        float RoundTrip() const { return static_cast<float>(SmoothedRoundTrip); }

        // Messages queued or sent and not yet acknowledged.
        std::size_t Unacknowledged() const;

    private:
        struct Piece
        {
            std::uint16_t Identifier = 0;
            std::uint16_t Index = 0;

            // 0 for a whole message.
            std::uint16_t Count = 0;

            std::vector<std::uint8_t> Bytes;
            double LastSent = -1.0;
            bool Acknowledged = false;
        };

        struct SendChannel
        {
            // Not yet acknowledged, oldest first, identifiers in a row.
            std::deque<Piece> Pieces;
            std::uint16_t NextIdentifier = 0;
        };

        struct UnreliableEntry
        {
            std::uint32_t Sequence = 0;
            std::vector<std::uint8_t> Bytes;
        };

        struct PacketRecord
        {
            std::uint16_t Sequence = 0;
            bool Used = false;
            bool Acknowledged = false;
            double SentAt = 0.0;
            std::vector<std::pair<std::uint8_t, std::uint16_t>> Pieces;
        };

        struct Assembly
        {
            std::uint16_t Count = 0;
            std::uint16_t Arrived = 0;
            std::vector<std::vector<std::uint8_t>> Parts;
        };

        static int ChannelIndex(Channel channel) { return channel == Channel::Reliable ? 0 : 1; }

        void QueueReliable(Channel channel, std::vector<std::uint8_t> bytes);
        void Acknowledge(double now, std::uint16_t sequence, bool newest);
        void NoteReceived(std::uint16_t sequence);
        void ReceiveOrdered(Piece piece, std::vector<DeliveredMessage>& delivered);
        void ReceiveUnordered(Piece piece, std::vector<DeliveredMessage>& delivered);
        void WriteHeader(std::vector<std::uint8_t>& packet, std::uint64_t token, std::uint16_t sequence) const;

        // Sending.
        std::array<SendChannel, 2> Sending;
        std::vector<UnreliableEntry> UnreliableQueue;
        std::uint32_t NextUnreliable = 1;
        std::uint16_t NextSequence = 0;
        std::vector<PacketRecord> Records = std::vector<PacketRecord>(Window);
        double Budget = SendBurst;
        double BudgetTime = 0.0;
        double LastSent = -1.0;
        int ReceivedSinceSent = 0;

        // Receiving.
        std::uint16_t NewestReceived = 0;
        std::uint32_t ReceivedBits = 0;
        bool ReceivedAny = false;
        double LastHeard = 0.0;

        std::uint16_t NextOrdered = 0;
        std::vector<std::optional<Piece>> Early = std::vector<std::optional<Piece>>(Window);
        std::vector<std::uint8_t> OrderedAssembly;
        std::uint16_t OrderedAssemblyCount = 0;
        std::uint16_t OrderedAssemblyNext = 0;

        std::uint16_t LowestUnordered = 0;
        std::bitset<Window> UnorderedArrived;
        std::map<std::uint16_t, Assembly> UnorderedAssemblies;

        double SmoothedRoundTrip = 0.1;
        bool MeasuredRoundTrip = false;
    };
}
