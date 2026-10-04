#include "MixerState.h"

#include <algorithm>
#include <chrono>

namespace easyforge::internal
{
    MixerState::MixerState(bool offline, int sampleRate, int channelCount)
        : Offline(offline), Rate(sampleRate), Channels(channelCount),
          TheEngine(Commands, FinishedVoices, sampleRate, channelCount)
    {
    }

    MixerState::~MixerState()
    {
        // The device's thread uses the engine, so it stops first.
        Device.reset();
        {
            std::lock_guard lock(FeedsLock);
            StopStreamer = true;
        }
        StreamerWake.notify_all();
        if (Streamer.joinable())
        {
            Streamer.join();
        }
        Voice* voice = nullptr;
        while (FinishedVoices.Pop(voice))
        {
            delete voice;
        }
    }

    void MixerState::Send(const Command& command)
    {
        while (!Commands.Push(command))
        {
            if (Offline)
            {
                // Nothing else takes commands from an offline mixer between
                // renders, so apply them here.
                std::lock_guard render(RenderLock);
                TheEngine.ApplyCommands();
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    void MixerState::Reclaim()
    {
        Voice* voice = nullptr;
        while (FinishedVoices.Pop(voice))
        {
            if (voice->Feed)
            {
                voice->Feed->Abandoned.store(true);
            }
            delete voice;
            --LiveVoices;
        }
    }

    std::shared_ptr<BusControl> MixerState::BusNamed(std::string_view name)
    {
        auto found = Buses.find(name);
        if (found != Buses.end())
        {
            return found->second;
        }
        auto control = std::make_shared<BusControl>();
        control->Mixer = Self;
        control->Name = std::string(name);
        if (Buses.size() < MaximumBuses && Working)
        {
            control->Index = static_cast<int>(Buses.size());
            auto bus = new BusEngine();
            PrepareBus(*bus, Rate.load());
            Command command;
            command.Kind = CommandKind::AddBus;
            command.Bus = control->Index;
            command.NewBus = bus;
            Send(command);
        }
        else if (Buses.size() >= MaximumBuses)
        {
            // Past the limit a bus still answers, but plays nothing of its own.
            return control;
        }
        Buses.emplace(control->Name, control);
        return control;
    }

    void MixerState::AddFeed(std::shared_ptr<StreamFeed> feed)
    {
        std::lock_guard lock(FeedsLock);
        Feeds.push_back(std::move(feed));
        if (!Offline && !Streamer.joinable())
        {
            Streamer = std::thread([this] { RunStreamer(); });
        }
    }

    void MixerState::FillFeeds()
    {
        std::lock_guard lock(FeedsLock);
        std::erase_if(Feeds, [](const std::shared_ptr<StreamFeed>& feed) { return feed->Abandoned.load(); });
        for (const std::shared_ptr<StreamFeed>& feed : Feeds)
        {
            feed->Fill();
        }
    }

    void MixerState::RunStreamer()
    {
        std::unique_lock lock(FeedsLock);
        while (!StopStreamer)
        {
            std::erase_if(Feeds, [](const std::shared_ptr<StreamFeed>& feed) { return feed->Abandoned.load(); });
            // Decoding happens without the lock, so plays never wait for it.
            std::vector<std::shared_ptr<StreamFeed>> feeds = Feeds;
            lock.unlock();
            for (const std::shared_ptr<StreamFeed>& feed : feeds)
            {
                feed->Fill();
            }
            feeds.clear();
            lock.lock();
            StreamerWake.wait_for(lock, std::chrono::milliseconds(20), [this] { return StopStreamer; });
        }
    }

    void MixerState::RenderOffline(float* output, std::size_t frames)
    {
        std::lock_guard render(RenderLock);
        TheEngine.ApplyCommands();
        int channels = Channels.load();
        while (frames > 0)
        {
            int block = static_cast<int>(std::min<std::size_t>(frames, BlockFrames));
            // Streams are topped up before every block, so offline mixing never
            // waits for a stream and gives the same result every time.
            FillFeeds();
            TheEngine.Render(output, block);
            output += static_cast<std::ptrdiff_t>(block) * channels;
            frames -= static_cast<std::size_t>(block);
        }
    }

    void MixerState::DeviceFormat(int sampleRate, int channelCount, const std::string& name)
    {
        TheEngine.SetFormat(sampleRate, channelCount);
        Rate.store(sampleRate);
        Channels.store(channelCount);
        std::lock_guard lock(NameLock);
        Name = name;
    }

    void MixerState::DeviceRender(float* output, int frames)
    {
        TheEngine.Render(output, frames);
    }

    std::string MixerState::DeviceName() const
    {
        std::lock_guard lock(NameLock);
        return Name;
    }
}
