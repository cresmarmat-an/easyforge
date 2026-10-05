#include "Benchmark.h"

#include <chrono>
#include <format>
#include <string>
#include <thread>

#include <easyforge/core/Clock.h>
#include <easyforge/network.h>

using namespace easyforge;

namespace
{
    struct Pair
    {
        Server Host;
        Client Guest;
    };

    Pair Connect(NetworkConditions conditions)
    {
        Pair pair;
        pair.Host = Server::New({ .Port = 0, .ThisComputerOnly = true, .Conditions = conditions });
        pair.Guest = Client::New({ .Address = "127.0.0.1", .Port = pair.Host.Port(), .Conditions = conditions });
        Clock clock;
        while (!pair.Guest.IsConnected() && clock.Seconds() < 5.0)
        {
            pair.Host.Update();
            pair.Guest.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return pair;
    }

    // Sends `count` reliable messages of `size` bytes from the client and
    // updates both ends until the server has them all. Gives the seconds taken,
    // or a negative number when they did not all arrive within a minute.
    double Deliver(Pair& pair, int count, std::size_t size)
    {
        int heard = 0;
        pair.Host.OnMessage("Data", [&](Connection, const Message&) { ++heard; });
        std::string payload(size, 'x');
        Clock clock;
        for (int index = 0; index < count; ++index)
        {
            pair.Guest.Send("Data", { { "Index", index }, { "Payload", payload } });
        }
        while (heard < count)
        {
            if (clock.Seconds() > 60.0)
            {
                return -1.0;
            }
            pair.Guest.Update();
            pair.Host.Update();
        }
        return clock.Seconds();
    }
}

EASYFORGE_BENCHMARK(Network)
{
    for (float loss : { 0.0f, 0.05f, 0.2f })
    {
        Pair pair = Connect({ .Loss = loss });
        if (!pair.Guest.IsConnected())
        {
            benchmarks::Note(std::format("could not connect with {:.0f}% loss", loss * 100.0f));
            continue;
        }
        double seconds = Deliver(pair, 20'000, 1'000);
        if (seconds < 0.0)
        {
            benchmarks::Note(std::format("20,000 messages did not all arrive within a minute with {:.0f}% loss", loss * 100.0f));
            continue;
        }
        benchmarks::Note(std::format("20,000 reliable messages of 1,000 bytes with {:>2.0f}% loss: {:.3f} s, {:.1f} megabytes a second",
            loss * 100.0f, seconds, 20'000.0 * 1'000.0 / seconds / 1'000'000.0));
    }

    Pair pair = Connect({});
    benchmarks::Measure("send 1,000 small messages and deliver them", 20, [&] { (void)Deliver(pair, 1'000, 100); });
}
