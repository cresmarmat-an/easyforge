#pragma once

// One end of a connection's reliability: sequence numbers, acknowledgements,
// resending, ordering, fragments, the send budget, and the round trip. A peer
// does not touch sockets; it fills packets and reads the ones handed to it.

#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <limits>
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

        // When the other end sent the packet that completed it, as NetworkTime:
        // when it would have arrived with no more delay along the way than the
        // least seen.
        double SentAt = 0.0;
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

        // Reads a data or keep-alive packet whose header up to the token has
        // been read, and adds the messages it completes. False when the packet
        // was damaged.
        bool Receive(double now, ByteReader& reader, bool keepAlive, std::vector<DeliveredMessage>& delivered);

        // True when enough packets arrived since the last one sent that the
        // other end should hear about them now.
        bool AckOverdue() const
        {
            return ReceivedSinceSent >= PacketsBeforeAck || (ReceivedSinceSent > 0 && GapSeen);
        }

        double LastReceived() const { return LastHeard; }

        // Counts the connection as heard from now, as when it is accepted.
        void Heard(double now) { LastHeard = now; }
        float RoundTrip() const { return static_cast<float>(SmoothedRoundTrip); }

        // When the other end sent the last packet read, on this end's clock.
        double LastPacketSentAt() const { return PacketSentAt; }

        // The other end's time on this end's clock, for a packet stamped with
        // `stamp` and received `now`.
        double StampedTime(double now, std::uint32_t stamp);

        // Messages queued or sent and not yet acknowledged.
        std::size_t Unacknowledged() const;

        // The bytes the connection may have on their way at once, as congestion
        // control has found the path can carry.
        double SendWindow() const { return CongestionWindow; }

        // Whether the other end has sent more unfinished messages than a
        // connection may hold, AssemblyLimit.
        bool OverBudget() const { return HeldBytes > AssemblyLimit; }

        // The bytes of data sent and not yet acknowledged or found lost.
        double BytesInFlight() const { return InFlight; }

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

            // Carried messages or was a keep-alive, so the other end answered
            // it at once and its acknowledgement measures the round trip.
            bool AskedForAnswer = false;

            double SentAt = 0.0;
            std::vector<std::pair<std::uint8_t, std::uint16_t>> Pieces;

            // Carried data and counts against the congestion window until it is
            // acknowledged or found lost.
            bool InFlight = false;
            std::uint16_t Size = 0;
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
        void FindLosses(double now);
        void Lose(double now, PacketRecord& record);
        Piece* FindPiece(std::uint8_t channel, std::uint16_t identifier);
        // Returns whether the packet came out of order: after a gap, or into one.
        bool NoteReceived(std::uint16_t sequence);
        void ReceiveOrdered(Piece piece, std::vector<DeliveredMessage>& delivered);
        void ReceiveUnordered(Piece piece, std::vector<DeliveredMessage>& delivered);
        void WriteHeader(std::vector<std::uint8_t>& packet, std::uint64_t token, std::uint16_t sequence, double now) const;

        // Sending.
        std::array<SendChannel, 2> Sending;
        std::vector<UnreliableEntry> UnreliableQueue;
        std::uint32_t NextUnreliable = 1;
        std::uint16_t NextSequence = 0;
        std::vector<PacketRecord> Records = std::vector<PacketRecord>(Window);
        double Budget = InitialWindow;
        double BudgetTime = 0.0;

        // Congestion control, after TCP's NewReno and QUIC: the window grows
        // while packets arrive, by a packet's size for each one until the first
        // loss and by about a packet each round trip after, and halves once for
        // each loss event, so the connection sends as fast as the path carries
        // and backs off when it is full.
        double CongestionWindow = InitialWindow;
        double SlowStartLimit = std::numeric_limits<double>::infinity();
        double InFlight = 0.0;
        // Losses of packets sent before this cut the window only once.
        double RecoveryStarted = -1.0;
        // Data packets in flight, oldest first.
        std::deque<std::uint16_t> Outstanding;
        std::uint16_t LargestAcknowledged = 0;
        bool AnyAcknowledged = false;
        double LatestRoundTrip = 0.0;
        double LastSent = -1.0;
        double LastAsked = -1.0;
        int ReceivedSinceSent = 0;

        // A packet with messages came out of order since the last
        // acknowledgement, so the sender should hear of the gap at once, as
        // TCP and QUIC do.
        bool GapSeen = false;

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

        // Bytes held for unordered messages not complete yet.
        std::size_t HeldBytes = 0;

        double SmoothedRoundTrip = 0.1;
        bool MeasuredRoundTrip = false;

        // This end's clock when the connection began, which stamps count from.
        double Began = 0.0;

        // The other end's clock: its newest stamp, and that stamp in seconds
        // without wrapping.
        bool StampSeen = false;
        std::uint32_t NewestStamp = 0;
        double NewestStampSeconds = 0.0;

        // The lowest difference between the two clocks in the current and the
        // previous window of ClockWindowSeconds. The packet that took the least
        // time along the way sets it, so later packets show how late they are.
        double LowestDifference[2] = { 0.0, 0.0 };
        bool DifferenceSeen[2] = { false, false };
        double WindowStarted = 0.0;
        double PacketSentAt = 0.0;
    };
}
