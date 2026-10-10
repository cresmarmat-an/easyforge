#include "Endpoint.h"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>

#include <easyforge/core/Log.h>
#include <easyforge/network/Identity.h>

namespace easyforge::internal::networking
{
    namespace
    {
        Channel ChannelFor(Delivery delivery)
        {
            switch (delivery)
            {
            case Delivery::Unreliable:
                return Channel::Unreliable;
            case Delivery::ReliableUnordered:
                return Channel::ReliableUnordered;
            case Delivery::Reliable:
                break;
            }
            return Channel::Reliable;
        }

        std::vector<std::uint8_t> StartPacket(PacketKind kind)
        {
            std::vector<std::uint8_t> packet;
            ByteWriter writer(packet);
            writer.WriteBytes(Magic);
            writer.Write8(static_cast<std::uint8_t>(kind));
            return packet;
        }

        std::string TooLarge(std::string_view what, std::string_view name)
        {
            return std::format("the {} \"{}\" is larger than {} MB", what, name, MessageLimit >> 20);
        }

        std::uint64_t Milliseconds(double seconds)
        {
            return static_cast<std::uint64_t>(std::max(seconds, 0.0) * 1000.0);
        }

        void WriteKey(ByteWriter& writer, std::span<const std::uint8_t> key)
        {
            writer.WriteBytes(key);
        }

        template <std::size_t Size>
        bool ReadKey(ByteReader& reader, std::array<std::uint8_t, Size>& key)
        {
            std::span<const std::uint8_t> bytes = reader.ReadBytes(Size);
            if (reader.Failed())
            {
                return false;
            }
            std::copy(bytes.begin(), bytes.end(), key.begin());
            return true;
        }
    }

    bool ReplayWindow::IsFresh(std::uint64_t counter) const
    {
        if (counter == 0)
        {
            return false;
        }
        if (counter > Newest)
        {
            return true;
        }
        std::uint64_t behind = Newest - counter;
        return behind < ReplayWindowSize && !Seen.test(static_cast<std::size_t>(behind));
    }

    void ReplayWindow::Take(std::uint64_t counter)
    {
        if (counter > Newest)
        {
            std::uint64_t ahead = counter - Newest;
            if (ahead >= ReplayWindowSize)
            {
                Seen.reset();
            }
            else
            {
                Seen <<= static_cast<std::size_t>(ahead);
            }
            Seen.set(0);
            Newest = counter;
            return;
        }
        Seen.set(static_cast<std::size_t>(Newest - counter));
    }

    Endpoint::Endpoint(Role role) : Kind(role)
    {
    }

    std::shared_ptr<Endpoint> Endpoint::StartServer(const ServerSettings& settings)
    {
        std::shared_ptr<Endpoint> endpoint(new Endpoint(Role::Server));
        endpoint->Self = endpoint;
        endpoint->Name = settings.Name;
        endpoint->Timeout = std::max(settings.Timeout, 0.1f);
        endpoint->Conditions = settings.Conditions;
        endpoint->Threaded = settings.Threaded;
        endpoint->Instance = endpoint->NewSalt();
        endpoint->Slots.resize(static_cast<std::size_t>(std::clamp(settings.MaximumClients, 1, 4096)));
        endpoint->Identity = settings.Identity.Key();
        if (!RandomBytes(endpoint->CookieSecret))
        {
            endpoint->Problem = "this system has no cryptography for secure connections";
            return endpoint;
        }

        std::string error;
        endpoint->Socket = OpenSocket({ .Port = settings.Port, .ThisComputerOnly = settings.ThisComputerOnly }, error);
        if (!endpoint->Socket)
        {
            endpoint->Problem = error;
            return endpoint;
        }
        endpoint->BoundPort = endpoint->Socket->Port();
        endpoint->Running = true;
        if (endpoint->Threaded)
        {
            endpoint->Worker = std::thread(&Endpoint::Work, std::weak_ptr<Endpoint>(endpoint));
        }
        return endpoint;
    }

    std::shared_ptr<Endpoint> Endpoint::StartClient(const ClientSettings& settings)
    {
        std::shared_ptr<Endpoint> endpoint(new Endpoint(Role::Client));
        endpoint->Self = endpoint;
        endpoint->Timeout = std::max(settings.Timeout, 0.1f);
        endpoint->ConnectTimeout = std::max(settings.ConnectTimeout, 0.1f);
        endpoint->Conditions = settings.Conditions;
        endpoint->Threaded = settings.Threaded;
        endpoint->Slots.resize(1);
        if (!settings.ServerKey.empty())
        {
            if (!PublicKeyFromText(settings.ServerKey, endpoint->PinnedServer))
            {
                endpoint->Problem = "ServerKey is not a server's public key: it should be the 128 digits PublicKey gave";
                return endpoint;
            }
            endpoint->PinsServer = true;
        }

        std::string error;
        std::optional<SocketAddress> address = FindAddress(settings.Address, settings.Port, error);
        if (!address)
        {
            endpoint->Problem = error;
            return endpoint;
        }

        // A server on this computer is reached without opening the socket to the network.
        endpoint->Socket = OpenSocket({ .Port = 0, .ThisComputerOnly = address->IsLoopback() }, error);
        if (!endpoint->Socket)
        {
            endpoint->Problem = error;
            return endpoint;
        }
        endpoint->BoundPort = endpoint->Socket->Port();
        endpoint->ServerAddress = *address;

        double now = endpoint->Now();
        Slot& slot = endpoint->Slots[0];
        slot.Used = true;
        slot.Generation = 1;
        slot.Address = *address;
        slot.ClientSalt = endpoint->NewSalt();
        slot.Started = now;
        slot.Link = std::make_unique<Peer>(now);
        slot.Exchange = std::make_unique<KeyExchange>();
        if (!*slot.Exchange)
        {
            endpoint->Problem = "this system has no cryptography for secure connections";
            return endpoint;
        }
        endpoint->Running = true;
        if (endpoint->Threaded)
        {
            endpoint->Worker = std::thread(&Endpoint::Work, std::weak_ptr<Endpoint>(endpoint));
        }
        return endpoint;
    }

