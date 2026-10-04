#pragma once

// Shares a data table between a server and its clients. Header-only, so it
// compiles only in programs that use both libraries.
//
//     #include <easyforge/bridges/data_network.h>
//
//     // on the server
//     TableShare shared = Share(game, server, Sharing::TwoWay);
//
//     // on each client
//     TableShare copy = Share(game, client);
//
// A client's table is emptied and filled with the server's, and from then on
// every change on the server reaches every client. With Sharing::OneWay the
// clients only receive. With Sharing::TwoWay each node belongs to whoever
// added it, and a client's changes to its own nodes reach the server and the
// other clients; a change to a node it does not own is undone by the server.
// The server may change any node. Types are defined by the server only.
//
// Changes travel during Update, as reliable messages named "easyforge.table."
// followed by the share's name. Sharing continues for as long as the server or
// client runs, whether or not the TableShare is kept; Stop ends it. The table
// is read and changed during Update, on the network's thread when the server or
// client is threaded, so share tables only with ends that are not threaded.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <easyforge/data.h>
#include <easyforge/network.h>

namespace easyforge
{
    enum class Sharing
    {
        // Clients receive the server's table and send nothing back.
        OneWay,

        // Clients also change the nodes they added.
        TwoWay,
    };

    namespace internal::bridges
    {
        inline MessageValue DataToMessage(const DataValue& value)
        {
            switch (value.Type())
            {
            case DataType::Nothing: return {};
            case DataType::Boolean: return value.AsBoolean();
            case DataType::Integer: return value.AsInteger();
            case DataType::Number: return value.AsNumber();
            case DataType::Text: return value.AsText();
            case DataType::Vector2: return value.AsVector2();
            case DataType::Vector3: return value.AsVector3();
            case DataType::Vector4: return value.AsVector4();
            case DataType::Color: return value.AsColor();
            }
            return {};
        }

        // Bytes have no place in a table, so they read as nothing.
        inline DataValue MessageToData(const MessageValue& value)
        {
            switch (value.Kind())
            {
            case MessageValueKind::Boolean: return value.AsBoolean();
            case MessageValueKind::Integer: return value.AsInteger();
            case MessageValueKind::Number: return value.AsNumber();
            case MessageValueKind::Text: return value.AsText();
            case MessageValueKind::Vector2: return value.AsVector2();
            case MessageValueKind::Vector3: return value.AsVector3();
            case MessageValueKind::Vector4: return value.AsVector4();
            case MessageValueKind::Color: return value.AsColor();
            case MessageValueKind::Nothing:
            case MessageValueKind::Bytes: break;
            }
            return {};
        }

        inline std::vector<std::uint8_t> EncodeValues(const PropertyValues& values)
        {
            Message message;
            for (const auto& [name, value] : values)
            {
                message.Set(name, DataToMessage(value));
            }
            return message.Encode();
        }

        inline PropertyValues DecodeValues(const MessageValue& bytes)
        {
            PropertyValues values;
            std::vector<std::uint8_t> encoded = bytes.AsBytes();
            if (std::optional<Message> message = Message::Decode(encoded))
            {
                for (const std::string& name : message->Names())
                {
                    values.emplace_back(name, MessageToData(message->Get(name)));
                }
            }
            return values;
        }

        inline PropertyValues OwnValues(const Node& node)
        {
            PropertyValues values;
            for (const std::string& name : node.Properties())
            {
                values.emplace_back(name, node.Get(name));
            }
            return values;
        }

        // Records go in one message as a list: a 4-byte length, then the record.
        inline std::vector<std::uint8_t> PackRecords(const std::vector<Message>& records)
        {
            std::vector<std::uint8_t> bytes;
            for (const Message& record : records)
            {
                std::vector<std::uint8_t> encoded = record.Encode();
                auto size = static_cast<std::uint32_t>(encoded.size());
                for (int shift = 0; shift < 32; shift += 8)
                {
                    bytes.push_back(static_cast<std::uint8_t>(size >> shift));
                }
                bytes.insert(bytes.end(), encoded.begin(), encoded.end());
            }
            return bytes;
        }

