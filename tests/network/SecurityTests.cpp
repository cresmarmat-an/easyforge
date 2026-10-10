#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <easyforge/core/Clock.h>
#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "Crypto.h"
#include "Encoding.h"
#include "Endpoint.h"
#include "NetworkTesting.h"
#include "Peer.h"
#include "Protocol.h"

using namespace easyforge;
using namespace easyforge::testing;
using namespace easyforge::internal::networking;

namespace
{
    std::vector<std::uint8_t> Bytes(std::string_view text)
    {
        return std::vector<std::uint8_t>(text.begin(), text.end());
    }

    // A plain UDP socket on this computer, to send packets to a server by hand
    // and see what comes back.
    class RawSocket
    {
    public:
        RawSocket()
        {
            WSADATA data {};
            WSAStartup(MAKEWORD(2, 2), &data);
            Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in local {};
            local.sin_family = AF_INET;
            local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            bind(Socket, reinterpret_cast<const sockaddr*>(&local), sizeof(local));
            u_long waitless = 1;
            ioctlsocket(Socket, FIONBIO, &waitless);
        }

        ~RawSocket()
        {
            closesocket(Socket);
            WSACleanup();
        }

        void Send(std::uint16_t port, const std::vector<std::uint8_t>& packet)
        {
            sockaddr_in to {};
            to.sin_family = AF_INET;
            to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            to.sin_port = htons(port);
            sendto(Socket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                reinterpret_cast<const sockaddr*>(&to), sizeof(to));
        }

        // The port this socket listens on.
        std::uint16_t Port() const
        {
            sockaddr_in local {};
            int size = sizeof(local);
            getsockname(Socket, reinterpret_cast<sockaddr*>(&local), &size);
            return ntohs(local.sin_port);
        }

        // Sends to whoever sent the last packet that arrived.
        void Reply(const std::vector<std::uint8_t>& packet)
        {
            Send(LastSender, packet);
        }

