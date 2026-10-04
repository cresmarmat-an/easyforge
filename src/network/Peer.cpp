#include "Peer.h"

#include <algorithm>
#include <span>

namespace easyforge::internal::networking
{
    namespace
    {
        // How far `to` is ahead of `from`, counting around the wrap.
        std::uint16_t Ahead(std::uint16_t from, std::uint16_t to)
        {
            return static_cast<std::uint16_t>(to - from);
        }

        std::size_t EntrySize(bool unreliable, bool fragment, std::size_t bytes)
        {
            return 1 + (unreliable ? 4 : 2) + (fragment ? 4 : 0) + 2 + bytes;
        }
    }

    Peer::Peer(double now) : BudgetTime(now), LastHeard(now)
    {
    }

    void Peer::Queue(Channel channel, std::vector<std::uint8_t> bytes)
    {
        if (channel == Channel::Unreliable)
        {
            if (bytes.size() <= FragmentSize)
            {
                if (UnreliableQueue.size() >= UnreliableQueueLimit)
                {
                    UnreliableQueue.erase(UnreliableQueue.begin());
                }
                UnreliableQueue.push_back({ NextUnreliable++, std::move(bytes) });
                return;
            }
            channel = Channel::ReliableUnordered;
        }
        QueueReliable(channel, std::move(bytes));
    }

