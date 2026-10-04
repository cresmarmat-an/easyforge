#pragma once

#include <chrono>
#include <functional>
#include <thread>

#include <easyforge/core/Clock.h>
#include <easyforge/network.h>

namespace easyforge::testing
{
    // A server only this computer can reach, on a free port, so the system
    // firewall has nothing to ask about.
    inline Server LocalServer(ServerSettings settings = {})
    {
        settings.Port = 0;
        settings.ThisComputerOnly = true;
        return Server::New(settings);
    }

    inline Client LocalClient(const Server& server, ClientSettings settings = {})
    {
        settings.Address = "127.0.0.1";
        settings.Port = server.Port();
        return Client::New(settings);
    }

    // A server and one client.
    struct ConnectedPair
    {
        Server Host;
        Client Guest;

        void Update() const
        {
            Host.Update();
            Guest.Update();
        }
    };

    // Calls `update` until `done` is true, or until the time is up. Gives `done`.
    inline bool UpdateUntil(const std::function<void()>& update, const std::function<bool()>& done, double seconds = 5.0)
    {
        Clock clock;
        while (!done())
        {
            if (clock.Seconds() > seconds)
            {
                return false;
            }
            update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    // Keeps updating for a while, to see that nothing more happens.
    inline void UpdateFor(const std::function<void()>& update, double seconds)
    {
        Clock clock;
        while (clock.Seconds() < seconds)
        {
            update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Waits without updating, for threaded servers and clients.
    inline bool WaitUntil(const std::function<bool()>& done, double seconds = 5.0)
    {
        return UpdateUntil([] {}, done, seconds);
    }

    // A server and a client connected to it, both with the same simulated trouble.
    inline ConnectedPair Connect(NetworkConditions conditions = {})
    {
        ConnectedPair pair;
        pair.Host = LocalServer({ .Conditions = conditions });
        pair.Guest = LocalClient(pair.Host, { .Conditions = conditions });
        UpdateUntil([&] { pair.Update(); }, [&] { return pair.Guest.IsConnected() && pair.Host.Connections().size() == 1; });
        return pair;
    }
}