        inline std::vector<Message> UnpackRecords(const MessageValue& value)
        {
            std::vector<std::uint8_t> bytes = value.AsBytes();
            std::vector<Message> records;
            std::size_t position = 0;
            while (bytes.size() - position >= 4)
            {
                std::uint32_t size = 0;
                for (int index = 0; index < 4; ++index)
                {
                    size |= static_cast<std::uint32_t>(bytes[position + index]) << (8 * index);
                }
                position += 4;
                if (bytes.size() - position < size)
                {
                    break;
                }
                std::optional<Message> record = Message::Decode(std::span(bytes).subspan(position, size));
                position += size;
                if (record)
                {
                    records.push_back(std::move(*record));
                }
            }
            return records;
        }

        // What one end of a share knows about the table. Every shared node has
        // a share number: the server's node identifier. A client numbers the
        // nodes it adds below zero until the server says which number they got.
        class ShareState
        {
        public:
            struct Entry
            {
                Node Local;
                std::uint64_t Identifier = 0;
                std::int64_t Parent = 0;
            };

            // Messages sent on the server's behalf, each to its own recipients.
            struct Chunk
            {
                std::vector<Message> Records;
                std::vector<Connection> Recipients;
            };

            // A client taking part, and the numbers its new nodes were given.
            struct Member
            {
                Connection Link;
                std::map<std::int64_t, std::int64_t> Assigned;
            };

            ShareState(Table table, bool server, std::string name)
                : Data(std::move(table)), IsServer(server), Prefix("easyforge.table." + std::move(name) + ".")
            {
                LastVersion = Data.Version();
            }

            Table Data;
            bool IsServer;
            std::string Prefix;
            bool TwoWay = false;
            bool Stopped = false;
            bool Ready = false;
            std::uint64_t LastVersion = 0;
            std::map<std::int64_t, Entry> Nodes;
            std::map<std::uint64_t, std::int64_t> ShareNumbers;
            std::int64_t NextLocalNumber = -1;

            // The server's.
            std::vector<Member> Members;
            std::map<std::int64_t, Connection> Owners;
            std::vector<Chunk> Outgoing;

            // The client's.
            Connection ToServer;
            std::vector<Message> Pending;

            std::string Named(std::string_view kind) const { return Prefix + std::string(kind); }

            std::int64_t NumberOf(const Node& node) const
            {
                auto found = ShareNumbers.find(node.Identifier());
                return node && found != ShareNumbers.end() ? found->second : 0;
            }

            Node NodeNumbered(std::int64_t number) const
            {
                auto found = Nodes.find(number);
                return found == Nodes.end() ? Node() : found->second.Local;
            }

            std::int64_t Register(const Node& node, std::int64_t parent, std::int64_t number = 0)
            {
                if (number == 0)
                {
                    number = IsServer ? static_cast<std::int64_t>(node.Identifier()) : NextLocalNumber--;
                }
                Nodes[number] = { node, node.Identifier(), parent };
                ShareNumbers[node.Identifier()] = number;
                return number;
            }

            void RegisterAll()
            {
                for (const Node& node : Data.Nodes())
                {
                    Register(node, node.Parent() ? NumberOf(node.Parent()) : 0);
                }
            }

            static Message AddRecord(std::int64_t number, std::int64_t parent, const Node& node)
            {
                return Message {
                    { "Kind", "Add" },
                    { "Node", number },
                    { "Parent", parent },
                    { "Name", node.Name() },
                    { "Type", node.TypeName() },
                    { "Values", EncodeValues(OwnValues(node)) },
                };
            }

            Message TypeRecord(const std::string& name) const
            {
                return Message { { "Kind", "Type" }, { "Name", name }, { "Values", EncodeValues(Data.TypeValues(name)) } };
            }