    Endpoint::~Endpoint()
    {
        Stopping = true;
        if (Worker.joinable())
        {
            // The thread itself lets go of the last handle when a handler held it.
            if (Worker.get_id() == std::this_thread::get_id())
            {
                Worker.detach();
            }
            else
            {
                Worker.join();
            }
        }

        // The other ends are told, but no handler runs.
        std::scoped_lock lock(Lock);
        if (!Socket)
        {
            return;
        }
        DisconnectReason code = Kind == Role::Server ? DisconnectReason::Stopped : DisconnectReason::Closed;
        for (Slot& slot : Slots)
        {
            if (slot.Used && slot.Connected)
            {
                for (int copy = 0; copy < 3; ++copy)
                {
                    SendNow(slot.Address, DisconnectPacket(slot, code));
                }
            }
        }
    }

    void Endpoint::Work(std::weak_ptr<Endpoint> weak)
    {
        while (true)
        {
            std::shared_ptr<Endpoint> endpoint = weak.lock();
            if (!endpoint || endpoint->Stopping)
            {
                return;
            }
            endpoint->UpdateNow();
            std::shared_ptr<UdpSocket> socket;
            {
                std::scoped_lock lock(endpoint->Lock);
                socket = endpoint->Socket;
            }
            if (socket)
            {
                socket->Wait(2);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    double Endpoint::ToNetworkTime(double time) const
    {
        return std::chrono::duration<double>(Start.time_since_epoch()).count() + time;
    }

    double Endpoint::Now() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
    }

    std::uint64_t Endpoint::NewSalt()
    {
        // From the system's generator, so nobody can guess the next one.
        std::uint64_t salt = 0;
        while (salt == 0)
        {
            std::array<std::uint8_t, 8> bytes {};
            if (!RandomBytes(bytes))
            {
                salt = (static_cast<std::uint64_t>(Chance.Next()) << 32) | Chance.Next();
                continue;
            }
            for (std::uint8_t byte : bytes)
            {
                salt = (salt << 8) | byte;
            }
        }
        return salt;
    }

    std::string Endpoint::Error() const
    {
        std::scoped_lock lock(Lock);
        return Problem;
    }

    std::uint16_t Endpoint::Port() const
    {
        std::scoped_lock lock(Lock);
        return BoundPort;
    }

    void Endpoint::SetMessageHandler(std::string_view name, MessageHandler handler)
    {
        std::scoped_lock lock(Lock);
        if (handler)
        {
            MessageHandlers.insert_or_assign(std::string(name), std::make_shared<MessageHandler>(std::move(handler)));
        }
        else if (auto found = MessageHandlers.find(name); found != MessageHandlers.end())
        {
            MessageHandlers.erase(found);
        }
    }

    void Endpoint::SetRequestHandler(std::string_view name, RequestHandler handler)
    {
        std::scoped_lock lock(Lock);
        if (handler)
        {
            RequestHandlers.insert_or_assign(std::string(name), std::make_shared<RequestHandler>(std::move(handler)));
        }
        else if (auto found = RequestHandlers.find(name); found != RequestHandlers.end())
        {
            RequestHandlers.erase(found);
        }
    }

    std::size_t Endpoint::AddAfterUpdate(std::function<void()> handler)
    {
        std::scoped_lock lock(Lock);
        std::size_t identifier = NextAfterUpdate++;
        AfterUpdateHandlers[identifier] = std::make_shared<std::function<void()>>(std::move(handler));
        return identifier;
    }

    void Endpoint::RemoveAfterUpdate(std::size_t handler)
    {
        std::scoped_lock lock(Lock);
        AfterUpdateHandlers.erase(handler);
    }

    void Endpoint::RunEvents(std::vector<std::function<void()>>& events)
    {
        for (std::function<void()>& event : events)
        {
            event();
        }
        events.clear();
    }

    void Endpoint::Update()
    {
        if (!Threaded)
        {
            UpdateNow();
        }
    }

    void Endpoint::UpdateNow()
    {
        std::vector<std::function<void()>> events;
        std::vector<std::shared_ptr<std::function<void()>>> afterUpdate;
        {
            std::scoped_lock lock(Lock);
            if (Updating)
            {
                return;
            }
            Updating = true;
            double now = Now();
            ReceivePackets(now);
            CheckTimes(now);
            events.swap(Events);
            for (const auto& [identifier, handler] : AfterUpdateHandlers)
            {
                afterUpdate.push_back(handler);
            }
        }

        RunEvents(events);
        for (const std::shared_ptr<std::function<void()>>& handler : afterUpdate)
        {
            if (*handler)
            {
                (*handler)();
            }
        }

        // What the handlers sent goes out in this same update.
        {
            std::scoped_lock lock(Lock);
            SendPackets(Now());
            events.swap(Events);
            Updating = false;
        }
        RunEvents(events);
    }

    void Endpoint::Stop()
    {
        std::vector<std::function<void()>> events;
        {
            std::scoped_lock lock(Lock);
            bool server = Kind == Role::Server;
            for (int index = 0; index < static_cast<int>(Slots.size()); ++index)
            {
                if (Slots[index].Used)
                {
                    Close(index, server ? "the server stopped" : "disconnected", true,
                        server ? DisconnectReason::Stopped : DisconnectReason::Closed);
                }
            }
            Running = false;
            Socket.reset();
            Delayed.clear();
            ForgetHandlers();
            events.swap(Events);
        }
        RunEvents(events);
    }

    // Handlers often hold a copy of the server or client they belong to, which
    // would keep it alive forever. Once it can do nothing more they are dropped;
    // events already collected keep the handlers they need.
    void Endpoint::ForgetHandlers()
    {
        MessageHandlers.clear();
        RequestHandlers.clear();
        AfterUpdateHandlers.clear();
        ServerConnected = nullptr;
        ServerDisconnected = nullptr;
        ClientConnected = nullptr;
        ClientDisconnected = nullptr;
    }

    void Endpoint::ReceivePackets(double now)
    {
        if (!Socket)
        {
            return;
        }
        std::array<std::uint8_t, 2048> buffer {};
        SocketAddress from;
        // Bounded, so a flood of packets cannot hold the update forever.
        for (int count = 0; count < 10000 && Socket; ++count)
        {
            std::optional<std::size_t> size = Socket->Receive(from, buffer);
            if (!size)
            {
                break;
            }
            HandlePacket(now, from, std::span<const std::uint8_t>(buffer.data(), *size));
        }
    }

    int Endpoint::SlotFor(const SocketAddress& from) const
    {
        if (Kind == Role::Client)
        {
            return Slots[0].Used && from == ServerAddress ? 0 : -1;
        }
        auto found = SlotsByAddress.find(from);
        return found == SlotsByAddress.end() ? -1 : found->second;
    }

    void Endpoint::HandlePacket(double now, const SocketAddress& from, std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        std::span<const std::uint8_t> magic = reader.ReadBytes(sizeof Magic);
        auto kind = static_cast<PacketKind>(reader.Read8());
        if (reader.Failed() || !std::ranges::equal(magic, Magic))
        {
            return;
        }

        switch (kind)
        {
        case PacketKind::ConnectRequest:
        {
            // Only a request as large as the answer is answered.
            std::uint64_t salt = reader.Read64();
            if (Kind == Role::Server && Running && !reader.Failed() && salt != 0 && bytes.size() >= ConnectRequestSize)
            {
                HandleConnectRequest(now, from, salt);
            }
            return;
        }
        case PacketKind::ConnectChallenge:
        {
            std::uint64_t salt = reader.Read64();
            Cookie cookie {};
            if (Kind != Role::Client || !ReadKey(reader, cookie) || from != ServerAddress)
            {
                return;
            }
            Slot& slot = Slots[0];
            if (slot.Used && !slot.Connected && slot.ClientSalt == salt)
            {
                slot.Challenge = cookie;
                slot.HasChallenge = true;
                slot.LastConnectRequest = -1.0;
            }
            return;
        }
        case PacketKind::ConnectResponse:
        {
            std::uint64_t salt = reader.Read64();
            Cookie cookie {};
            PublicKey clientPublic {};
            if (Kind == Role::Server && Running && salt != 0 && ReadKey(reader, cookie) && ReadKey(reader, clientPublic))
            {
                HandleConnectResponse(now, from, salt, cookie, clientPublic);
            }
            return;
        }
        case PacketKind::ConnectAccept:
        {
            if (Kind == Role::Client && from == ServerAddress)
            {
                HandleConnectAccept(now, reader);
            }
            return;
        }
        case PacketKind::ConnectRefuse:
        {
            std::uint64_t clientSalt = reader.Read64();
            reader.Read8();
            if (Kind != Role::Client || reader.Failed() || from != ServerAddress)
            {
                return;
            }
            Slot& slot = Slots[0];
            if (slot.Used && !slot.Connected && slot.ClientSalt == clientSalt)
            {
                Close(0, "the server is full", false, DisconnectReason::Closed);
            }
            return;
        }
        case PacketKind::Data:
        case PacketKind::KeepAlive:
        {
            int index = SlotFor(from);
            if (index < 0)
            {
                return;
            }
            Slot& slot = Slots[index];
            std::vector<std::uint8_t> body;
            if (!Unseal(slot, bytes, body))
            {
                return;
            }
            ByteReader opened(body);
            std::vector<DeliveredMessage> delivered;
            slot.Link->Receive(now, opened, kind == PacketKind::KeepAlive, delivered);
            if (slot.Link->OverBudget())
            {
                Close(index, "it sent more unfinished messages than a connection may hold", true, DisconnectReason::Closed);
                return;
            }
            double sentAt = ToNetworkTime(slot.Link->LastPacketSentAt());
            for (DeliveredMessage& message : delivered)
            {
                message.SentAt = sentAt;
                HandleDelivered(index, std::move(message));
            }
            if (slot.Connected && slot.Link->AckOverdue())
            {
                FlushSlot(index, now);
            }
            return;
        }
        case PacketKind::Disconnect:
        {
            int index = SlotFor(from);
            if (index < 0)
            {
                return;
            }
            Slot& slot = Slots[index];
            std::vector<std::uint8_t> body;
            if (!Unseal(slot, bytes, body))
            {
                return;
            }
            ByteReader opened(body);
            auto code = static_cast<DisconnectReason>(opened.Read8());
            if (opened.Failed())
            {
                return;
            }
            std::string reason = "disconnected";
            if (Kind == Role::Client)
            {
                reason = code == DisconnectReason::Stopped ? "the server stopped" : "the server closed the connection";
            }
            Close(index, reason, false, DisconnectReason::Closed);
            return;
        }
        case PacketKind::Discover:
        {
            std::uint64_t nonce = reader.Read64();
            if (Kind != Role::Server || !Running || reader.Failed() || bytes.size() < DiscoverRequestSize)
            {
                return;
            }
            auto clients = std::ranges::count_if(Slots, [](const Slot& slot) { return slot.Connected; });
            std::vector<std::uint8_t> reply = StartPacket(PacketKind::DiscoverReply);
            ByteWriter writer(reply);
            writer.Write64(nonce);
            writer.Write64(Instance);
            writer.Write16(static_cast<std::uint16_t>(clients));
            writer.Write16(static_cast<std::uint16_t>(Slots.size()));
            writer.Write16(BoundPort);
            writer.WriteShortText(Name);
            // Never more than was asked with.
            if (reply.size() <= bytes.size())
            {
                SendRaw(from, std::move(reply));
            }
            return;
        }
        default:
            return;
        }
    }

    Cookie Endpoint::MakeCookie(const SocketAddress& from, std::uint64_t salt, std::uint64_t expiry) const
    {
        std::vector<std::uint8_t> said;
        ByteWriter writer(said);
        writer.Write32(from.Host);
        writer.Write16(from.Port);
        writer.Write64(salt);
        writer.Write64(expiry);
        Digest proof = Authenticate(CookieSecret, said);
        Cookie cookie {};
        for (std::size_t index = 0; index < 8; ++index)
        {
            cookie[index] = static_cast<std::uint8_t>(expiry >> (8 * index));
        }
        std::copy(proof.begin(), proof.begin() + (CookieSize - 8), cookie.begin() + 8);
        return cookie;
    }

    bool Endpoint::CheckCookie(double now, const SocketAddress& from, std::uint64_t salt, const Cookie& cookie) const
    {
        std::uint64_t expiry = 0;
        for (std::size_t index = 0; index < 8; ++index)
        {
            expiry |= static_cast<std::uint64_t>(cookie[index]) << (8 * index);
        }
        if (expiry < Milliseconds(now))
        {
            return false;
        }
        return SameBytes(MakeCookie(from, salt, expiry), cookie);
    }

    bool Endpoint::AllowConnectAttempt(double now, const SocketAddress& from)
    {
        // Too many addresses at once, as in a flood: start over, which every
        // honest client survives by trying again.
        if (ConnectAllowances.size() >= MostAddressesTracked && !ConnectAllowances.contains(from))
        {
            ConnectAllowances.clear();
        }
        auto [found, added] = ConnectAllowances.try_emplace(from);
        ConnectAllowance& allowance = found->second;
        if (added)
        {
            allowance.Updated = now;
        }
        allowance.Attempts = std::min(ConnectAttemptBurst, allowance.Attempts + (now - allowance.Updated) * ConnectAttemptsPerSecond);
        allowance.Updated = now;
        if (allowance.Attempts < 1.0)
        {
            return false;
        }
        allowance.Attempts -= 1.0;
        return true;
    }

    void Endpoint::HandleConnectRequest(double now, const SocketAddress& from, std::uint64_t salt)
    {
        if (!AllowConnectAttempt(now, from))
        {
            return;
        }
        std::vector<std::uint8_t> challenge = StartPacket(PacketKind::ConnectChallenge);
        ByteWriter writer(challenge);
        writer.Write64(salt);
        WriteKey(writer, MakeCookie(from, salt, Milliseconds(now + CookieSeconds)));
        SendRaw(from, std::move(challenge));
    }

    std::vector<std::uint8_t> Endpoint::AcceptTranscript(const Slot& slot)
    {
        std::vector<std::uint8_t> transcript;
        ByteWriter writer(transcript);
        constexpr std::string_view Purpose = "easyforge network accept 3";
        writer.WriteBytes(Purpose);
        writer.Write64(slot.ClientSalt);
        writer.Write64(slot.ServerSalt);
        writer.WriteBytes(slot.ClientPublic);
        writer.WriteBytes(slot.ServerPublic);
        return transcript;
    }

    bool Endpoint::SetKeys(Slot& slot, const SecretKey& shared)
    {
        std::array<std::uint8_t, 16> salts {};
        for (std::size_t index = 0; index < 8; ++index)
        {
            salts[index] = static_cast<std::uint8_t>(slot.ClientSalt >> (8 * index));
            salts[8 + index] = static_cast<std::uint8_t>(slot.ServerSalt >> (8 * index));
        }
        std::array<std::uint8_t, 2 * KeySize> keys {};
        DeriveKeys(shared, salts, "easyforge network keys 3", keys);
        SecretKey toServer {};
        SecretKey toClient {};
        std::copy(keys.begin(), keys.begin() + KeySize, toServer.begin());
        std::copy(keys.begin() + KeySize, keys.end(), toClient.begin());
        slot.Outgoing = std::make_unique<Sealer>();
        slot.Incoming = std::make_unique<Sealer>();
        bool server = Kind == Role::Server;
        return slot.Outgoing->SetKey(server ? toClient : toServer) && slot.Incoming->SetKey(server ? toServer : toClient);
    }

    void Endpoint::HandleConnectResponse(double now, const SocketAddress& from, std::uint64_t salt, const Cookie& cookie,
        const PublicKey& clientPublic)
    {
        if (!CheckCookie(now, from, salt, cookie))
        {
            return;
        }
        if (auto found = SlotsByAddress.find(from); found != SlotsByAddress.end())
        {
            Slot& existing = Slots[found->second];
            if (existing.ClientSalt == salt)
            {
                // The accept was lost on the way.
                SendAccept(found->second);
                return;
            }
            // A new salt from the same address: the program there started over.
            Close(found->second, "disconnected", false, DisconnectReason::Closed);
        }
        if (!AllowConnectAttempt(now, from))
        {
            return;
        }

        auto free = std::ranges::find_if(Slots, [](const Slot& slot) { return !slot.Used; });
        if (free == Slots.end())
        {
            std::vector<std::uint8_t> refusal = StartPacket(PacketKind::ConnectRefuse);
            ByteWriter writer(refusal);
            writer.Write64(salt);
            writer.Write8(static_cast<std::uint8_t>(RefuseReason::Full));
            SendRaw(from, std::move(refusal));
            return;
        }

        // Agree on the keys before taking the slot; a public key that is not
        // a point on the curve gets no answer.
        KeyExchange exchange;
        SecretKey shared {};
        if (!exchange || !exchange.Agree(clientPublic, shared))
        {
            return;
        }
        int index = static_cast<int>(free - Slots.begin());
        Slot& slot = *free;
        slot.ClientSalt = salt;
        slot.ServerSalt = NewSalt();
        slot.ClientPublic = clientPublic;
        slot.ServerPublic = exchange.Public();
        if (!SetKeys(slot, shared))
        {
            slot.Outgoing.reset();
            slot.Incoming.reset();
            return;
        }
        slot.Signed = Identity && Identity->Sign(AcceptTranscript(slot), slot.ServerSignature);
        slot.Used = true;
        slot.Connected = true;
        ++slot.Generation;
        slot.Address = from;
        slot.Started = now;
        slot.Link = std::make_unique<Peer>(now);
        slot.NewestUnreliable.clear();
        slot.SentPackets = 0;
        slot.Received = {};
        SlotsByAddress[from] = index;
        SendAccept(index);
        Events.push_back([handler = ServerConnected, connection = MakeConnection(index)] {
            if (handler)
            {
                handler(connection);
            }
        });
    }

    void Endpoint::SendAccept(int index)
    {
        const Slot& slot = Slots[index];
        std::vector<std::uint8_t> packet = StartPacket(PacketKind::ConnectAccept);
        ByteWriter writer(packet);
        writer.Write64(slot.ClientSalt);
        writer.Write64(slot.ServerSalt);
        writer.Write16(static_cast<std::uint16_t>(index));
        WriteKey(writer, slot.ServerPublic);
        writer.Write8(slot.Signed ? 1 : 0);
        if (slot.Signed)
        {
            WriteKey(writer, slot.ServerSignature);
        }
        SendRaw(slot.Address, std::move(packet));
    }

    void Endpoint::HandleConnectAccept(double now, ByteReader& reader)
    {
        std::uint64_t clientSalt = reader.Read64();
        std::uint64_t serverSalt = reader.Read64();
        reader.Read16();
        PublicKey serverPublic {};
        if (!ReadKey(reader, serverPublic))
        {
            return;
        }
        bool signedAccept = reader.Read8() != 0;
        Signature signature {};
        if (reader.Failed() || (signedAccept && !ReadKey(reader, signature)))
        {
            return;
        }
        Slot& slot = Slots[0];
        if (!slot.Used || slot.Connected || slot.ClientSalt != clientSalt || !slot.Exchange)
        {
            return;
        }
        slot.ServerSalt = serverSalt;
        slot.ClientPublic = slot.Exchange->Public();
        slot.ServerPublic = serverPublic;
        if (PinsServer && (!signedAccept || serverPublic == PublicKey {} ||
                              !Verify(PinnedServer, AcceptTranscript(slot), signature)))
        {
            Close(0, "the server could not prove it is the one ServerKey names", false, DisconnectReason::Closed);
            return;
        }
        SecretKey shared {};
        if (!slot.Exchange->Agree(serverPublic, shared) || !SetKeys(slot, shared))
        {
            return;
        }
        slot.Exchange.reset();
        slot.Connected = true;
        slot.Link->Heard(now);
        Events.push_back([handler = ClientConnected] {
            if (handler)
            {
                handler();
            }
        });
    }

    std::vector<std::uint8_t> Endpoint::Seal(Slot& slot, std::span<const std::uint8_t> plain)
    {
        // The plain packet starts with magic, kind, and token; the rest is sealed.
        constexpr std::size_t Open = 4 + 1 + 8;
        std::vector<std::uint8_t> packet(plain.begin(), plain.begin() + static_cast<std::ptrdiff_t>(std::min(Open, plain.size())));
        if (plain.size() < Open || !slot.Outgoing)
        {
            return {};
        }
        std::uint64_t counter = ++slot.SentPackets;
        ByteWriter writer(packet);
        writer.Write64(counter);
        std::vector<std::uint8_t> header = packet;
        if (!slot.Outgoing->Seal(counter, header, plain.subspan(Open), packet))
        {
            return {};
        }
        return packet;
    }

    bool Endpoint::Unseal(Slot& slot, std::span<const std::uint8_t> packet, std::vector<std::uint8_t>& body)
    {
        if (!slot.Connected || !slot.Incoming || packet.size() < SealedHeaderSize + TagSize)
        {
            return false;
        }
        ByteReader reader(packet.subspan(4 + 1));
        std::uint64_t token = reader.Read64();
        std::uint64_t counter = reader.Read64();
        if (reader.Failed() || token != slot.Token() || !slot.Received.IsFresh(counter))
        {
            return false;
        }
        if (!slot.Incoming->Open(counter, packet.first(SealedHeaderSize), packet.subspan(SealedHeaderSize), body))
        {
            return false;
        }
        slot.Received.Take(counter);
        return true;
    }

    std::vector<std::uint8_t> Endpoint::DisconnectPacket(Slot& slot, DisconnectReason code)
    {
        std::vector<std::uint8_t> plain = StartPacket(PacketKind::Disconnect);
        ByteWriter writer(plain);
        writer.Write64(slot.Token());
        writer.Write8(static_cast<std::uint8_t>(code));
        return Seal(slot, plain);
    }

    void Endpoint::HandleDelivered(int index, DeliveredMessage delivered)
    {
        Slot& slot = Slots[index];
        if (!slot.Connected)
        {
            return;
        }
        ByteReader reader(delivered.Bytes);
        auto kind = static_cast<MessageKind>(reader.Read8());
        switch (kind)
        {
        case MessageKind::Message:
        {
            std::string name = reader.ReadShortText();
            if (reader.Failed())
            {
                return;
            }
            if (delivered.Kind == Channel::Unreliable)
            {
                auto [newest, added] = slot.NewestUnreliable.try_emplace(name, delivered.Sequence);
                if (!added)
                {
                    if (static_cast<std::int32_t>(delivered.Sequence - newest->second) <= 0)
                    {
                        return;
                    }
                    newest->second = delivered.Sequence;
                }
            }
            auto handler = MessageHandlers.find(name);
            if (handler == MessageHandlers.end())
            {
                return;
            }
            std::optional<Message> body = Message::Decode(reader.ReadBytes(reader.Remaining()));
            if (!body)
            {
                return;
            }
            body->Sent = delivered.SentAt;
            Events.push_back([handler = handler->second, connection = MakeConnection(index), body = std::move(*body)] {
                (*handler)(connection, body);
            });
            return;
        }
        case MessageKind::Request:
        {
            std::uint32_t request = reader.Read32();
            std::string name = reader.ReadShortText();
            if (reader.Failed())
            {
                return;
            }
            // Every answer is sent from an event, refusals too, so replies leave in
            // the order the requests came.
            std::optional<Message> body = Message::Decode(reader.ReadBytes(reader.Remaining()));
            if (body)
            {
                body->Sent = delivered.SentAt;
            }
            auto handler = RequestHandlers.find(name);
            std::shared_ptr<RequestHandler> answering = handler == RequestHandlers.end() ? nullptr : handler->second;
            std::string refusal;
            if (!body)
            {
                refusal = "the request could not be read";
            }
            else if (!answering)
            {
                refusal = std::format("nothing answers \"{}\" here", name);
            }
            Events.push_back([weak = Self, answering, refusal, connection = MakeConnection(index), index,
                                 generation = slot.Generation, request, body = body.value_or(Message {})] {
                Result<Message> answer = refusal.empty() ? (*answering)(connection, body) : Result<Message>(Failure(refusal));
                if (std::shared_ptr<Endpoint> endpoint = weak.lock())
                {
                    std::scoped_lock lock(endpoint->Lock);
                    endpoint->SendReply(index, generation, request, answer);
                }
            });
            return;
        }
        case MessageKind::Reply:
        case MessageKind::ReplyError:
        {
            std::uint32_t request = reader.Read32();
            if (reader.Failed())
            {
                return;
            }
            auto pending = Requests.find(request);
            if (pending == Requests.end() || pending->second.Slot != index || pending->second.Generation != slot.Generation)
            {
                return;
            }
            Reply reply;
            if (kind == MessageKind::Reply)
            {
                std::optional<Message> body = Message::Decode(reader.ReadBytes(reader.Remaining()));
                if (body)
                {
                    body->Sent = delivered.SentAt;
                    reply.Message = std::move(*body);
                }
                else
                {
                    reply.Error = "the reply could not be read";
                }
            }
            else
            {
                reply.Error = reader.ReadText(reader.Remaining());
                if (reply.Error.empty())
                {
                    reply.Error = "the request failed";
                }
            }
            Events.push_back([callback = std::move(pending->second.Callback), reply = std::move(reply)] {
                if (callback)
                {
                    callback(reply);
                }
            });
            Requests.erase(pending);
            return;
        }
        default:
            return;
        }
    }

    void Endpoint::SendReply(int index, std::uint32_t generation, std::uint32_t request, const Result<Message>& answer)
    {
        Slot* slot = FindSlot(index, generation);
        if (!slot || !slot->Link)
        {
            return;
        }
        std::vector<std::uint8_t> bytes;
        ByteWriter writer(bytes);
        if (answer)
        {
            writer.Write8(static_cast<std::uint8_t>(MessageKind::Reply));
            writer.Write32(request);
            writer.WriteBytes(answer->Encode());
        }
        if (!answer || bytes.size() > MessageLimit)
        {
            bytes.clear();
            writer.Write8(static_cast<std::uint8_t>(MessageKind::ReplyError));
            writer.Write32(request);
            writer.WriteBytes(answer ? std::format("the answer is larger than {} MB", MessageLimit >> 20) : answer.Error());
        }
        slot->Link->Queue(Channel::Reliable, std::move(bytes));
    }

    void Endpoint::CheckTimes(double now)
    {
        for (int index = 0; index < static_cast<int>(Slots.size()); ++index)
        {
            Slot& slot = Slots[index];
            if (!slot.Used)
            {
                continue;
            }
            if (slot.Connected)
            {
                if (now - slot.Link->LastReceived() > Timeout)
                {
                    Close(index, "timed out", true, DisconnectReason::Closed);
                }
            }
            else if (Kind == Role::Client && now - slot.Started > ConnectTimeout)
            {
                Close(index, "the server did not answer", false, DisconnectReason::Closed);
            }
        }

        for (auto request = Requests.begin(); request != Requests.end();)
        {
            if (request->second.Deadline > now)
            {
                ++request;
                continue;
            }
            Events.push_back([callback = std::move(request->second.Callback)] {
                if (callback)
                {
                    callback(Reply { .Error = "the request timed out" });
                }
            });
            request = Requests.erase(request);
        }
    }

    void Endpoint::SendPackets(double now)
    {
        if (!Socket)
        {
            return;
        }
        for (int index = 0; index < static_cast<int>(Slots.size()); ++index)
        {
            Slot& slot = Slots[index];
            if (!slot.Used)
            {
                continue;
            }
            if (slot.Connected)
            {
                FlushSlot(index, now);
            }
            else if (Kind == Role::Client && (slot.LastConnectRequest < 0.0 || now - slot.LastConnectRequest >= ConnectRetrySeconds))
            {
                // First a request, padded so the answer is never larger; with
                // the server's cookie, the response with this end's key.
                std::vector<std::uint8_t> packet =
                    StartPacket(slot.HasChallenge ? PacketKind::ConnectResponse : PacketKind::ConnectRequest);
                ByteWriter writer(packet);
                writer.Write64(slot.ClientSalt);
                if (slot.HasChallenge && slot.Exchange)
                {
                    WriteKey(writer, slot.Challenge);
                    WriteKey(writer, slot.Exchange->Public());
                }
                else
                {
                    packet.resize(ConnectRequestSize, 0);
                }
                SendRaw(slot.Address, std::move(packet));
                slot.LastConnectRequest = now;

                // Every fourth response goes with a new request, so a cookie the
                // server no longer takes (it ran out, or the server started
                // again) is replaced.
                if (slot.HasChallenge && ++slot.ResponsesSent % 4 == 0)
                {
                    std::vector<std::uint8_t> request = StartPacket(PacketKind::ConnectRequest);
                    ByteWriter requestWriter(request);
                    requestWriter.Write64(slot.ClientSalt);
                    request.resize(ConnectRequestSize, 0);
                    SendRaw(slot.Address, std::move(request));
                }
            }
        }

        if (!Delayed.empty())
        {
            auto waiting = std::stable_partition(
                Delayed.begin(), Delayed.end(), [now](const DelayedPacket& packet) { return packet.Release <= now; });
            std::vector<DelayedPacket> due(std::make_move_iterator(Delayed.begin()), std::make_move_iterator(waiting));
            Delayed.erase(Delayed.begin(), waiting);
            std::ranges::stable_sort(due, {}, &DelayedPacket::Release);
            for (const DelayedPacket& packet : due)
            {
                Socket->Send(packet.To, packet.Bytes);
            }
        }
    }

    void Endpoint::FlushSlot(int index, double now)
    {
        Slot& slot = Slots[index];
        std::vector<std::vector<std::uint8_t>> packets;
        slot.Link->Flush(now, slot.Token(), packets);
        for (std::vector<std::uint8_t>& packet : packets)
        {
            std::vector<std::uint8_t> sealed = Seal(slot, packet);
            if (!sealed.empty())
            {
                SendRaw(slot.Address, std::move(sealed));
            }
        }
    }

    void Endpoint::SendRaw(const SocketAddress& to, std::vector<std::uint8_t> bytes)
    {
        if (!Socket)
        {
            return;
        }
        if (Conditions.Loss > 0.0f && Chance.Fraction() < Conditions.Loss)
        {
            return;
        }
        double delay = Conditions.Latency;
        if (Conditions.Jitter > 0.0f)
        {
            delay += Chance.Between(-Conditions.Jitter, Conditions.Jitter);
        }
        if (delay > 0.0)
        {
            Delayed.push_back({ Now() + delay, to, std::move(bytes) });
            return;
        }
        Socket->Send(to, bytes);
    }

    void Endpoint::SendNow(const SocketAddress& to, std::span<const std::uint8_t> bytes)
    {
        if (Socket)
        {
            Socket->Send(to, bytes);
        }
    }

    void Endpoint::Close(int index, const std::string& reason, bool tellOtherEnd, DisconnectReason code)
    {
        Slot& slot = Slots[index];
        if (!slot.Used)
        {
            return;
        }
        if (tellOtherEnd && slot.Connected)
        {
            for (int copy = 0; copy < 3; ++copy)
            {
                SendNow(slot.Address, DisconnectPacket(slot, code));
            }
        }

        Connection connection = MakeConnection(index);
        for (auto request = Requests.begin(); request != Requests.end();)
        {
            if (request->second.Slot != index || request->second.Generation != slot.Generation)
            {
                ++request;
                continue;
            }
            Events.push_back([callback = std::move(request->second.Callback)] {
                if (callback)
                {
                    callback(Reply { .Error = "the connection closed" });
                }
            });
            request = Requests.erase(request);
        }

        // The address and generation stay, so handles can still say where the
        // connection went.
        if (Kind == Role::Server)
        {
            SlotsByAddress.erase(slot.Address);
        }
        slot.Used = false;
        slot.Connected = false;
        slot.Link.reset();
        slot.NewestUnreliable.clear();
        slot.Exchange.reset();
        slot.HasChallenge = false;
        slot.ResponsesSent = 0;
        slot.Outgoing.reset();
        slot.Incoming.reset();
        slot.SentPackets = 0;
        slot.Received = {};

        if (Kind == Role::Server)
        {
            Events.push_back([handler = ServerDisconnected, connection, reason] {
                if (handler)
                {
                    handler(connection, reason);
                }
            });
            return;
        }
        Running = false;
        Events.push_back([handler = ClientDisconnected, reason] {
            if (handler)
            {
                handler(reason);
            }
        });
        ForgetHandlers();
    }

    Endpoint::Slot* Endpoint::FindSlot(int index, std::uint32_t generation)
    {
        if (index < 0 || index >= static_cast<int>(Slots.size()))
        {
            return nullptr;
        }
        Slot& slot = Slots[index];
        return slot.Used && slot.Generation == generation ? &slot : nullptr;
    }

    const Endpoint::Slot* Endpoint::FindSlot(int index, std::uint32_t generation) const
    {
        return const_cast<Endpoint*>(this)->FindSlot(index, generation);
    }

    Connection Endpoint::MakeConnection(int index) const
    {
        return Connection(Self, index, Slots[index].Generation);
    }

    bool Endpoint::IsConnected(int index, std::uint32_t generation) const
    {
        std::scoped_lock lock(Lock);
        const Slot* slot = FindSlot(index, generation);
        return slot && slot->Connected;
    }

    std::string Endpoint::AddressOf(int index, std::uint32_t generation) const
    {
        std::scoped_lock lock(Lock);
        if (index < 0 || index >= static_cast<int>(Slots.size()) || Slots[index].Generation != generation)
        {
            return {};
        }
        return Slots[index].Address.Text();
    }

    float Endpoint::RoundTripOf(int index, std::uint32_t generation) const
    {
        std::scoped_lock lock(Lock);
        const Slot* slot = FindSlot(index, generation);
        return slot && slot->Connected ? slot->Link->RoundTrip() : 0.0f;
    }

    void Endpoint::Send(int index, std::uint32_t generation, std::string_view name, const Message& message, Delivery delivery)
    {
        std::vector<std::uint8_t> bytes;
        ByteWriter writer(bytes);
        writer.Write8(static_cast<std::uint8_t>(MessageKind::Message));
        writer.WriteShortText(name);
        writer.WriteBytes(message.Encode());
        if (bytes.size() > MessageLimit)
        {
            Log(LogLevel::Warning, "easyforge network: {}, so it was not sent", TooLarge("message", name));
            return;
        }
        std::scoped_lock lock(Lock);
        Slot* slot = FindSlot(index, generation);
        if (slot && slot->Link)
        {
            slot->Link->Queue(ChannelFor(delivery), std::move(bytes));
        }
    }

    void Endpoint::SendToAll(std::string_view name, const Message& message, Delivery delivery)
    {
        std::vector<std::uint8_t> bytes;
        ByteWriter writer(bytes);
        writer.Write8(static_cast<std::uint8_t>(MessageKind::Message));
        writer.WriteShortText(name);
        writer.WriteBytes(message.Encode());
        if (bytes.size() > MessageLimit)
        {
            Log(LogLevel::Warning, "easyforge network: {}, so it was not sent", TooLarge("message", name));
            return;
        }
        std::scoped_lock lock(Lock);
        for (Slot& slot : Slots)
        {
            if (slot.Connected)
            {
                slot.Link->Queue(ChannelFor(delivery), bytes);
            }
        }
    }

    void Endpoint::Request(int index, std::uint32_t generation, std::string_view name, const Message& message,
        std::function<void(const Reply&)> callback, const RequestSettings& settings)
    {
        std::vector<std::uint8_t> body = message.Encode();
        std::scoped_lock lock(Lock);
        Slot* slot = FindSlot(index, generation);
        std::string problem;
        if (!slot || !slot->Link)
        {
            problem = "not connected";
        }
        else if (body.size() + name.size() + 6 > MessageLimit)
        {
            problem = TooLarge("request", name);
        }
        if (!problem.empty())
        {
            // Answered during the next update, like every other reply.
            Events.push_back([callback = std::move(callback), problem] {
                if (callback)
                {
                    callback(Reply { .Error = problem });
                }
            });
            return;
        }

        std::uint32_t request = NextRequest++;
        if (NextRequest == 0)
        {
            NextRequest = 1;
        }
        std::vector<std::uint8_t> bytes;
        ByteWriter writer(bytes);
        writer.Write8(static_cast<std::uint8_t>(MessageKind::Request));
        writer.Write32(request);
        writer.WriteShortText(name);
        writer.WriteBytes(body);
        Requests[request] = { index, generation, Now() + std::max(settings.Timeout, 0.0f), std::move(callback) };
        slot->Link->Queue(Channel::Reliable, std::move(bytes));
    }

    void Endpoint::Disconnect(int index, std::uint32_t generation)
    {
        std::vector<std::function<void()>> events;
        {
            std::scoped_lock lock(Lock);
            if (!FindSlot(index, generation))
            {
                return;
            }
            Close(index, "disconnected", true, DisconnectReason::Closed);
            if (Kind == Role::Client)
            {
                Socket.reset();
                Delayed.clear();
            }
            events.swap(Events);
        }
        RunEvents(events);
    }

    std::vector<Connection> Endpoint::Connections() const
    {
        std::scoped_lock lock(Lock);
        std::vector<Connection> connections;
        for (int index = 0; index < static_cast<int>(Slots.size()); ++index)
        {
            if (Slots[index].Connected)
            {
                connections.push_back(MakeConnection(index));
            }
        }
        return connections;
    }

    std::uint32_t Endpoint::ClientGeneration() const
    {
        std::scoped_lock lock(Lock);
        return Slots.empty() ? 0 : Slots[0].Generation;
    }

    bool Endpoint::ClientIsConnected() const
    {
        std::scoped_lock lock(Lock);
        return Kind == Role::Client && !Slots.empty() && Slots[0].Connected;
    }

    Connection Endpoint::ServerConnection() const
    {
        std::scoped_lock lock(Lock);
        if (Kind != Role::Client || Slots.empty() || !Slots[0].Used)
        {
            return {};
        }
        return MakeConnection(0);
    }
}
