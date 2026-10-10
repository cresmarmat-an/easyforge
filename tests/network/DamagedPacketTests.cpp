#include <cstdint>
#include <string>
#include <vector>

#include <easyforge/core/Random.h>
#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "Damage.h"
#include "NetworkTesting.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using namespace easyforge;
using namespace easyforge::testing;

// Whatever arrives, messages and packets that are damaged, cut short, made up,
// or repeated never crash or hang an end, and an end keeps serving others.

EASYFORGE_TEST(DamagedMessagesNeverCrash)
{
    Message message {
        { "Text", "hello \"there\"" },
        { "Number", -1234567890123 },
        { "Fraction", 0.125 },
        { "Flag", true },
        { "Place", Vector2 { 1.5f, -2.0f } },
        { "Direction", Vector3 { 0.0f, 1.0f, 0.0f } },
        { "Quarter", Vector4 { 1.0f, 2.0f, 3.0f, 4.0f } },
        { "Tint", Color::Hex("#66CCFF") },
        { "Bytes", std::vector<std::uint8_t> { 0, 1, 2, 255 } },
        { "Empty", "" },
    };
    Damage(
        message.Encode(),
        [](const std::vector<std::uint8_t>& damaged) {
            std::optional<Message> decoded = Message::Decode(damaged);
            if (decoded)
            {
                // Whatever was read writes back as bytes that read the same.
                std::optional<Message> again = Message::Decode(decoded->Encode());
                EASYFORGE_EXPECT(again.has_value());
                EASYFORGE_EXPECT(again && *again == *decoded);
            }
        },
        41, 2000);
}

#ifdef _WIN32
namespace
{
    // A UDP relay between one client and a server that damages, drops, and
    // makes up packets on the way, both ways.
    class DamagingRelay
    {
    public:
        explicit DamagingRelay(std::uint16_t serverPort) : Chance(77)
        {
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
            Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in address {};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            bind(Socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
            int length = sizeof(address);
            getsockname(Socket, reinterpret_cast<sockaddr*>(&address), &length);
            Port = ntohs(address.sin_port);
            u_long nonBlocking = 1;
            ioctlsocket(Socket, FIONBIO, &nonBlocking);
            Server.sin_family = AF_INET;
            Server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            Server.sin_port = htons(serverPort);
        }

        ~DamagingRelay()
        {
            closesocket(Socket);
            WSACleanup();
        }

        void Pass()
        {
            std::uint8_t buffer[2048];
            while (true)
            {
                sockaddr_in from {};
                int fromLength = sizeof(from);
                int size = recvfrom(Socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
                if (size < 0)
                {
                    break;
                }
                bool fromServer = from.sin_port == Server.sin_port;
                if (!fromServer)
                {
                    ClientAddress = from;
                    KnowClient = true;
                }
                if (!KnowClient)
                {
                    continue;
                }
                std::vector<std::uint8_t> packet(buffer, buffer + size);
                const sockaddr_in& to = fromServer ? ClientAddress : Server;
                if (Chance.Chance(0.05f))
                {
                    continue;
                }
                if (Chance.Chance(0.25f))
                {
                    Damage(packet);
                }
                Send(packet, to);
                if (Chance.Chance(0.05f))
                {
                    // The same packet again, later in the stream.
                    Send(packet, to);
                }
            }
            if (KnowClient && Chance.Chance(0.2f))
            {
                // Something made up, sometimes with the right first bytes.
                std::vector<std::uint8_t> madeUp(static_cast<std::size_t>(Chance.IntegerBetween(0, 1300)));
                for (std::uint8_t& value : madeUp)
                {
                    value = static_cast<std::uint8_t>(Chance.Next());
                }
                if (madeUp.size() >= 5 && Chance.Chance(0.7f))
                {
                    madeUp[0] = 'E';
                    madeUp[1] = 'F';
                    madeUp[2] = 'N';
                    madeUp[3] = '3';
                    madeUp[4] = static_cast<std::uint8_t>(Chance.IntegerBetween(0, 11));
                }
                Send(madeUp, Chance.Chance(0.5f) ? Server : ClientAddress);
            }
        }

        std::uint16_t Port = 0;

    private:
        void Damage(std::vector<std::uint8_t>& packet)
        {
            int changes = Chance.IntegerBetween(1, 4);
            for (int change = 0; change < changes && !packet.empty(); ++change)
            {
                std::size_t place = static_cast<std::size_t>(Chance.IntegerBetween(0, static_cast<int>(packet.size()) - 1));
                switch (Chance.IntegerBetween(0, 2))
                {
                case 0: packet[place] = static_cast<std::uint8_t>(Chance.Next()); break;
                case 1: packet.resize(place); break;
                default: packet.insert(packet.begin() + static_cast<std::ptrdiff_t>(place), static_cast<std::uint8_t>(Chance.Next())); break;
                }
            }
        }

        void Send(const std::vector<std::uint8_t>& packet, const sockaddr_in& to)
        {
            sendto(Socket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                reinterpret_cast<const sockaddr*>(&to), sizeof(to));
        }

        SOCKET Socket = INVALID_SOCKET;
        sockaddr_in Server {};
        sockaddr_in ClientAddress {};
        bool KnowClient = false;
        Random Chance;
    };
}

EASYFORGE_TEST(DamagedPacketsNeverCrashEitherEnd)
{
    Server host = LocalServer();
    int messages = 0;
    host.OnMessage("Chat", [&](Connection, const Message&) { ++messages; });
    host.OnRequest("Echo", [](Connection, const Message& request) { return request; });
    DamagingRelay relay(host.Port());
    Client guest = Client::New({ .Address = "127.0.0.1", .Port = relay.Port, .Timeout = 2.0f });
    int replies = 0;
    UpdateFor(
        [&] {
            relay.Pass();
            host.Update();
            guest.Update();
            if (guest.IsConnected())
            {
                guest.Send("Chat", { { "Text", "hello" }, { "Count", messages } });
                guest.Send("Chat", { { "Text", std::string(3000, 'x') } }, Delivery::ReliableUnordered);
                guest.Request("Echo", { { "Number", replies } }, [&](const Reply& reply) { replies += reply ? 1 : 0; });
                host.SendToAll("News", { { "Text", "from the host" } }, Delivery::Unreliable);
            }
        },
        3.0);

    // The server still serves a client that reaches it directly.
    Client direct = LocalClient(host);
    bool answered = false;
    EASYFORGE_REQUIRE(UpdateUntil(
        [&] {
            relay.Pass();
            host.Update();
            direct.Update();
            guest.Update();
        },
        [&] { return direct.IsConnected(); }));
    direct.Request("Echo", { { "Text", "still here" } }, [&](const Reply& reply) { answered = reply && reply.Message["Text"].AsText() == "still here"; });
    EASYFORGE_EXPECT(UpdateUntil(
        [&] {
            host.Update();
            direct.Update();
        },
        [&] { return answered; }));
}
#endif