            // A new node and everything under it that is new too, parents first.
            void WriteNew(const Node& node, std::vector<Message>& records, std::set<std::int64_t>& fresh)
            {
                if (!node || NumberOf(node) != 0)
                {
                    return;
                }
                Node parent = node.Parent();
                std::int64_t parentNumber = parent ? NumberOf(parent) : 0;
                if (parent && parentNumber == 0)
                {
                    return;
                }
                std::int64_t number = Register(node, parentNumber);
                fresh.insert(number);
                records.push_back(AddRecord(number, parentNumber, node));
                for (const Node& child : node.Children())
                {
                    WriteNew(child, records, fresh);
                }
            }

            // A node and everything under it as they are now, known or not.
            void WriteWhole(const Node& node, std::vector<Message>& records) const
            {
                std::int64_t number = NumberOf(node);
                if (number == 0)
                {
                    return;
                }
                Node parent = node.Parent();
                records.push_back(AddRecord(number, parent ? NumberOf(parent) : 0, node));
                for (const Node& child : node.Children())
                {
                    WriteWhole(child, records);
                }
            }

            std::vector<Message> Snapshot() const
            {
                std::vector<Message> records;
                for (const std::string& type : Data.TypeNames())
                {
                    records.push_back(TypeRecord(type));
                }
                for (const Node& node : Data.Nodes())
                {
                    Node parent = node.Parent();
                    records.push_back(AddRecord(NumberOf(node), parent ? NumberOf(parent) : 0, node));
                }
                return records;
            }

            // Forgets nodes that are gone, and with `records` names the topmost
            // of them for the other ends.
            void Sweep(std::vector<Message>* records)
            {
                std::vector<std::int64_t> gone;
                for (const auto& [number, entry] : Nodes)
                {
                    if (!entry.Local)
                    {
                        gone.push_back(number);
                    }
                }
                for (std::int64_t number : gone)
                {
                    const Entry& entry = Nodes[number];
                    auto parent = Nodes.find(entry.Parent);
                    bool topmost = entry.Parent == 0 || parent == Nodes.end() || parent->second.Local;
                    if (records && topmost)
                    {
                        records->push_back(Message { { "Kind", "Remove" }, { "Node", number } });
                    }
                }
                for (std::int64_t number : gone)
                {
                    ShareNumbers.erase(Nodes[number].Identifier);
                    Nodes.erase(number);
                    Owners.erase(number);
                }
            }

            // The table's changes since last time, as records.
            std::vector<Message> CollectChanges()
            {
                std::vector<Message> records;
                std::vector<Change> changes = Data.ChangesSince(LastVersion);
                LastVersion = Data.Version();
                std::set<std::int64_t> fresh;
                bool removed = false;
                for (const Change& change : changes)
                {
                    std::int64_t number = NumberOf(change.Target);
                    bool skip = number == 0 || fresh.contains(number);
                    switch (change.Kind)
                    {
                    case ChangeKind::Added:
                        WriteNew(change.Target, records, fresh);
                        break;
                    case ChangeKind::Removed:
                        removed = true;
                        break;
                    case ChangeKind::PropertySet:
                        if (!skip)
                        {
                            records.push_back(Message {
                                { "Kind", "Set" },
                                { "Node", number },
                                { "Property", change.Property },
                                { "Value", DataToMessage(change.New) },
                            });
                        }
                        break;
                    case ChangeKind::Renamed:
                        if (!skip)
                        {
                            records.push_back(Message { { "Kind", "Rename" }, { "Node", number }, { "Name", change.New.AsText() } });
                        }
                        break;
                    case ChangeKind::Moved:
                    {
                        std::int64_t parent = change.Parent ? NumberOf(change.Parent) : 0;
                        if (!skip && !(change.Parent && parent == 0))
                        {
                            Nodes[number].Parent = parent;
                            records.push_back(Message { { "Kind", "Move" }, { "Node", number }, { "Parent", parent } });
                        }
                        break;
                    }
                    case ChangeKind::TypeDefined:
                        records.push_back(TypeRecord(change.Property));
                        break;
                    }
                }
                if (removed)
                {
                    Sweep(&records);
                }
                return records;
            }