    void Peer::QueueReliable(Channel channel, std::vector<std::uint8_t> bytes)
    {
        SendChannel& sending = Sending[ChannelIndex(channel)];
        if (bytes.size() <= FragmentSize)
        {
            sending.Pieces.push_back({ .Identifier = sending.NextIdentifier++, .Bytes = std::move(bytes) });
            return;
        }
        auto count = static_cast<std::uint16_t>((bytes.size() + FragmentSize - 1) / FragmentSize);
        for (std::uint16_t index = 0; index < count; ++index)
        {
            std::size_t start = index * FragmentSize;
            std::size_t end = std::min(bytes.size(), start + FragmentSize);
            sending.Pieces.push_back({
                .Identifier = sending.NextIdentifier++,
                .Index = index,
                .Count = count,
                .Bytes = std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                    bytes.begin() + static_cast<std::ptrdiff_t>(end)),
            });
        }
    }

    std::size_t Peer::Unacknowledged() const
    {
        std::size_t count = 0;
        for (const SendChannel& sending : Sending)
        {
            count += static_cast<std::size_t>(
                std::ranges::count_if(sending.Pieces, [](const Piece& piece) { return !piece.Acknowledged; }));
        }
        return count;
    }

    void Peer::WriteHeader(std::vector<std::uint8_t>& packet, std::uint64_t token, std::uint16_t sequence) const
    {
        ByteWriter writer(packet);
        writer.WriteBytes(Magic);
        writer.Write8(static_cast<std::uint8_t>(PacketKind::Data));
        writer.Write64(token);
        writer.Write16(sequence);

        // Before anything has arrived, the packet "before 0" is named, which the
        // other end has no record of.
        writer.Write16(ReceivedAny ? NewestReceived : std::uint16_t { 65535 });
        writer.Write32(ReceivedAny ? ReceivedBits : 0);
    }

    void Peer::Flush(double now, std::uint64_t token, std::vector<std::vector<std::uint8_t>>& packets)
    {
        Budget = std::min(SendBurst, Budget + (now - BudgetTime) * SendRate);
        BudgetTime = now;

        // Pieces never sent, or not acknowledged in time. Only the first Window
        // identifiers of a channel go out, so the other end can tell new from old.
        double resendAfter = std::max(1.5 * SmoothedRoundTrip, MinimumResendSeconds);
        std::vector<std::pair<Channel, Piece*>> due;
        for (int index = 0; index < 2; ++index)
        {
            Channel channel = index == 0 ? Channel::Reliable : Channel::ReliableUnordered;
            std::deque<Piece>& pieces = Sending[index].Pieces;
            std::size_t limit = std::min<std::size_t>(pieces.size(), Window);
            for (std::size_t position = 0; position < limit; ++position)
            {
                Piece& piece = pieces[position];
                if (!piece.Acknowledged && (piece.LastSent < 0.0 || now - piece.LastSent >= resendAfter))
                {
                    due.emplace_back(channel, &piece);
                }
            }
        }

        bool ackDue = (ReceivedSinceSent > 0 && now - LastSent >= AckDelaySeconds) || AckOverdue();
        bool keepAliveDue = LastSent < 0.0 || now - LastSent >= KeepAliveSeconds;
        std::size_t nextUnreliable = 0;
        std::size_t nextDue = 0;
        while (true)
        {
            bool dataWaiting = Budget > 0.0 && (nextUnreliable < UnreliableQueue.size() || nextDue < due.size());
            if (!dataWaiting && !ackDue && !keepAliveDue)
            {
                break;
            }

            std::vector<std::uint8_t> packet;
            packet.reserve(PacketLimit);
            std::uint16_t sequence = NextSequence++;
            WriteHeader(packet, token, sequence);
            PacketRecord& record = Records[sequence % Window];
            record.Sequence = sequence;
            record.Used = true;
            record.Acknowledged = false;
            record.SentAt = now;
            record.Pieces.clear();

            ByteWriter writer(packet);
            if (dataWaiting)
            {
                // Unreliable messages first: they are small and only useful now.
                while (nextUnreliable < UnreliableQueue.size())
                {
                    const UnreliableEntry& entry = UnreliableQueue[nextUnreliable];
                    if (packet.size() + EntrySize(true, false, entry.Bytes.size()) > PacketLimit)
                    {
                        break;
                    }
                    writer.Write8(static_cast<std::uint8_t>(Channel::Unreliable));
                    writer.Write32(entry.Sequence);
                    writer.Write16(static_cast<std::uint16_t>(entry.Bytes.size()));
                    writer.WriteBytes(entry.Bytes);
                    ++nextUnreliable;
                }
                while (nextUnreliable == UnreliableQueue.size() && nextDue < due.size())
                {
                    auto [channel, piece] = due[nextDue];
                    bool fragment = piece->Count != 0;
                    if (packet.size() + EntrySize(false, fragment, piece->Bytes.size()) > PacketLimit)
                    {
                        break;
                    }
                    writer.Write8(static_cast<std::uint8_t>(static_cast<std::uint8_t>(channel) | (fragment ? 4 : 0)));
                    writer.Write16(piece->Identifier);
                    if (fragment)
                    {
                        writer.Write16(piece->Index);
                        writer.Write16(piece->Count);
                    }
                    writer.Write16(static_cast<std::uint16_t>(piece->Bytes.size()));
                    writer.WriteBytes(piece->Bytes);
                    piece->LastSent = now;
                    record.Pieces.emplace_back(static_cast<std::uint8_t>(channel), piece->Identifier);
                    ++nextDue;
                }
            }

            Budget -= static_cast<double>(packet.size());
            packets.push_back(std::move(packet));
            LastSent = now;
            ReceivedSinceSent = 0;
            ackDue = false;
            keepAliveDue = false;
        }

        // What did not fit in the budget is dropped, as unreliable messages may be.
        UnreliableQueue.clear();
    }

    void Peer::NoteReceived(std::uint16_t sequence)
    {
        if (!ReceivedAny)
        {
            ReceivedAny = true;
            NewestReceived = sequence;
            ReceivedBits = 0;
            return;
        }
        std::uint16_t ahead = Ahead(NewestReceived, sequence);
        if (ahead == 0)
        {
            return;
        }
        if (ahead < 32768)
        {
            // The previous newest moves into the bits, `ahead` places back.
            if (ahead < 32)
            {
                ReceivedBits = (ReceivedBits << ahead) | (1u << (ahead - 1));
            }
            else
            {
                ReceivedBits = ahead == 32 ? 1u << 31 : 0;
            }
            NewestReceived = sequence;
            return;
        }
        std::uint16_t behind = Ahead(sequence, NewestReceived);
        if (behind <= 32)
        {
            ReceivedBits |= 1u << (behind - 1);
        }
    }

    void Peer::Acknowledge(double now, std::uint16_t sequence, bool newest)
    {
        PacketRecord& record = Records[sequence % Window];
        if (!record.Used || record.Acknowledged || record.Sequence != sequence)
        {
            return;
        }
        record.Acknowledged = true;
        if (newest)
        {
            double sample = now - record.SentAt;
            if (!MeasuredRoundTrip)
            {
                SmoothedRoundTrip = sample;
                MeasuredRoundTrip = true;
            }
            else
            {
                SmoothedRoundTrip += 0.125 * (sample - SmoothedRoundTrip);
            }
        }
        for (auto [channel, identifier] : record.Pieces)
        {
            std::deque<Piece>& pieces = Sending[ChannelIndex(static_cast<Channel>(channel))].Pieces;
            if (pieces.empty())
            {
                continue;
            }
            std::uint16_t offset = Ahead(pieces.front().Identifier, identifier);
            if (offset < pieces.size() && offset < Window && pieces[offset].Identifier == identifier)
            {
                pieces[offset].Acknowledged = true;
            }
        }
    }

    bool Peer::Receive(double now, ByteReader& reader, std::vector<DeliveredMessage>& delivered)
    {
        std::uint16_t sequence = reader.Read16();
        std::uint16_t newest = reader.Read16();
        std::uint32_t bits = reader.Read32();
        if (reader.Failed())
        {
            return false;
        }

        LastHeard = now;
        NoteReceived(sequence);
        ++ReceivedSinceSent;

        Acknowledge(now, newest, true);
        for (int bit = 0; bit < 32; ++bit)
        {
            if (bits & (1u << bit))
            {
                Acknowledge(now, static_cast<std::uint16_t>(newest - 1 - bit), false);
            }
        }
        for (SendChannel& sending : Sending)
        {
            while (!sending.Pieces.empty() && sending.Pieces.front().Acknowledged)
            {
                sending.Pieces.pop_front();
            }
        }

        while (!reader.IsAtEnd())
        {
            std::uint8_t flags = reader.Read8();
            auto channel = static_cast<Channel>(flags & 3);
            bool fragment = (flags & 4) != 0;
            if (channel == Channel::Unreliable)
            {
                std::uint32_t unreliableSequence = reader.Read32();
                std::span<const std::uint8_t> bytes = reader.ReadBytes(reader.Read16());
                if (reader.Failed())
                {
                    return false;
                }
                delivered.push_back({ Channel::Unreliable, unreliableSequence, { bytes.begin(), bytes.end() } });
                continue;
            }
            if (channel != Channel::Reliable && channel != Channel::ReliableUnordered)
            {
                return false;
            }

            Piece piece;
            piece.Identifier = reader.Read16();
            if (fragment)
            {
                piece.Index = reader.Read16();
                piece.Count = reader.Read16();
            }
            std::span<const std::uint8_t> bytes = reader.ReadBytes(reader.Read16());
            if (reader.Failed())
            {
                return false;
            }
            if (fragment && (piece.Count < 2 || piece.Count > FragmentLimit || piece.Index >= piece.Count || bytes.empty()))
            {
                return false;
            }
            piece.Bytes.assign(bytes.begin(), bytes.end());
            if (channel == Channel::Reliable)
            {
                ReceiveOrdered(std::move(piece), delivered);
            }
            else
            {
                ReceiveUnordered(std::move(piece), delivered);
            }
        }
        return true;
    }

    void Peer::ReceiveOrdered(Piece piece, std::vector<DeliveredMessage>& delivered)
    {
        // Behind the next expected is a repeat; as far ahead as a window cannot
        // have been sent yet.
        if (Ahead(NextOrdered, piece.Identifier) >= Window)
        {
            return;
        }
        std::optional<Piece>& slot = Early[piece.Identifier % Window];
        if (slot)
        {
            return;
        }
        slot = std::move(piece);

        while (Early[NextOrdered % Window])
        {
            Piece next = std::move(*Early[NextOrdered % Window]);
            Early[NextOrdered % Window].reset();
            ++NextOrdered;
            if (next.Count == 0)
            {
                delivered.push_back({ Channel::Reliable, 0, std::move(next.Bytes) });
                continue;
            }
            if (next.Index == 0)
            {
                OrderedAssembly.clear();
                OrderedAssemblyCount = next.Count;
                OrderedAssemblyNext = 0;
            }
            if (next.Count != OrderedAssemblyCount || next.Index != OrderedAssemblyNext)
            {
                OrderedAssembly.clear();
                OrderedAssemblyCount = 0;
                continue;
            }
            OrderedAssembly.insert(OrderedAssembly.end(), next.Bytes.begin(), next.Bytes.end());
            if (++OrderedAssemblyNext == OrderedAssemblyCount)
            {
                delivered.push_back({ Channel::Reliable, 0, std::move(OrderedAssembly) });
                OrderedAssembly = {};
                OrderedAssemblyCount = 0;
            }
        }
    }

    void Peer::ReceiveUnordered(Piece piece, std::vector<DeliveredMessage>& delivered)
    {
        if (Ahead(LowestUnordered, piece.Identifier) >= Window || UnorderedArrived[piece.Identifier % Window])
        {
            return;
        }
        UnorderedArrived[piece.Identifier % Window] = true;
        while (UnorderedArrived[LowestUnordered % Window])
        {
            UnorderedArrived[LowestUnordered % Window] = false;
            ++LowestUnordered;
        }

        if (piece.Count == 0)
        {
            delivered.push_back({ Channel::ReliableUnordered, 0, std::move(piece.Bytes) });
            return;
        }

        // Fragments of one message have identifiers in a row, so the first one's
        // names the group.
        auto group = static_cast<std::uint16_t>(piece.Identifier - piece.Index);
        Assembly& assembly = UnorderedAssemblies[group];
        if (assembly.Count == 0)
        {
            assembly.Count = piece.Count;
            assembly.Parts.resize(piece.Count);
        }
        if (assembly.Count != piece.Count || !assembly.Parts[piece.Index].empty())
        {
            return;
        }
        assembly.Parts[piece.Index] = std::move(piece.Bytes);
        if (++assembly.Arrived == assembly.Count)
        {
            std::vector<std::uint8_t> whole;
            for (const std::vector<std::uint8_t>& part : assembly.Parts)
            {
                whole.insert(whole.end(), part.begin(), part.end());
            }
            UnorderedAssemblies.erase(group);
            delivered.push_back({ Channel::ReliableUnordered, 0, std::move(whole) });
        }
    }
}
