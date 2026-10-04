#include <string>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

#include "NetworkTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(RequestsGetAnswers)
{
    ConnectedPair pair = Connect();
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    pair.Host.OnRequest("Add", [](Connection, const Message& request) {
        return Message { { "Sum", request["First"].As<int>() + request["Second"].As<int>() } };
    });
    std::vector<Reply> replies;
    pair.Guest.Request("Add", { { "First", 2 }, { "Second", 3 } }, [&](const Reply& reply) { replies.push_back(reply); });
    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Update(); }, [&] { return !replies.empty(); }));
    EASYFORGE_EXPECT(replies[0]);
    EASYFORGE_EXPECT(replies[0].Error.empty());
    int sum = replies[0].Message["Sum"];
    EASYFORGE_EXPECT_EQUAL(sum, 5);

    // The server can ask a client the same way.
    pair.Guest.OnRequest("Name", [](const Message&) { return Message { { "Name", "Ari" } }; });
    std::string name;
    pair.Host.Connections()[0].Request("Name", {}, [&](const Reply& reply) { name = reply.Message["Name"].AsText(); });
    EASYFORGE_EXPECT(UpdateUntil([&] { pair.Update(); }, [&] { return !name.empty(); }));
    EASYFORGE_EXPECT_EQUAL(name, std::string("Ari"));
}

EASYFORGE_TEST(RefusedRequestsCarryTheReason)
{
    ConnectedPair pair = Connect();
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());

    pair.Host.OnRequest("Buy", [](Connection, const Message& request) -> Result<Message> {
        if (request["Item"].AsText() != "Sword")
        {
            return Failure("there is no such item");
        }
        return Message { { "Bought", true } };
    });
    std::vector<Reply> replies;
    auto keep = [&](const Reply& reply) { replies.push_back(reply); };
    pair.Guest.Request("Buy", { { "Item", "Dragon" } }, keep);
    pair.Guest.Request("Buy", { { "Item", "Sword" } }, keep);
    pair.Guest.Request("Sell", {}, keep);
    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Update(); }, [&] { return replies.size() == 3; }));

    // Replies come back in the order the requests went.
    EASYFORGE_EXPECT(!replies[0]);
    EASYFORGE_EXPECT_EQUAL(replies[0].Error, std::string("there is no such item"));
    EASYFORGE_EXPECT(replies[1]);
    EASYFORGE_EXPECT(replies[1].Message["Bought"].AsBoolean());
    EASYFORGE_EXPECT(!replies[2]);
    EASYFORGE_EXPECT_EQUAL(replies[2].Error, std::string("nothing answers \"Sell\" here"));
}

EASYFORGE_TEST(UnansweredRequestsTimeOut)
{
    ConnectedPair pair = Connect();
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());
    pair.Host.OnRequest("Slow", [](Connection, const Message&) { return Message {}; });

    // The server is not updated, so it never answers.
    std::string error;
    Clock clock;
    pair.Guest.Request("Slow", {}, [&](const Reply& reply) { error = reply.Error; }, { .Timeout = 0.2f });
    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Guest.Update(); }, [&] { return !error.empty(); }));
    EASYFORGE_EXPECT_EQUAL(error, std::string("the request timed out"));
    EASYFORGE_EXPECT(clock.Seconds() >= 0.2);

    // A late answer finds no callback and is ignored.
    UpdateFor([&] { pair.Update(); }, 0.2);
    EASYFORGE_EXPECT_EQUAL(error, std::string("the request timed out"));
}

EASYFORGE_TEST(RequestsFailWhenTheConnectionCloses)
{
    ConnectedPair pair = Connect();
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());
    pair.Host.OnRequest("Wait", [](Connection, const Message&) { return Message {}; });

    std::string error;
    pair.Guest.Request("Wait", {}, [&](const Reply& reply) { error = reply.Error; });
    pair.Guest.Disconnect();
    EASYFORGE_EXPECT_EQUAL(error, std::string("the connection closed"));

    // Asking once closed fails during the next update.
    std::string later;
    pair.Guest.Request("Wait", {}, [&](const Reply& reply) { later = reply.Error; });
    EASYFORGE_EXPECT(later.empty());
    pair.Guest.Update();
    EASYFORGE_EXPECT_EQUAL(later, std::string("not connected"));
}

EASYFORGE_TEST(EveryRequestIsAnsweredOnAPoorNetwork)
{
    ConnectedPair pair = Connect({ .Loss = 0.1f, .Latency = 0.05f, .Jitter = 0.02f });
    EASYFORGE_REQUIRE(pair.Guest.IsConnected());
    pair.Host.OnRequest("Double", [](Connection, const Message& request) {
        return Message { { "Value", request["Value"].As<int>() * 2 } };
    });

    constexpr int count = 60;
    std::vector<int> answers(count, -1);
    for (int index = 0; index < count; ++index)
    {
        pair.Guest.Request("Double", { { "Value", index } }, [&answers, index](const Reply& reply) {
            answers[index] = reply ? reply.Message["Value"].As<int>() : -2;
        });
    }
    auto answered = [&] {
        for (int answer : answers)
        {
            if (answer == -1)
            {
                return false;
            }
        }
        return true;
    };
    EASYFORGE_REQUIRE(UpdateUntil([&] { pair.Update(); }, answered, 20.0));
    for (int index = 0; index < count; ++index)
    {
        EASYFORGE_EXPECT_EQUAL(answers[index], index * 2);
    }
}