        // Every packet that arrives while `update` runs for `seconds`.
        std::vector<std::vector<std::uint8_t>> Gather(const std::function<void()>& update, double seconds)
        {
            std::vector<std::vector<std::uint8_t>> arrived;
            Clock clock;
            while (clock.Seconds() < seconds)
            {
                update();
                char buffer[2048];
                sockaddr_in from {};
                int fromSize = sizeof(from);
                int size = recvfrom(Socket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromSize);
                if (size > 0)
                {
                    arrived.emplace_back(buffer, buffer + size);
                    LastSender = ntohs(from.sin_port);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return arrived;
        }

    private:
        SOCKET Socket = INVALID_SOCKET;
        std::uint16_t LastSender = 0;
    };

    std::vector<std::uint8_t> Packet(PacketKind kind, std::uint64_t salt, std::size_t size = 0)
    {
        std::vector<std::uint8_t> packet;
        ByteWriter writer(packet);
        writer.WriteBytes(Magic);
        writer.Write8(static_cast<std::uint8_t>(kind));
        writer.Write64(salt);
        if (packet.size() < size)
        {
            packet.resize(size, 0);
        }
        return packet;
    }
}

// ---- The system's cryptography, as the network uses it ------------------------------

EASYFORGE_TEST(SealedBytesOpenOnlyUnchanged)
{
    SecretKey key {};
    EASYFORGE_REQUIRE(RandomBytes(key));
    Sealer sealer;
    EASYFORGE_REQUIRE(sealer.SetKey(key));
    std::vector<std::uint8_t> header = Bytes("header");
    std::vector<std::uint8_t> plain = Bytes("the message itself");
    std::vector<std::uint8_t> sealed;
    EASYFORGE_REQUIRE(sealer.Seal(7, header, plain, sealed));
    EASYFORGE_EXPECT_EQUAL(sealed.size(), plain.size() + TagSize);
    EASYFORGE_EXPECT(sealed != plain);

    std::vector<std::uint8_t> opened;
    EASYFORGE_REQUIRE(sealer.Open(7, header, sealed, opened));
    EASYFORGE_EXPECT(opened == plain);

    // Any change, anywhere, and it does not open.
    for (std::size_t place = 0; place < sealed.size(); ++place)
    {
        std::vector<std::uint8_t> changed = sealed;
        changed[place] ^= 1;
        EASYFORGE_EXPECT(!sealer.Open(7, header, changed, opened));
    }
    EASYFORGE_EXPECT(!sealer.Open(8, header, sealed, opened));
    EASYFORGE_EXPECT(!sealer.Open(7, Bytes("other"), sealed, opened));
    SecretKey other {};
    other[0] = 1;
    Sealer stranger;
    EASYFORGE_REQUIRE(stranger.SetKey(other));
    EASYFORGE_EXPECT(!stranger.Open(7, header, sealed, opened));
}

EASYFORGE_TEST(BothEndsAgreeOnOneSecret)
{
    KeyExchange first;
    KeyExchange second;
    EASYFORGE_REQUIRE(first && second);
    SecretKey fromFirst {};
    SecretKey fromSecond {};
    EASYFORGE_REQUIRE(first.Agree(second.Public(), fromFirst));
    EASYFORGE_REQUIRE(second.Agree(first.Public(), fromSecond));
    EASYFORGE_EXPECT(fromFirst == fromSecond);
    EASYFORGE_EXPECT(fromFirst != SecretKey {});

    // A key that is not a point on the curve is turned away.
    PublicKey nothing {};
    SecretKey shared {};
    EASYFORGE_EXPECT(!first.Agree(nothing, shared));
}

EASYFORGE_TEST(SignaturesProveTheKey)
{
    std::shared_ptr<SigningKey> key = SigningKey::Make();
    EASYFORGE_REQUIRE(key);
    std::vector<std::uint8_t> said = Bytes("both public keys");
    Signature signature {};
    EASYFORGE_REQUIRE(key->Sign(said, signature));
    EASYFORGE_EXPECT(Verify(key->Public(), said, signature));
    EASYFORGE_EXPECT(!Verify(key->Public(), Bytes("something else"), signature));
    std::shared_ptr<SigningKey> other = SigningKey::Make();
    EASYFORGE_EXPECT(!Verify(other->Public(), said, signature));

    // A key kept and read back is the same key.
    std::shared_ptr<SigningKey> again = SigningKey::FromPrivate(key->Private());
    EASYFORGE_REQUIRE(again);
    EASYFORGE_EXPECT(again->Public() == key->Public());
    Signature second {};
    EASYFORGE_REQUIRE(again->Sign(said, second));
    EASYFORGE_EXPECT(Verify(key->Public(), said, second));
}

EASYFORGE_TEST(KeysDeriveAsTheStandardSays)
{
    // The first test case of RFC 5869.
    std::vector<std::uint8_t> secret(22, 0x0b);
    std::vector<std::uint8_t> salt;
    for (std::uint8_t value = 0; value <= 0x0c; ++value)
    {
        salt.push_back(value);
    }
    std::string purpose;
    for (int value = 0xf0; value <= 0xf9; ++value)
    {
        purpose += static_cast<char>(value);
    }
    std::vector<std::uint8_t> output(42);
    DeriveKeys(secret, salt, purpose, output);
    EASYFORGE_EXPECT_EQUAL(HexText(output),
        std::string("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"));
}

// ---- The protocol ------------------------------------------------------------------

EASYFORGE_TEST(EachPacketCounterIsTakenOnce)
{
    ReplayWindow window;
    EASYFORGE_EXPECT(!window.IsFresh(0));
    EASYFORGE_EXPECT(window.IsFresh(1));
    window.Take(1);
    EASYFORGE_EXPECT(!window.IsFresh(1));
    window.Take(5);
    // Late arrivals within the window are still taken, once.
    EASYFORGE_EXPECT(window.IsFresh(3));
    window.Take(3);
    EASYFORGE_EXPECT(!window.IsFresh(3));
    EASYFORGE_EXPECT(!window.IsFresh(5));
    // Far behind the newest is too old.
    window.Take(5000);
    EASYFORGE_EXPECT(!window.IsFresh(5000 - ReplayWindowSize));
    EASYFORGE_EXPECT(window.IsFresh(5000 - ReplayWindowSize + 1));
}

EASYFORGE_TEST(UnfinishedMessagesHaveALimit)
{
    // Pieces that each claim to start a message of the most pieces there can
    // be, none ever finished: the receiving end notices once they hold more
    // than a connection may.
    Peer peer(0.0);
    std::uint16_t identifier = 0;
    for (int packet = 0; packet < 40 && !peer.OverBudget(); ++packet)
    {
        std::vector<std::uint8_t> bytes;
        ByteWriter writer(bytes);
        writer.Write32(0);
        writer.Write16(static_cast<std::uint16_t>(packet));
        writer.Write16(65535);
        writer.Write32(0);
        for (int entry = 0; entry < 20; ++entry)
        {
            writer.Write8(static_cast<std::uint8_t>(Channel::ReliableUnordered) | 4);
            writer.Write16(identifier++);
            writer.Write16(0);
            writer.Write16(static_cast<std::uint16_t>(FragmentLimit));
            writer.Write16(1);
            writer.Write8(7);
        }
        ByteReader reader(bytes);
        std::vector<DeliveredMessage> delivered;
        peer.Receive(0.01 * packet, reader, false, delivered);
    }
    EASYFORGE_EXPECT(peer.OverBudget());
}

// ---- Connections -------------------------------------------------------------------

EASYFORGE_TEST(SmallConnectRequestsGetNoAnswer)
{
    Server server = LocalServer();
    EASYFORGE_REQUIRE(server);
    RawSocket raw;

    raw.Send(server.Port(), Packet(PacketKind::ConnectRequest, 99));
    EASYFORGE_EXPECT(raw.Gather([&] { server.Update(); }, 0.3).empty());

    // A full-size request gets a small challenge, and nothing is kept for it.
    raw.Send(server.Port(), Packet(PacketKind::ConnectRequest, 99, ConnectRequestSize));
    std::vector<std::vector<std::uint8_t>> answers = raw.Gather([&] { server.Update(); }, 0.3);
    EASYFORGE_REQUIRE(answers.size() == 1);
    EASYFORGE_EXPECT(answers[0].size() < ConnectRequestSize);
    EASYFORGE_EXPECT_EQUAL(answers[0][4], static_cast<std::uint8_t>(PacketKind::ConnectChallenge));
    EASYFORGE_EXPECT(server.Connections().empty());
}

EASYFORGE_TEST(MadeUpCookiesGetNoPlace)
{
    Server server = LocalServer();
    EASYFORGE_REQUIRE(server);
    RawSocket raw;
    KeyExchange exchange;
    std::vector<std::uint8_t> response = Packet(PacketKind::ConnectResponse, 99);
    response.resize(response.size() + CookieSize, 0x5a);
    response.insert(response.end(), exchange.Public().begin(), exchange.Public().end());
    raw.Send(server.Port(), response);
    EASYFORGE_EXPECT(raw.Gather([&] { server.Update(); }, 0.3).empty());
    EASYFORGE_EXPECT(server.Connections().empty());
}

EASYFORGE_TEST(FindingServersNeverAnswersWithMore)
{
    Server server = LocalServer({ .Name = std::string(255, 'n') });
    EASYFORGE_REQUIRE(server);
    RawSocket raw;
    raw.Send(server.Port(), Packet(PacketKind::Discover, 5));
    EASYFORGE_EXPECT(raw.Gather([&] { server.Update(); }, 0.3).empty());
    raw.Send(server.Port(), Packet(PacketKind::Discover, 5, DiscoverRequestSize));
    std::vector<std::vector<std::uint8_t>> answers = raw.Gather([&] { server.Update(); }, 0.3);
    EASYFORGE_REQUIRE(answers.size() == 1);
    EASYFORGE_EXPECT(answers[0].size() <= DiscoverRequestSize);
}

EASYFORGE_TEST(ConnectAttemptsFromOneAddressAreLimited)
{
    Server server = LocalServer();
    EASYFORGE_REQUIRE(server);
    RawSocket raw;
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        raw.Send(server.Port(), Packet(PacketKind::ConnectRequest, 1000 + attempt, ConnectRequestSize));
    }
    std::vector<std::vector<std::uint8_t>> answers = raw.Gather([&] { server.Update(); }, 0.3);
    EASYFORGE_EXPECT(!answers.empty());
    EASYFORGE_EXPECT(answers.size() <= 30);
}

EASYFORGE_TEST(ClientsConnectOnlyToTheServerTheyPin)
{
    NetworkIdentity identity = NetworkIdentity::New();
    EASYFORGE_REQUIRE(identity);
    EASYFORGE_EXPECT_EQUAL(identity.PublicKey().size(), std::size_t { 128 });

    Server server = LocalServer({ .Identity = identity });
    Client pinned = LocalClient(server, { .ServerKey = identity.PublicKey() });
    EASYFORGE_EXPECT(UpdateUntil([&] { server.Update(); pinned.Update(); }, [&] { return pinned.IsConnected(); }));

    // A server with another identity, or none, is turned down.
    for (NetworkIdentity other : { NetworkIdentity::New(), NetworkIdentity() })
    {
        Server impostor = LocalServer({ .Identity = other });
        Client wary = LocalClient(impostor, { .ServerKey = identity.PublicKey() });
        std::string reason;
        wary.OnDisconnected = [&](std::string why) { reason = why; };
        EASYFORGE_EXPECT(UpdateUntil([&] { impostor.Update(); wary.Update(); }, [&] { return !reason.empty(); }));
        EASYFORGE_EXPECT(!wary.IsConnected());
        EASYFORGE_EXPECT_EQUAL(reason, std::string("the server could not prove it is the one ServerKey names"));
    }

    Client broken = LocalClient(server, { .ServerKey = "not a key" });
    EASYFORGE_EXPECT(!broken);
}

EASYFORGE_TEST(ACookieTheServerNoLongerTakesIsAskedForAgain)
{
    // A stand-in server answers the first request with a cookie no server
    // would take, then only listens. The client asks for a new one.
    RawSocket standIn;
    Client client = Client::New({ .Address = "127.0.0.1", .Port = standIn.Port() });
    EASYFORGE_REQUIRE(client);
    std::vector<std::vector<std::uint8_t>> requests = standIn.Gather([&] { client.Update(); }, 0.15);
    EASYFORGE_REQUIRE(!requests.empty());
    EASYFORGE_REQUIRE(requests[0][4] == static_cast<std::uint8_t>(PacketKind::ConnectRequest));

    // The magic, the kind, and the client's salt, then the cookie.
    std::vector<std::uint8_t> challenge(requests[0].begin(), requests[0].begin() + 13);
    challenge[4] = static_cast<std::uint8_t>(PacketKind::ConnectChallenge);
    challenge.resize(challenge.size() + CookieSize, 0x33);
    standIn.Reply(challenge);

    std::vector<std::vector<std::uint8_t>> later = standIn.Gather([&] { client.Update(); }, 1.0);
    bool responded = false;
    bool askedAgain = false;
    for (const std::vector<std::uint8_t>& packet : later)
    {
        responded = responded || packet[4] == static_cast<std::uint8_t>(PacketKind::ConnectResponse);
        askedAgain = askedAgain || (responded && packet[4] == static_cast<std::uint8_t>(PacketKind::ConnectRequest));
    }
    EASYFORGE_EXPECT(responded);
    EASYFORGE_EXPECT(askedAgain);
}

EASYFORGE_TEST(IdentitiesAreKeptInFiles)
{
    std::string path = std::string(EASYFORGE_TEST_OUTPUT) + "server.identity";
    NetworkIdentity identity = NetworkIdentity::New();
    EASYFORGE_REQUIRE(identity.Save(path));
    NetworkIdentity loaded = NetworkIdentity::Load(path);
    EASYFORGE_REQUIRE(loaded);
    EASYFORGE_EXPECT_EQUAL(loaded.PublicKey(), identity.PublicKey());

    NetworkIdentity missing = NetworkIdentity::Load(std::string(EASYFORGE_TEST_OUTPUT) + "no such file.identity");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(!missing.Error().empty());
}
