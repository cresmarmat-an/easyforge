// Finding servers: one Discover packet to this computer and one to the whole
// local network, then every DiscoverReply that answers it.

#include <easyforge/network/Server.h>

#include <algorithm>
#include <array>
#include <chrono>

#include <easyforge/core/Random.h>

#include "Encoding.h"
#include "Protocol.h"
#include "Socket.h"

namespace easyforge
{
    using namespace internal::networking;

    std::vector<FoundServer> FindServers(const ServerSearchSettings& settings)
    {
        std::vector<FoundServer> found;
        std::string error;
        std::unique_ptr<UdpSocket> socket = OpenSocket(
            { .Port = 0, .ThisComputerOnly = settings.ThisComputerOnly, .Broadcast = !settings.ThisComputerOnly }, error);
        if (!socket)
        {
            return found;
        }

        Random random;
        std::uint64_t nonce = (static_cast<std::uint64_t>(random.Next()) << 32) | random.Next();
        std::vector<std::uint8_t> packet;
        ByteWriter writer(packet);
        writer.WriteBytes(Magic);
        writer.Write8(static_cast<std::uint8_t>(PacketKind::Discover));
        writer.Write64(nonce);
        socket->Send({ LoopbackHost, settings.Port }, packet);
        if (!settings.ThisComputerOnly)
        {
            socket->Send({ BroadcastHost, settings.Port }, packet);
        }

        // A server reached both ways answers twice; its instance number tells
        // the answers apart, and the network address is kept over loopback.
        std::vector<std::uint64_t> instances;
        auto start = std::chrono::steady_clock::now();
        std::array<std::uint8_t, 2048> buffer {};
        while (true)
        {
            double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            double remaining = static_cast<double>(settings.Seconds) - elapsed;
            if (remaining <= 0.0)
            {
                break;
            }
            socket->Wait(std::max(1, static_cast<int>(remaining * 1000.0)));

            SocketAddress from;
            while (std::optional<std::size_t> size = socket->Receive(from, buffer))
            {
                ByteReader reader(std::span<const std::uint8_t>(buffer.data(), *size));
                std::span<const std::uint8_t> magic = reader.ReadBytes(sizeof Magic);
                auto kind = static_cast<PacketKind>(reader.Read8());
                std::uint64_t answered = reader.Read64();
                std::uint64_t instance = reader.Read64();
                FoundServer server;
                server.Clients = reader.Read16();
                server.MaximumClients = reader.Read16();
                server.Port = reader.Read16();
                server.Name = reader.ReadShortText();
                server.Address = from.HostText();
                if (reader.Failed() || !std::ranges::equal(magic, Magic) || kind != PacketKind::DiscoverReply || answered != nonce)
                {
                    continue;
                }
                auto seen = std::ranges::find(instances, instance);
                if (seen == instances.end())
                {
                    instances.push_back(instance);
                    found.push_back(std::move(server));
                }
                else if (!from.IsLoopback())
                {
                    found[static_cast<std::size_t>(seen - instances.begin())].Address = server.Address;
                }
            }
        }
        return found;
    }
}
