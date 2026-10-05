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

    Peer::Peer(double now) : BudgetTime(now), LastHeard(now), Began(now), WindowStarted(now)
    {
    }

    double Peer::StampedTime(double now, std::uint32_t stamp)
    {
        // Packets can arrive out of order, so the stamp is read against the
        // newest one, either way, and only a newer one moves it on.
        double seconds = static_cast<double>(stamp) / 1000.0;
        if (StampSeen)
        {
            auto ahead = static_cast<std::int32_t>(stamp - NewestStamp);
            seconds = NewestStampSeconds + static_cast<double>(ahead) / 1000.0;
            if (ahead > 0)
            {
                NewestStamp = stamp;
                NewestStampSeconds = seconds;
            }
        }
        else
        {
            StampSeen = true;
            NewestStamp = stamp;
            NewestStampSeconds = seconds;
        }

        if (now - WindowStarted >= ClockWindowSeconds)
        {
            LowestDifference[1] = LowestDifference[0];
            DifferenceSeen[1] = DifferenceSeen[0];
            DifferenceSeen[0] = false;
            WindowStarted = now;
        }
        double difference = now - seconds;
        if (!DifferenceSeen[0] || difference < LowestDifference[0])
        {
            LowestDifference[0] = difference;
            DifferenceSeen[0] = true;
        }
        double lowest = LowestDifference[0];
        if (DifferenceSeen[1])
        {
            lowest = std::min(lowest, LowestDifference[1]);
        }
        return seconds + lowest;
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

    void Peer::WriteHeader(std::vector<std::uint8_t>& packet, std::uint64_t token, std::uint16_t sequence, double now) const
    {
        ByteWriter writer(packet);
        writer.WriteBytes(Magic);
        writer.Write8(static_cast<std::uint8_t>(PacketKind::Data));
        writer.Write64(token);
        // Milliseconds wrap after 49 days; the other end reads them against the
        // newest it has, so wrapping does not matter.
        writer.Write32(static_cast<std::uint32_t>(static_cast<std::uint64_t>(std::max(now - Began, 0.0) * 1000.0)));
        writer.Write16(sequence);

        // Before anything has arrived, the packet "before 0" is named, which the
        // other end has no record of.
        writer.Write16(ReceivedAny ? NewestReceived : std::uint16_t { 65535 });
        writer.Write32(ReceivedAny ? ReceivedBits : 0);
    }

    void Peer::Flush(double now, std::uint64_t token, std::vector<std::vector<std::uint8_t>>& packets)
    {
        FindLosses(now);
        // Pacing: the window's worth of bytes is spread over a round trip.
        double rate = PacingGain * CongestionWindow / std::max(SmoothedRoundTrip, 0.001);
        Budget = std::min(std::max(CongestionWindow, InitialWindow), Budget + (now - BudgetTime) * rate);
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
        // Counted from the last packet that asked for an answer, so even an end
        // that only acknowledges keeps measuring the round trip.
        bool keepAliveDue = LastAsked < 0.0 || now - LastAsked >= KeepAliveSeconds;
        std::size_t nextUnreliable = 0;
        std::size_t nextDue = 0;
        while (true)
        {
            bool roomToSend = Budget > 0.0 && InFlight < CongestionWindow && Outstanding.size() < MostPacketsInFlight;
            bool dataWaiting = roomToSend && (nextUnreliable < UnreliableQueue.size() || nextDue < due.size());
            if (!dataWaiting && !ackDue && !keepAliveDue)
            {
                break;
            }

            std::vector<std::uint8_t> packet;
            packet.reserve(PacketLimit);
            std::uint16_t sequence = NextSequence++;
            WriteHeader(packet, token, sequence, now);
            PacketRecord& record = Records[sequence % Window];
            if (record.Used && record.InFlight)
            {
                // Never reached while packets in flight stay under the limit, but
                // a record reused must not stay counted.
                InFlight -= record.Size;
            }
            record.Sequence = sequence;
            record.Used = true;
            record.Acknowledged = false;
            record.AskedForAnswer = false;
            record.InFlight = false;
            record.Size = 0;
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

            // Empty packets only acknowledge, and are not acknowledged in turn,
            // unless the connection was quiet long enough to need a keep-alive.
            if (packet.size() > DataHeaderSize)
            {
                record.AskedForAnswer = true;
            }
            else if (keepAliveDue)
            {
                packet[sizeof Magic] = static_cast<std::uint8_t>(PacketKind::KeepAlive);
                record.AskedForAnswer = true;
            }
            if (record.AskedForAnswer)
            {
                LastAsked = now;
            }
            if (packet.size() > DataHeaderSize)
            {
                record.InFlight = true;
                record.Size = static_cast<std::uint16_t>(packet.size());
                InFlight += static_cast<double>(packet.size());
                Outstanding.push_back(sequence);
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
        if (!AnyAcknowledged || (Ahead(LargestAcknowledged, sequence) != 0 && Ahead(LargestAcknowledged, sequence) < 32768))
        {
            LargestAcknowledged = sequence;
            AnyAcknowledged = true;
        }
        if (record.InFlight)
        {
            record.InFlight = false;
            double size = record.Size;
            // The window grows only while it was in use, and not for packets
            // sent before the last cut.
            if (record.SentAt > RecoveryStarted && InFlight + size >= CongestionWindow * 0.5)
            {
                CongestionWindow += CongestionWindow < SlowStartLimit ? size : PacketLimit * size / CongestionWindow;
                CongestionWindow = std::min(CongestionWindow, MaximumWindow);
            }
            InFlight -= size;
        }
        if (newest && record.AskedForAnswer)
        {
            double sample = now - record.SentAt;
            LatestRoundTrip = sample;
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
            if (Piece* piece = FindPiece(channel, identifier))
            {
                piece->Acknowledged = true;
            }
        }
    }

    Peer::Piece* Peer::FindPiece(std::uint8_t channel, std::uint16_t identifier)
    {
        std::deque<Piece>& pieces = Sending[ChannelIndex(static_cast<Channel>(channel))].Pieces;
        if (pieces.empty())
        {
            return nullptr;
        }
        std::uint16_t offset = Ahead(pieces.front().Identifier, identifier);
        if (offset < pieces.size() && offset < Window && pieces[offset].Identifier == identifier)
        {
            return &pieces[offset];
        }
        return nullptr;
    }

    void Peer::FindLosses(double now)
    {
        double roundTrip = std::max(SmoothedRoundTrip, LatestRoundTrip);
        double late = LossTimeShare * roundTrip + AckDelaySeconds;
        // With nothing after it acknowledged either, a packet is given up on
        // after twice as long, as when every packet of a burst was lost.
        double gone = std::max(2.0 * late, 2.0 * MinimumResendSeconds);
        while (!Outstanding.empty())
        {
            std::uint16_t sequence = Outstanding.front();
            PacketRecord& record = Records[sequence % Window];
            if (record.Sequence != sequence || !record.InFlight)
            {
                Outstanding.pop_front();
                continue;
            }
            std::uint16_t behind = AnyAcknowledged ? Ahead(sequence, LargestAcknowledged) : 0;
            bool laterArrived = behind != 0 && behind < 32768;
            double waited = now - record.SentAt;
            bool lost = (laterArrived && (behind >= LossPacketThreshold || waited >= late)) || waited >= gone;
            if (!lost)
            {
                // Packets go out in order, so the ones after this waited less.
                break;
            }
            Lose(now, record);
            Outstanding.pop_front();
        }
    }

    void Peer::Lose(double now, PacketRecord& record)
    {
        record.InFlight = false;
        InFlight -= record.Size;
        // Its pieces go again at once, unless a later packet carried them.
        for (auto [channel, identifier] : record.Pieces)
        {
            Piece* piece = FindPiece(channel, identifier);
            if (piece && !piece->Acknowledged && piece->LastSent <= record.SentAt)
            {
                piece->LastSent = -1.0;
            }
        }
        if (record.SentAt > RecoveryStarted)
        {
            RecoveryStarted = now;
            CongestionWindow = std::max(CongestionWindow * 0.5, MinimumWindow);
            SlowStartLimit = CongestionWindow;
        }
    }

    bool Peer::Receive(double now, ByteReader& reader, bool keepAlive, std::vector<DeliveredMessage>& delivered)
    {
        std::uint32_t stamp = reader.Read32();
        std::uint16_t sequence = reader.Read16();
        std::uint16_t newest = reader.Read16();
        std::uint32_t bits = reader.Read32();
        if (reader.Failed())
        {
            return false;
        }
        PacketSentAt = StampedTime(now, stamp);

        LastHeard = now;
        NoteReceived(sequence);
        if (keepAlive || !reader.IsAtEnd())
        {
            ++ReceivedSinceSent;
        }

        Acknowledge(now, newest, true);
        for (int bit = 0; bit < 32; ++bit)
        {
            if (bits & (1u << bit))
            {
                Acknowledge(now, static_cast<std::uint16_t>(newest - 1 - bit), false);
            }
        }
        FindLosses(now);
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