            // Adds a node, or brings an existing one up to date.
            void Place(std::int64_t number, std::int64_t parentNumber, const Message& record)
            {
                Node parent;
                if (parentNumber != 0)
                {
                    parent = NodeNumbered(parentNumber);
                    if (!parent)
                    {
                        return;
                    }
                }
                std::string name = record["Name"].AsText();
                PropertyValues values = DecodeValues(record["Values"]);
                if (Node existing = NodeNumbered(number))
                {
                    existing.Rename(name);
                    if (!(existing.Parent() == parent))
                    {
                        existing.MoveTo(parent);
                    }
                    for (const std::string& property : existing.Properties())
                    {
                        bool kept = false;
                        for (const auto& value : values)
                        {
                            kept = kept || value.first == property;
                        }
                        if (!kept)
                        {
                            existing.Set(property, {});
                        }
                    }
                    for (const auto& [property, value] : values)
                    {
                        existing.Set(property, value);
                    }
                    Nodes[number].Parent = parentNumber;
                    return;
                }
                std::string type = record["Type"].AsText();
                Node added = parent ? parent.Add(name, type, values) : Data.Add(name, type, values);
                Register(added, parentNumber, number);
            }

            // A client applying what the server sent.
            void ApplyFromServer(const std::vector<Message>& records)
            {
                bool removed = false;
                for (const Message& record : records)
                {
                    std::string kind = record["Kind"].AsText();
                    std::int64_t number = record["Node"].AsInteger();
                    Node node = NodeNumbered(number);
                    if (kind == "Add")
                    {
                        Place(number, record["Parent"].AsInteger(), record);
                    }
                    else if (kind == "Remove" && node)
                    {
                        node.Remove();
                        removed = true;
                    }
                    else if (kind == "Set" && node)
                    {
                        node.Set(record["Property"].AsText(), MessageToData(record["Value"]));
                    }
                    else if (kind == "Rename" && node)
                    {
                        node.Rename(record["Name"].AsText());
                    }
                    else if (kind == "Move" && node)
                    {
                        std::int64_t parentNumber = record["Parent"].AsInteger();
                        Node parent = NodeNumbered(parentNumber);
                        if (parentNumber == 0 || parent)
                        {
                            node.MoveTo(parent);
                            Nodes[number].Parent = parentNumber;
                        }
                    }
                    else if (kind == "Type")
                    {
                        Data.DefineType(record["Name"].AsText(), DecodeValues(record["Values"]));
                    }
                    else if (kind == "Assign")
                    {
                        Renumber(record["Local"].AsInteger(), number);
                    }
                }
                if (removed)
                {
                    Sweep(nullptr);
                }
            }

            // The server gave a node this client added its share number.
            void Renumber(std::int64_t local, std::int64_t number)
            {
                auto found = Nodes.find(local);
                if (found == Nodes.end())
                {
                    return;
                }
                Entry entry = found->second;
                Nodes.erase(found);
                Nodes[number] = entry;
                ShareNumbers[entry.Identifier] = number;
                for (auto& [other, child] : Nodes)
                {
                    if (child.Parent == local)
                    {
                        child.Parent = number;
                    }
                }
            }

            Member* FindMember(const Connection& client)
            {
                for (Member& member : Members)
                {
                    if (member.Link == client)
                    {
                        return &member;
                    }
                }
                return nullptr;
            }

            bool Owns(const Connection& client, std::int64_t number) const
            {
                auto owner = Owners.find(number);
                return owner != Owners.end() && owner->second == client;
            }

