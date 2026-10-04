#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <easyforge/core/Vector.h>
#include <easyforge/sound/Mixer.h>

#include "Filters.h"
#include "RingQueue.h"
#include "SoundSource.h"

// The part of the mixer that runs on the mixer thread. It never allocates,
// frees, or waits: voices and buses are made and freed by the program's
// threads and handed over through queues.

namespace easyforge::internal
{
    inline constexpr std::size_t MaximumVoices = 512;
    inline constexpr std::size_t MaximumBuses = 64;

    // Frames mixed at a time; longer requests are mixed in pieces.
    inline constexpr int BlockFrames = 1024;

    // The longest echo, in seconds.
    inline constexpr float LongestEcho = 2.0f;

    // Seconds over which a changed volume or pan slides to its new value, so
    // that changes do not click.
    inline constexpr float SmoothingSeconds = 0.01f;

    // What the mixer thread reports about a voice, read by PlayingSound.
    struct VoiceStatus
    {
        std::atomic<bool> Finished { false };
        std::atomic<double> Time { 0.0 };
    };

    // A file being decoded ahead of the voice that plays it, by the streaming
    // thread, into a ring of frames the mixer thread reads.
    struct StreamFeed
    {
        StreamFeed(SoundStream stream, bool loop, std::size_t capacityFrames);

        // On the streaming thread: decodes until the ring is full or the sound
        // ends.
        void Fill();

        // On the mixer thread.
        std::size_t Available() const;
        bool Take(float* frame);

        SoundStream Stream;
        bool Loop;
        int ChannelCount;
        std::size_t Capacity;
        std::vector<float> Ring;
        std::atomic<std::uint64_t> Written { 0 };
        std::atomic<std::uint64_t> Taken { 0 };
        std::atomic<bool> EndReached { false };

        // Set when the voice is gone, so the streaming thread lets go.
        std::atomic<bool> Abandoned { false };
    };

    // A value that slides to a target over a number of frames.
    struct Ramp
    {
        float Value = 1.0f;
        float Target = 1.0f;
        float Step = 0.0f;
        std::uint32_t FramesLeft = 0;

        void To(float target, std::uint32_t frames)
        {
            Target = target;
            FramesLeft = frames;
            if (frames == 0)
            {
                Value = target;
                Step = 0.0f;
            }
            else
            {
                Step = (target - Value) / static_cast<float>(frames);
            }
        }

        float Next()
        {
            if (FramesLeft > 0)
            {
                Value = --FramesLeft == 0 ? Target : Value + Step;
            }
            return Value;
        }

        bool Moving() const { return FramesLeft > 0; }
    };

    struct Voice
    {
        std::uint64_t Id = 0;
        std::shared_ptr<const SoundSource> Source;
        std::shared_ptr<VoiceStatus> Status;
        std::shared_ptr<StreamFeed> Feed;

        // Whole sounds read from here.
        const float* Samples = nullptr;
        std::uint64_t NextPull = 0;

        std::uint64_t FrameCount = 0;
        int ChannelCount = 1;
        int SourceRate = 0;
        bool Loop = false;

        // Four frames around the playing position for the interpolation: the
        // position is Window[1] plus Fraction of the way to Window[2].
        float Window[4][2] {};
        std::uint64_t Current = 0;
        double Fraction = 0.0;
        std::uint32_t PulledPastEnd = 0;

        int Bus = -1;
        float Pan = 0.0f;
        float Pitch = 1.0f;
        bool Positional = false;
        Vector3 Position {};
        float MinimumDistance = 1.0f;
        float MaximumDistance = 50.0f;
        StereoFilters Filters;
        float LowPass = 0.0f;
        float HighPass = 0.0f;

        // Left from the first channel, left from the second, right from the
        // first, right from the second: pan and place, slid between blocks.
        float Matrix[4] {};
        bool Started = false;
        Ramp Volume;
        Ramp StopGain;
        bool Stopping = false;
        bool Paused = false;
        bool Finished = false;
    };

    struct BusEngine
    {
        std::vector<float> Buffer;
        Ramp Volume;
        Ramp Mute;
        StereoFilters Filters;
        float LowPass = 0.0f;
        float HighPass = 0.0f;
        EchoSettings Echo;
        std::vector<float> EchoLine;
        std::size_t EchoAt = 0;
    };

    enum class CommandKind : std::uint8_t
    {
        AddVoice,
        VoiceVolume,
        VoicePan,
        VoicePitch,
        VoicePosition,
        VoicePause,
        VoiceResume,
        VoiceStop,
        StopAll,
        AddBus,
        BusVolume,
        BusMuted,
        BusFilters,
        BusEcho,
        MasterVolume,
        Listener,
    };

    struct Command
    {
        CommandKind Kind = CommandKind::AddVoice;
        std::uint64_t Voice = 0;
        int Bus = -1;
        float Value = 0.0f;
        float Other = 0.0f;
        float Seconds = 0.0f;
        Vector3 Vectors[3] {};
        EchoSettings Echo {};
        internal::Voice* NewVoice = nullptr;
        BusEngine* NewBus = nullptr;
    };

    using CommandQueue = RingQueue<Command, 4096>;
    using FinishedQueue = RingQueue<Voice*, MaximumVoices * 2>;

    // Prepares a voice to start: reads the first frames into its window. Runs
    // on the program's thread before the voice is handed over.
    void PrepareVoice(Voice& voice, std::uint64_t startFrame);

    // Makes a bus's buffers for a sample rate. On the program's thread, or the
    // mixer thread while the device is being opened again.
    void PrepareBus(BusEngine& bus, int sampleRate);

    class Engine
    {
    public:
        Engine(CommandQueue& commands, FinishedQueue& finished, int sampleRate, int channelCount);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Changes the rate the mixer works at, when the device changes. Not
        // while Render runs.
        void SetFormat(int sampleRate, int channelCount);
        int SampleRate() const { return Rate; }
        int ChannelCount() const { return Channels; }

        // Applies waiting commands, then mixes `frames` frames into `output`,
        // channels interleaved.
        void Render(float* output, int frames);

        // Applies waiting commands without mixing.
        void ApplyCommands();

    private:
        void Apply(const Command& command);
        Voice* FindVoice(std::uint64_t id) const;
        void MixBlock(float* output, int frames);
        void MixVoice(Voice& voice, float* destination, int frames);
        void TargetMatrix(const Voice& voice, float* matrix) const;
        void ProcessBus(BusEngine& bus, int frames);
        void Finish(Voice& voice);
        std::uint32_t Frames(float seconds) const;

        CommandQueue& Commands;
        FinishedQueue& FinishedVoices;
        int Rate;
        int Channels;
        std::vector<Voice*> Voices;
        std::vector<BusEngine*> Buses;
        std::vector<float> Master;
        Ramp MasterVolume;
        Vector3 ListenerPosition {};
        Vector3 ListenerRight { 1.0f, 0.0f, 0.0f };
    };
}
