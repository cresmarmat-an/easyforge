#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <easyforge/core/Log.h>
#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "NetworkTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    // Ten percent of packets lost both ways, and 50 ms of delay that varies
    // enough to put some packets out of order.
    constexpr NetworkConditions PoorNetwork { .Loss = 0.1f, .Latency = 0.05f, .Jitter = 0.02f };
}

EASYFORGE_TEST(ReliableMessagesArriveOnceInOrder)
{
    ConnectedPair pair = Connect(PoorNetwork);
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    std::vector<int> atServer;
    std::vector<int> atClient;
    pair.Host.OnMessage("Count", [&](Connection, const Message& message) { atServer.push_back(message["Number"]); });
    pair.Guest.OnMessage("Count", [&](const Message& message) { atClient.push_back(message["Number"]); });

    constexpr int count = 300;
    Connection guest = pair.Host.Connections()[0];
    for (int number = 0; number < count; ++number)
    {
        pair.Guest.Send("Count", { { "Number", number } });
        guest.Send("Count", { { "Number", number } });
        if (number % 10 == 0)
        {
            pair.Update();
        }
    }
    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Update(); }, [&] { return atServer.size() >= count && atClient.size() >= count; }, 20.0));

    // Nothing more arrives later: no repeats.
    UpdateFor([&] { pair.Update(); }, 0.3);
    EASYFORGE_REQUIRE(atServer.size() == count && atClient.size() == count);
    for (int number = 0; number < count; ++number)
    {
        EASYFORGE_EXPECT_EQUAL(atServer[number], number);
        EASYFORGE_EXPECT_EQUAL(atClient[number], number);
    }
}

EASYFORGE_TEST(UnorderedMessagesArriveOnce)
{
    ConnectedPair pair = Connect(PoorNetwork);
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    std::multiset<int> arrived;
    pair.Host.OnMessage("Count", [&](Connection, const Message& message) { arrived.insert(message["Number"].As<int>()); });

    constexpr int count = 200;
    for (int number = 0; number < count; ++number)
    {
        pair.Guest.Send("Count", { { "Number", number } }, Delivery::ReliableUnordered);
    }
    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Update(); }, [&] { return arrived.size() >= count; }, 20.0));
    UpdateFor([&] { pair.Update(); }, 0.3);
    EASYFORGE_EXPECT_EQUAL(arrived.size(), std::size_t { count });
    for (int number = 0; number < count; ++number)
    {
        EASYFORGE_EXPECT_EQUAL(arrived.count(number), std::size_t { 1 });
    }
}

EASYFORGE_TEST(UnreliableMessagesKeepTheNewest)
{
    ConnectedPair pair = Connect(PoorNetwork);
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    std::vector<int> positions;
    std::vector<int> scores;
    pair.Host.OnMessage("Position", [&](Connection, const Message& message) { positions.push_back(message["Frame"]); });
    pair.Host.OnMessage("Score", [&](Connection, const Message& message) { scores.push_back(message["Frame"]); });

    // One of each a frame, the way a game sends them.
    constexpr int frames = 200;
    for (int frame = 0; frame < frames; ++frame)
    {
        pair.Guest.Send("Position", { { "Frame", frame } }, Delivery::Unreliable);
        pair.Guest.Send("Score", { { "Frame", frame } }, Delivery::Unreliable);
        pair.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    UpdateFor([&] { pair.Update(); }, 0.3);

    // Most arrive; an older one never follows a newer one of the same name.
    EASYFORGE_EXPECT(positions.size() > frames / 2);
    EASYFORGE_EXPECT(scores.size() > frames / 2);
    for (std::size_t index = 1; index < positions.size(); ++index)
    {
        EASYFORGE_EXPECT(positions[index] > positions[index - 1]);
    }
    for (std::size_t index = 1; index < scores.size(); ++index)
    {
        EASYFORGE_EXPECT(scores[index] > scores[index - 1]);
    }
}

EASYFORGE_TEST(LargeMessagesArriveWhole)
{
    ConnectedPair pair = Connect(PoorNetwork);
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    std::vector<std::uint8_t> picture(200 * 1024);
    for (std::size_t index = 0; index < picture.size(); ++index)
    {
        picture[index] = static_cast<std::uint8_t>(index * 31 + index / 977);
    }
    std::string longText(5000, 'x');

    std::vector<Message> arrived;
    pair.Host.OnMessage("Picture", [&](Connection, const Message& message) { arrived.push_back(message); });
    pair.Host.OnMessage("Note", [&](Connection, const Message& message) { arrived.push_back(message); });
    pair.Guest.Send("Picture", { { "Pixels", picture }, { "Width", 512 } });
    pair.Guest.Send("Picture", { { "Pixels", picture }, { "Width", 256 } }, Delivery::ReliableUnordered);

    // Too large for one packet, so it goes reliably after all.
    pair.Guest.Send("Note", { { "Text", longText } }, Delivery::Unreliable);

    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Update(); }, [&] { return arrived.size() >= 3; }, 20.0));
    int pictures = 0;
    for (const Message& message : arrived)
    {
        if (message.Has("Pixels"))
        {
            ++pictures;
            EASYFORGE_EXPECT(message["Pixels"].AsBytes() == picture);
        }
        else
        {
            EASYFORGE_EXPECT(message["Text"].AsText() == longText);
        }
    }
    EASYFORGE_EXPECT_EQUAL(pictures, 2);
}

EASYFORGE_TEST(OversizedMessagesAreNotSent)
{
    ConnectedPair pair = Connect();
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    std::vector<std::string> warnings;
    SetLogHandler([&](LogLevel level, std::string_view message) {
        if (level == LogLevel::Warning)
        {
            warnings.emplace_back(message);
        }
    });
    int arrived = 0;
    pair.Host.OnMessage("Huge", [&](Connection, const Message&) { ++arrived; });
    pair.Guest.Send("Huge", { { "Bytes", std::vector<std::uint8_t>(5 << 20) } });
    UpdateFor([&] { pair.Update(); }, 0.2);
    SetLogHandler(nullptr);

    EASYFORGE_EXPECT_EQUAL(arrived, 0);
    EASYFORGE_REQUIRE(warnings.size() == 1);
    EASYFORGE_EXPECT(warnings[0].find("\"Huge\" is larger than 4 MB") != std::string::npos);
}

EASYFORGE_TEST(MessagesWithoutHandlersAreDropped)
{
    ConnectedPair pair = Connect();
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());
    int arrived = 0;
    pair.Guest.Send("Unknown", { { "Value", 1 } });
    pair.Host.OnMessage("Known", [&](Connection, const Message&) { ++arrived; });
    pair.Guest.Send("Known");
    EASYFORGE_EXPECT(UpdateUntil([&] { pair.Update(); }, [&] { return arrived == 1; }));

    // Removing a handler.
    pair.Host.OnMessage("Known", nullptr);
    pair.Guest.Send("Known");
    UpdateFor([&] { pair.Update(); }, 0.2);
    EASYFORGE_EXPECT_EQUAL(arrived, 1);
}