            // The server applying a client's changes. What it accepts goes in
            // `relay` for the other clients; what it refuses is answered in
            // `corrections` with how things really are.
            void ApplyFromClient(const std::vector<Message>& records, Member& member, std::vector<Message>& relay,
                std::vector<Message>& corrections)
            {
                auto resolve = [&member](std::int64_t number) {
                    if (number >= 0)
                    {
                        return number;
                    }
                    auto found = member.Assigned.find(number);
                    return found == member.Assigned.end() ? std::int64_t { 0 } : found->second;
                };

                bool removed = false;
                for (const Message& record : records)
                {
                    std::string kind = record["Kind"].AsText();
                    std::int64_t sent = record["Node"].AsInteger();
                    std::int64_t number = resolve(sent);
                    Node node = NodeNumbered(number);
                    bool owned = node && Owns(member.Link, number);

                    if (kind == "Add")
                    {
                        // Clients number their new nodes below zero.
                        std::int64_t parentNumber = resolve(record["Parent"].AsInteger());
                        Node parent = NodeNumbered(parentNumber);
                        if (sent >= 0 || member.Assigned.contains(sent) || (parentNumber != 0 && !parent))
                        {
                            continue;
                        }
                        std::string name = record["Name"].AsText();
                        std::string type = record["Type"].AsText();
                        PropertyValues values = DecodeValues(record["Values"]);
                        Node added = parent ? parent.Add(name, type, values) : Data.Add(name, type, values);
                        std::int64_t given = Register(added, parentNumber);
                        Owners[given] = member.Link;
                        member.Assigned[sent] = given;
                        relay.push_back(AddRecord(given, parentNumber, added));
                        corrections.push_back(Message { { "Kind", "Assign" }, { "Local", sent }, { "Node", given } });
                    }
                    else if (kind == "Remove")
                    {
                        if (owned)
                        {
                            node.Remove();
                            removed = true;
                            relay.push_back(Message { { "Kind", "Remove" }, { "Node", number } });
                        }
                        else if (node)
                        {
                            WriteWhole(node, corrections);
                        }
                    }
                    else if (kind == "Set" && node)
                    {
                        std::string property = record["Property"].AsText();
                        if (owned)
                        {
                            DataValue value = MessageToData(record["Value"]);
                            node.Set(property, value);
                            relay.push_back(Message {
                                { "Kind", "Set" }, { "Node", number }, { "Property", property }, { "Value", DataToMessage(value) } });
                        }
                        else
                        {
                            bool own = false;
                            for (const std::string& name : node.Properties())
                            {
                                own = own || name == property;
                            }
                            corrections.push_back(Message {
                                { "Kind", "Set" },
                                { "Node", number },
                                { "Property", property },
                                { "Value", own ? DataToMessage(node.Get(property)) : MessageValue() },
                            });
                        }
                    }
                    else if (kind == "Rename" && node)
                    {
                        std::string name = owned ? record["Name"].AsText() : node.Name();
                        if (owned)
                        {
                            node.Rename(name);
                        }
                        (owned ? relay : corrections).push_back(Message { { "Kind", "Rename" }, { "Node", number }, { "Name", name } });
                    }
                    else if (kind == "Move" && node)
                    {
                        std::int64_t parentNumber = resolve(record["Parent"].AsInteger());
                        Node parent = NodeNumbered(parentNumber);
                        if (owned && (parentNumber == 0 || parent))
                        {
                            node.MoveTo(parent);
                            Nodes[number].Parent = parentNumber;
                            relay.push_back(Message { { "Kind", "Move" }, { "Node", number }, { "Parent", parentNumber } });
                        }
                        else
                        {
                            Node current = node.Parent();
                            corrections.push_back(Message {
                                { "Kind", "Move" }, { "Node", number }, { "Parent", current ? NumberOf(current) : 0 } });
                        }
                    }
                    else if (kind == "Type")
                    {
                        std::string name = record["Name"].AsText();
                        for (const std::string& type : Data.TypeNames())
                        {
                            if (type == name)
                            {
                                corrections.push_back(TypeRecord(name));
                            }
                        }
                    }
                }
                if (removed)
                {
                    Sweep(nullptr);
                }
            }

            std::vector<Connection> Everyone(const Connection* except = nullptr) const
            {
                std::vector<Connection> everyone;
                for (const Member& member : Members)
                {
                    if (!except || !(member.Link == *except))
                    {
                        everyone.push_back(member.Link);
                    }
                }
                return everyone;
            }

            void Queue(std::vector<Message> records, std::vector<Connection> recipients)
            {
                if (!records.empty() && !recipients.empty())
                {
                    Outgoing.push_back({ std::move(records), std::move(recipients) });
                }
            }

