#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <easyforge/core/Vector.h>
#include <easyforge/sound/Mixer.h>

#include "Engine.h"
#include "OutputDevice.h"

namespace easyforge::internal
{
    class MixerState;

    // Behind a PlayingSound.
    struct VoiceControl
    {
        std::weak_ptr<MixerState> Mixer;
        std::uint64_t Id = 0;
        std::shared_ptr<VoiceStatus> Status;
        std::atomic<bool> Stopped { false };
        std::atomic<bool> Paused { false };

        // The values last set, for reading back.
        mutable std::mutex Lock;
        float Volume = 1.0f;
        float Pan = 0.0f;
        float Pitch = 1.0f;
        Vector3 Position {};
    };

    // Behind a MixerBus.
    struct BusControl
    {
        std::weak_ptr<MixerState> Mixer;
        std::string Name;
        int Index = -1;

        mutable std::mutex Lock;
        float Volume = 1.0f;
        bool Muted = false;
        float LowPass = 0.0f;
        float HighPass = 0.0f;
        EchoSettings Echo;
    };

    // Everything a mixer is. The program's threads use it under ProgramLock;
    // the engine runs on the device's thread, or, offline, in Render under
    // RenderLock. Locks are only taken in the order ProgramLock, RenderLock,
    // FeedsLock.
    class MixerState final : public DeviceClient
    {
    public:
        MixerState(bool offline, int sampleRate, int channelCount);
        ~MixerState() override;

        MixerState(const MixerState&) = delete;
        MixerState& operator=(const MixerState&) = delete;

        // With ProgramLock held: hands a command to the engine.
        void Send(const Command& command);

        // With ProgramLock held: frees the voices the engine has finished with.
        void Reclaim();

        // With ProgramLock held: the bus with this name, made if it is new, or
        // nothing when there are already as many buses as there can be.
        std::shared_ptr<BusControl> BusNamed(std::string_view name);

        // Gives a stream to the streaming thread, starting it the first time.
        void AddFeed(std::shared_ptr<StreamFeed> feed);

        // Offline: mixes into `output`, `frames` frames.
        void RenderOffline(float* output, std::size_t frames);

        void DeviceFormat(int sampleRate, int channelCount, const std::string& name) override;
        void DeviceRender(float* output, int frames) override;

        std::string DeviceName() const;

        std::mutex ProgramLock;
        std::uint64_t NextVoiceId = 1;
        std::size_t LiveVoices = 0;
        std::map<std::string, std::shared_ptr<BusControl>, std::less<>> Buses;
        float MasterVolume = 1.0f;
        Vector3 ListenerPosition {};
        Vector3 ListenerForward { 0.0f, 0.0f, -1.0f };
        Vector3 ListenerUp { 0.0f, 1.0f, 0.0f };

        const bool Offline;
        bool Working = false;
        std::string ErrorText;
        std::atomic<int> Rate;
        std::atomic<int> Channels;

        std::weak_ptr<MixerState> Self;

        std::unique_ptr<OutputDevice> Device;

    private:
        void FillFeeds();
        void RunStreamer();

        CommandQueue Commands;
        FinishedQueue FinishedVoices;
        Engine TheEngine;
        std::mutex RenderLock;

        mutable std::mutex NameLock;
        std::string Name;

        std::mutex FeedsLock;
        std::vector<std::shared_ptr<StreamFeed>> Feeds;
        std::condition_variable StreamerWake;
        bool StopStreamer = false;
        std::thread Streamer;
    };
}
