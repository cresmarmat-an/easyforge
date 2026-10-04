#include "Endpoint.h"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>

#include <easyforge/core/Log.h>

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
                std::vector<std::uint8_t> packet = StartPacket(PacketKind::Disconnect);
                ByteWriter writer(packet);
                writer.Write64(slot.Token());
                writer.Write8(static_cast<std::uint8_t>(code));
                for (int copy = 0; copy < 3; ++copy)
                {
                    SendNow(slot.Address, packet);
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

    double Endpoint::Now() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
    }

    std::uint64_t Endpoint::NewSalt()
    {
        std::uint64_t salt = 0;
        while (salt == 0)
        {
            salt = (static_cast<std::uint64_t>(Chance.Next()) << 32) | Chance.Next();
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
            std::uint64_t salt = reader.Read64();
            if (Kind == Role::Server && Running && !reader.Failed() && salt != 0)
            {
                HandleConnectRequest(now, from, salt);
            }
            return;
        }
        case PacketKind::ConnectAccept:
        {
            std::uint64_t clientSalt = reader.Read64();
            std::uint64_t serverSalt = reader.Read64();
            reader.Read16();
            if (Kind != Role::Client || reader.Failed() || from != ServerAddress)
            {
                return;
            }
            Slot& slot = Slots[0];
            if (!slot.Used || slot.Connected || slot.ClientSalt != clientSalt)
            {
                return;
            }
            slot.ServerSalt = serverSalt;
            slot.Connected = true;
            slot.Link->Heard(now);
            Events.push_back([handler = ClientConnected] {
                if (handler)
                {
                    handler();
                }
            });
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
        {
            std::uint64_t token = reader.Read64();
            int index = SlotFor(from);
            if (index < 0 || reader.Failed())
            {
                return;
            }
            Slot& slot = Slots[index];
            if (!slot.Connected || slot.Token() != token)
            {
                return;
            }
            // A damaged packet still delivers what came before the damage.
            std::vector<DeliveredMessage> delivered;
            slot.Link->Receive(now, reader, delivered);
            for (DeliveredMessage& message : delivered)
            {
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
            std::uint64_t token = reader.Read64();
            auto code = static_cast<DisconnectReason>(reader.Read8());
            int index = SlotFor(from);
            if (index < 0 || reader.Failed())
            {
                return;
            }
            Slot& slot = Slots[index];
            if (!slot.Connected || slot.Token() != token)
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
            if (Kind != Role::Server || !Running || reader.Failed())
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
            SendRaw(from, std::move(reply));
            return;
        }
        default:
            return;
        }
    }

    void Endpoint::HandleConnectRequest(double now, const SocketAddress& from, std::uint64_t salt)
    {
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

        int index = static_cast<int>(free - Slots.begin());
        Slot& slot = *free;
        slot.Used = true;
        slot.Connected = true;
        ++slot.Generation;
        slot.Address = from;
        slot.ClientSalt = salt;
        slot.ServerSalt = NewSalt();
        slot.Started = now;
        slot.Link = std::make_unique<Peer>(now);
        slot.NewestUnreliable.clear();
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
        SendRaw(slot.Address, std::move(packet));
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
                std::vector<std::uint8_t> packet = StartPacket(PacketKind::ConnectRequest);
                ByteWriter writer(packet);
                writer.Write64(slot.ClientSalt);
                SendRaw(slot.Address, std::move(packet));
                slot.LastConnectRequest = now;
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
            SendRaw(slot.Address, std::move(packet));
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
            std::vector<std::uint8_t> packet = StartPacket(PacketKind::Disconnect);
            ByteWriter writer(packet);
            writer.Write64(slot.Token());
            writer.Write8(static_cast<std::uint8_t>(code));
            for (int copy = 0; copy < 3; ++copy)
            {
                SendNow(slot.Address, packet);
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