            void SendSnapshot(const Connection& client) const
            {
                client.Send(Named("snapshot"), { { "Records", PackRecords(Snapshot()) }, { "TwoWay", TwoWay } });
            }

            // The server, once an update: its own changes, and everything queued, to each client.
            void ServerAfterUpdate()
            {
                std::erase_if(Members, [](const Member& member) { return !member.Link; });
                std::erase_if(Owners, [](const auto& owner) { return !owner.second; });

                // Too many changes since last time to replay: everyone starts over.
                if (Data.OldestVersion() > LastVersion + 1)
                {
                    LastVersion = Data.Version();
                    Outgoing.clear();
                    RegisterAll();
                    Sweep(nullptr);
                    for (const Member& member : Members)
                    {
                        SendSnapshot(member.Link);
                    }
                    return;
                }

                Queue(CollectChanges(), Everyone());
                for (const Member& member : Members)
                {
                    std::vector<Message> records;
                    for (const Chunk& chunk : Outgoing)
                    {
                        for (const Connection& recipient : chunk.Recipients)
                        {
                            if (recipient == member.Link)
                            {
                                records.insert(records.end(), chunk.Records.begin(), chunk.Records.end());
                                break;
                            }
                        }
                    }
                    if (!records.empty())
                    {
                        member.Link.Send(Named("changes"), { { "Records", PackRecords(records) } });
                    }
                }
                Outgoing.clear();
            }

            void Join(const Connection& client)
            {
                Queue(CollectChanges(), Everyone(&client));
                SendSnapshot(client);
                if (Member* member = FindMember(client))
                {
                    member->Assigned.clear();
                }
                else
                {
                    Members.push_back({ client, {} });
                }
            }

            void ChangesFromClient(const Connection& client, const Message& message)
            {
                Member* member = FindMember(client);
                if (!member || !TwoWay)
                {
                    return;
                }
                Queue(CollectChanges(), Everyone());
                std::vector<Message> relay;
                std::vector<Message> corrections;
                ApplyFromClient(UnpackRecords(message["Records"]), *member, relay, corrections);
                LastVersion = Data.Version();
                Queue(std::move(relay), Everyone(&client));
                Queue(std::move(corrections), { client });
            }

            void SnapshotFromServer(const Message& message)
            {
                for (const Node& node : Data.TopNodes())
                {
                    node.Remove();
                }
                Nodes.clear();
                ShareNumbers.clear();
                Pending.clear();
                TwoWay = message["TwoWay"].AsBoolean();
                ApplyFromServer(UnpackRecords(message["Records"]));
                LastVersion = Data.Version();
                Ready = true;
            }

            void ChangesFromServer(const Message& message)
            {
                if (!Ready)
                {
                    return;
                }
                // The client's own changes so far are kept for sending, and what
                // the server sent is not sent back.
                if (TwoWay)
                {
                    std::vector<Message> own = CollectChanges();
                    Pending.insert(Pending.end(), own.begin(), own.end());
                }
                ApplyFromServer(UnpackRecords(message["Records"]));
                LastVersion = Data.Version();
            }

            void ClientAfterUpdate()
            {
                if (!Ready || !TwoWay)
                {
                    LastVersion = Data.Version();
                    return;
                }
                std::vector<Message> own = CollectChanges();
                Pending.insert(Pending.end(), own.begin(), own.end());
                if (!Pending.empty())
                {
                    ToServer.Send(Named("changes"), { { "Records", PackRecords(Pending) } });
                    Pending.clear();
                }
            }
        };
    }

    // A table being shared. Sharing goes on whether or not this handle is kept.
    class TableShare
    {
    public:
        // Shares nothing. Tests as false.
        TableShare() = default;

        explicit TableShare(std::shared_ptr<internal::bridges::ShareState> state) : State(std::move(state)) {}

        explicit operator bool() const { return State && !State->Stopped; }

