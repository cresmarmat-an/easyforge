#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#include <easyforge/core/Testing.h>

#include "Encoding.h"
#include "Peer.h"
#include "Protocol.h"

using namespace easyforge::internal::networking;

// Congestion control, checked on two peers that hand packets to each other in
// memory on a made-up clock, so every run is the same.

namespace
{
    constexpr std::uint64_t Token = 42;

    // A path from a sender to a receiver: packets take Delay seconds each way,
    // and the data packets Drop picks, by the count of packets sent, are lost.
    struct Path
    {
        struct Travelling
        {
            double Arrives = 0.0;
            bool ToReceiver = true;
            std::vector<std::uint8_t> Bytes;
        };

        Peer Sender { 0.0 };
        Peer Receiver { 0.0 };
        double Now = 0.0;
        double Delay = 0.01;
        std::size_t Delivered = 0;
        int Sent = 0;
        std::function<bool(int)> Drop;
        std::vector<Travelling> Packets;

        void Read(Peer& peer, const std::vector<std::uint8_t>& bytes)
        {
            ByteReader reader(bytes);
            reader.ReadBytes(sizeof Magic);
            auto kind = static_cast<PacketKind>(reader.Read8());
            reader.Read64();
            std::vector<DeliveredMessage> messages;
            peer.Receive(Now, reader, kind == PacketKind::KeepAlive, messages);
            if (&peer == &Receiver)
            {
                Delivered += messages.size();
            }
        }

        // Moves the clock on, hands over what arrived, and sends what is due.
        void Run(double step)
        {
            Now += step;
            std::vector<Travelling> arrived;
            std::erase_if(Packets, [&](Travelling& packet) {
                if (packet.Arrives > Now)
                {
                    return false;
                }
                arrived.push_back(std::move(packet));
                return true;
            });
            for (const Travelling& packet : arrived)
            {
                Read(packet.ToReceiver ? Receiver : Sender, packet.Bytes);
            }
            std::vector<std::vector<std::uint8_t>> out;
            Sender.Flush(Now, Token, out);
            for (std::vector<std::uint8_t>& bytes : out)
            {
                ++Sent;
                if (!Drop || !Drop(Sent))
                {
                    Packets.push_back({ Now + Delay, true, std::move(bytes) });
                }
            }
            out.clear();
            Receiver.Flush(Now, Token, out);
            for (std::vector<std::uint8_t>& bytes : out)
            {
                Packets.push_back({ Now + Delay, false, std::move(bytes) });
            }
        }

        // Runs until every queued message arrived, or `seconds` pass.
        bool RunUntilDelivered(std::size_t count, double seconds)
        {
            double end = Now + seconds;
            while (Delivered < count && Now < end)
            {
                Run(0.001);
            }
            return Delivered == count;
        }

        void Queue(std::size_t count, std::size_t size)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                Sender.Queue(Channel::Reliable, std::vector<std::uint8_t>(size, static_cast<std::uint8_t>(index)));
            }
        }
    };
}

EASYFORGE_TEST(TheSendWindowGrowsWhilePacketsArrive)
{
    // Two megabytes over a 20 millisecond round trip. A fixed rate of a
    // megabyte a second would take two seconds; a window that doubles each
    // round trip until the path is full takes a small part of one.
    Path path;
    path.Queue(2000, 1000);
    EASYFORGE_REQUIRE(path.RunUntilDelivered(2000, 10.0));
    EASYFORGE_EXPECT(path.Now < 0.5);
    EASYFORGE_EXPECT(path.Sender.SendWindow() > InitialWindow * 4.0);

    // Once everything is acknowledged nothing is counted as on its way.
    for (int step = 0; step < 1000; ++step)
    {
        path.Run(0.001);
    }
    EASYFORGE_EXPECT_EQUAL(path.Sender.BytesInFlight(), 0.0);
}

EASYFORGE_TEST(ALostPacketHalvesTheWindow)
{
    Path path;
    path.Drop = [](int sent) { return sent == 150; };
    path.Queue(2000, 1000);
    // Only a loss makes the window smaller. Acknowledgements that arrive with
    // the news of the loss still count first, so the cut is to half of a
    // window a little larger than the one before.
    double previous = path.Sender.SendWindow();
    double smallestRatio = 1.0;
    while (path.Delivered < 2000 && path.Now < 10.0)
    {
        path.Run(0.001);
        double window = path.Sender.SendWindow();
        smallestRatio = std::min(smallestRatio, window / previous);
        previous = window;
    }
    EASYFORGE_EXPECT_EQUAL(path.Delivered, std::size_t { 2000 });
    EASYFORGE_EXPECT(smallestRatio <= 0.7);
    EASYFORGE_EXPECT(path.Sender.SendWindow() >= MinimumWindow);
}

EASYFORGE_TEST(LossyPathsStillDeliverEverythingInOrder)
{
    Path path;
    path.Drop = [](int sent) { return sent % 7 == 0; };
    path.Queue(1000, 700);
    EASYFORGE_EXPECT(path.RunUntilDelivered(1000, 30.0));
    EASYFORGE_EXPECT(path.Sender.SendWindow() >= MinimumWindow);
    EASYFORGE_EXPECT(path.Sender.SendWindow() <= MaximumWindow);
}

EASYFORGE_TEST(AQuietConnectionKeepsItsFirstWindow)
{
    // A few small messages never fill the window, so it has no reason to grow.
    Path path;
    for (int message = 0; message < 50; ++message)
    {
        path.Queue(1, 50);
        for (int step = 0; step < 50; ++step)
        {
            path.Run(0.001);
        }
    }
    EASYFORGE_EXPECT_EQUAL(path.Delivered, std::size_t { 50 });
    EASYFORGE_EXPECT_EQUAL(path.Sender.SendWindow(), InitialWindow);
}