        // Stops sending and receiving changes. The table stays as it is.
        void Stop() const
        {
            if (!State || State->Stopped)
            {
                return;
            }
            State->Stopped = true;
            if (!State->IsServer)
            {
                State->ToServer.Send(State->Named("leave"));
            }
        }

        // Sends the changes made so far at once, instead of during the next
        // update, so that a message or request sent next arrives after them.
        void SendChanges() const
        {
            if (!State || State->Stopped)
            {
                return;
            }
            if (State->IsServer)
            {
                State->ServerAfterUpdate();
            }
            else
            {
                State->ClientAfterUpdate();
            }
        }

        // On a client, true once the server's table has arrived. On a server, true.
        bool IsReady() const { return State && !State->Stopped && (State->IsServer || State->Ready); }

        Sharing Mode() const { return State && State->TwoWay ? Sharing::TwoWay : Sharing::OneWay; }

        // On a server sharing two ways: the client that added a node, or no
        // connection for the server's own nodes. A client's nodes stay when it
        // leaves and become the server's after the next update, so they can be
        // removed in the server's OnDisconnected handler:
        //
        //     for (Node node : shared.NodesOwnedBy(client)) { node.Remove(); }
        Connection Owner(const Node& node) const
        {
            if (!State)
            {
                return {};
            }
            auto owner = State->Owners.find(State->NumberOf(node));
            return owner == State->Owners.end() ? Connection() : owner->second;
        }

        std::vector<Node> NodesOwnedBy(const Connection& client) const
        {
            std::vector<Node> nodes;
            if (!State)
            {
                return nodes;
            }
            for (const auto& [number, owner] : State->Owners)
            {
                if (owner == client)
                {
                    if (Node node = State->NodeNumbered(number))
                    {
                        nodes.push_back(node);
                    }
                }
            }
            return nodes;
        }

    private:
        std::shared_ptr<internal::bridges::ShareState> State;
    };

    // Shares the table with the server's clients that share theirs under the
    // same name.
    inline TableShare Share(const Table& table, const Server& server, Sharing sharing = Sharing::OneWay,
        std::string_view name = "table")
    {
        auto state = std::make_shared<internal::bridges::ShareState>(table, true, std::string(name));
        state->TwoWay = sharing == Sharing::TwoWay;
        state->Ready = true;
        state->RegisterAll();

        server.OnMessage(state->Named("join"), [state](Connection from, const Message&) {
            if (!state->Stopped)
            {
                state->Join(from);
            }
        });
        server.OnMessage(state->Named("leave"), [state](Connection from, const Message&) {
            std::erase_if(state->Members, [&](const auto& member) { return member.Link == from; });
        });
        server.OnMessage(state->Named("changes"), [state](Connection from, const Message& message) {
            if (!state->Stopped)
            {
                state->ChangesFromClient(from, message);
            }
        });
        server.AfterUpdate([state] {
            if (!state->Stopped)
            {
                state->ServerAfterUpdate();
            }
        });

        // Clients that started sharing first ask again.
        server.SendToAll(state->Named("announce"));
        return TableShare(state);
    }

    // Replaces the table's nodes with the server's table shared under the same
    // name, and keeps it up to date. Whether changes go both ways is the server's choice.
    inline TableShare Share(const Table& table, const Client& client, std::string_view name = "table")
    {
        auto state = std::make_shared<internal::bridges::ShareState>(table, false, std::string(name));
        state->ToServer = client.Server();

        client.OnMessage(state->Named("snapshot"), [state](const Message& message) {
            if (!state->Stopped)
            {
                state->SnapshotFromServer(message);
            }
        });
        client.OnMessage(state->Named("changes"), [state](const Message& message) {
            if (!state->Stopped)
            {
                state->ChangesFromServer(message);
            }
        });
        client.OnMessage(state->Named("announce"), [state](const Message&) {
            if (!state->Stopped)
            {
                state->ToServer.Send(state->Named("join"));
            }
        });
        client.AfterUpdate([state] {
            if (!state->Stopped)
            {
                state->ClientAfterUpdate();
            }
        });
        state->ToServer.Send(state->Named("join"));
        return TableShare(state);
    }
}
