#include "Engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <easyforge/core/Scalar.h>

namespace easyforge::internal
{
    namespace
    {
        // Reads the next frame of a voice's sound into `frame`: two samples, the
        // second 0 for a mono sound. Past the end, frames are silent.
        void Pull(Voice& voice, float* frame)
        {
            if (voice.Feed)
            {
                if (!voice.Feed->Take(frame))
                {
                    frame[0] = 0.0f;
                    frame[1] = 0.0f;
                    ++voice.PulledPastEnd;
                }
                return;
            }
            if (voice.NextPull >= voice.FrameCount)
            {
                if (!voice.Loop || voice.FrameCount == 0)
                {
                    frame[0] = 0.0f;
                    frame[1] = 0.0f;
                    ++voice.PulledPastEnd;
                    return;
                }
                voice.NextPull = 0;
            }
            const float* samples = voice.Samples + voice.NextPull * static_cast<std::uint64_t>(voice.ChannelCount);
            frame[0] = samples[0];
            frame[1] = voice.ChannelCount > 1 ? samples[1] : 0.0f;
            ++voice.NextPull;
        }

        void Advance(Voice& voice, double step)
        {
            voice.Fraction += step;
            while (voice.Fraction >= 1.0)
            {
                voice.Fraction -= 1.0;
                std::memmove(voice.Window[0], voice.Window[1], sizeof(float) * 6);
                Pull(voice, voice.Window[3]);
                if (++voice.Current >= voice.FrameCount && voice.Loop && voice.FrameCount > 0)
                {
                    voice.Current -= voice.FrameCount;
                }
            }
        }

        bool Ended(const Voice& voice)
        {
            if (voice.Loop)
            {
                return false;
            }
            if (voice.FrameCount > 0)
            {
                return voice.Current >= voice.FrameCount;
            }
            // A stream that did not know its length ends once its last frames
            // have passed through the window.
            return voice.Feed && voice.Feed->EndReached.load(std::memory_order_acquire) && voice.PulledPastEnd >= 3;
        }

        // Loud peaks bend toward 1 instead of being cut off; everything below 0.9
        // passes untouched.
        float SoftClip(float sample)
        {
            float magnitude = std::fabs(sample);
            if (magnitude <= 0.9f)
            {
                return sample;
            }
            return std::copysign(0.9f + 0.1f * std::tanh((magnitude - 0.9f) * 10.0f), sample);
        }
    }

    // ---- Streams -----------------------------------------------------------------

    StreamFeed::StreamFeed(SoundStream stream, bool loop, std::size_t capacityFrames)
        : Stream(std::move(stream)), Loop(loop), ChannelCount(std::max(Stream.ChannelCount(), 1)),
          Capacity(std::max<std::size_t>(capacityFrames, 64))
    {
        Ring.resize(Capacity * static_cast<std::size_t>(ChannelCount));
    }

    void StreamFeed::Fill()
    {
        std::uint64_t written = Written.load(std::memory_order_relaxed);
        bool seekedWithoutData = false;
        while (!EndReached.load(std::memory_order_relaxed))
        {
            std::size_t space = Capacity - static_cast<std::size_t>(written - Taken.load(std::memory_order_acquire));
            if (space == 0)
            {
                return;
            }
            std::size_t start = static_cast<std::size_t>(written % Capacity);
            std::size_t run = std::min(space, Capacity - start);
            std::size_t channels = static_cast<std::size_t>(ChannelCount);
            std::size_t read = Stream.Read(std::span<float>(Ring.data() + start * channels, run * channels));
            if (read == 0)
            {
                // A sound with no frames would loop forever without this.
                if (Loop && !seekedWithoutData && Stream.Seek(0))
                {
                    seekedWithoutData = true;
                    continue;
                }
                EndReached.store(true, std::memory_order_release);
                return;
            }
            seekedWithoutData = false;
            written += read;
            Written.store(written, std::memory_order_release);
        }
    }

    std::size_t StreamFeed::Available() const
    {
        return static_cast<std::size_t>(Written.load(std::memory_order_acquire) - Taken.load(std::memory_order_relaxed));
    }

    bool StreamFeed::Take(float* frame)
    {
        std::uint64_t taken = Taken.load(std::memory_order_relaxed);
        if (taken == Written.load(std::memory_order_acquire))
        {
            return false;
        }
        const float* samples = Ring.data() + static_cast<std::size_t>(taken % Capacity) * static_cast<std::size_t>(ChannelCount);
        frame[0] = samples[0];
        frame[1] = ChannelCount > 1 ? samples[1] : 0.0f;
        Taken.store(taken + 1, std::memory_order_release);
        return true;
    }

    // ---- Preparing -------------------------------------------------------------

    void PrepareVoice(Voice& voice, std::uint64_t startFrame)
    {
        voice.Current = startFrame;
        voice.Fraction = 0.0;
        voice.Window[0][0] = 0.0f;
        voice.Window[0][1] = 0.0f;
        if (!voice.Feed && voice.FrameCount > 0)
        {
            std::uint64_t before = startFrame > 0 ? startFrame - 1 : voice.Loop ? voice.FrameCount - 1 : voice.FrameCount;
            if (before < voice.FrameCount)
            {
                const float* samples = voice.Samples + before * static_cast<std::uint64_t>(voice.ChannelCount);
                voice.Window[0][0] = samples[0];
                voice.Window[0][1] = voice.ChannelCount > 1 ? samples[1] : 0.0f;
            }
            voice.NextPull = startFrame;
        }
        Pull(voice, voice.Window[1]);
        Pull(voice, voice.Window[2]);
        Pull(voice, voice.Window[3]);
    }

    void PrepareBus(BusEngine& bus, int sampleRate)
    {
        bus.Buffer.assign(static_cast<std::size_t>(BlockFrames) * 2, 0.0f);
        bus.EchoLine.assign(static_cast<std::size_t>(LongestEcho * static_cast<float>(sampleRate) + 1.0f) * 2, 0.0f);
        bus.EchoAt = 0;
        bus.Filters.Set(bus.LowPass, bus.HighPass, static_cast<float>(sampleRate));
    }

    // ---- The engine ------------------------------------------------------------

    Engine::Engine(CommandQueue& commands, FinishedQueue& finished, int sampleRate, int channelCount)
        : Commands(commands), FinishedVoices(finished), Rate(sampleRate), Channels(channelCount)
    {
        Voices.reserve(MaximumVoices);
        Buses.assign(MaximumBuses, nullptr);
        Master.assign(static_cast<std::size_t>(BlockFrames) * 2, 0.0f);
    }

    Engine::~Engine()
    {
        Command command;
        while (Commands.Pop(command))
        {
            delete command.NewVoice;
            delete command.NewBus;
        }
        for (Voice* voice : Voices)
        {
            if (voice->Feed)
            {
                voice->Feed->Abandoned.store(true);
            }
            delete voice;
        }
        for (BusEngine* bus : Buses)
        {
            delete bus;
        }
    }

    void Engine::SetFormat(int sampleRate, int channelCount)
    {
        Rate = sampleRate;
        Channels = channelCount;
        for (Voice* voice : Voices)
        {
            voice->Filters.Set(voice->LowPass, voice->HighPass, static_cast<float>(Rate));
        }
        for (BusEngine* bus : Buses)
        {
            if (bus)
            {
                PrepareBus(*bus, Rate);
            }
        }
    }

    std::uint32_t Engine::Frames(float seconds) const
    {
        return static_cast<std::uint32_t>(std::max(seconds, 0.0f) * static_cast<float>(Rate) + 0.5f);
    }

    Voice* Engine::FindVoice(std::uint64_t id) const
    {
        for (Voice* voice : Voices)
        {
            if (voice->Id == id)
            {
                return voice;
            }
        }
        return nullptr;
    }

    void Engine::ApplyCommands()
    {
        Command command;
        while (Commands.Pop(command))
        {
            Apply(command);
        }
    }

    void Engine::Apply(const Command& command)
    {
        BusEngine* bus = command.Bus >= 0 && command.Bus < static_cast<int>(Buses.size()) ? Buses[static_cast<std::size_t>(command.Bus)] : nullptr;
        switch (command.Kind)
        {
        case CommandKind::AddVoice:
        {
            Voice* voice = command.NewVoice;
            voice->Filters.Set(voice->LowPass, voice->HighPass, static_cast<float>(Rate));
            if (Voices.size() < MaximumVoices)
            {
                Voices.push_back(voice);
            }
            else
            {
                Finish(*voice);
            }
            return;
        }
        case CommandKind::AddBus:
            if (command.Bus >= 0 && command.Bus < static_cast<int>(Buses.size()) && !Buses[static_cast<std::size_t>(command.Bus)])
            {
                Buses[static_cast<std::size_t>(command.Bus)] = command.NewBus;
            }
            else
            {
                // Cannot happen: the program side numbers buses. Kept safe anyway.
                delete command.NewBus;
            }
            return;
        case CommandKind::BusVolume:
            if (bus)
            {
                bus->Volume.To(std::max(command.Value, 0.0f), Frames(std::max(command.Seconds, SmoothingSeconds)));
            }
            return;
        case CommandKind::BusMuted:
            if (bus)
            {
                bus->Mute.To(command.Value > 0.5f ? 0.0f : 1.0f, Frames(SmoothingSeconds));
            }
            return;
        case CommandKind::BusFilters:
            if (bus)
            {
                bus->LowPass = command.Value;
                bus->HighPass = command.Other;
                bus->Filters.Set(command.Value, command.Other, static_cast<float>(Rate));
            }
            return;
        case CommandKind::BusEcho:
            if (bus)
            {
                if (bus->Echo.Mix <= 0.0f && command.Echo.Mix > 0.0f)
                {
                    std::fill(bus->EchoLine.begin(), bus->EchoLine.end(), 0.0f);
                }
                bus->Echo = command.Echo;
            }
            return;
        case CommandKind::MasterVolume:
            MasterVolume.To(std::max(command.Value, 0.0f), Frames(std::max(command.Seconds, SmoothingSeconds)));
            return;
        case CommandKind::Listener:
        {
            ListenerPosition = command.Vectors[0];
            Vector3 right = Cross(command.Vectors[1], command.Vectors[2]);
            ListenerRight = Length(right) > 1e-6f ? Normalize(right) : Vector3 { 1.0f, 0.0f, 0.0f };
            return;
        }
        case CommandKind::StopAll:
            for (Voice* voice : Voices)
            {
                voice->Stopping = true;
                voice->StopGain.To(0.0f, Frames(std::max(command.Seconds, 0.005f)));
            }
            return;
        default:
            break;
        }

        Voice* voice = FindVoice(command.Voice);
        if (!voice)
        {
            // The voice already ended; there is nothing left to change.
            return;
        }
        switch (command.Kind)
        {
        case CommandKind::VoiceVolume:
            voice->Volume.To(std::max(command.Value, 0.0f), Frames(std::max(command.Seconds, SmoothingSeconds)));
            break;
        case CommandKind::VoicePan: voice->Pan = command.Value; break;
        case CommandKind::VoicePitch: voice->Pitch = command.Value; break;
        case CommandKind::VoicePosition:
            voice->Position = command.Vectors[0];
            voice->Positional = true;
            break;
        case CommandKind::VoicePause: voice->Paused = true; break;
        case CommandKind::VoiceResume: voice->Paused = false; break;
        case CommandKind::VoiceStop:
            voice->Stopping = true;
            voice->StopGain.To(0.0f, Frames(std::max(command.Seconds, 0.005f)));
            break;
        default: break;
        }
    }

    void Engine::Render(float* output, int frames)
    {
        ApplyCommands();
        while (frames > 0)
        {
            int block = std::min(frames, BlockFrames);
            MixBlock(output, block);
            output += static_cast<std::ptrdiff_t>(block) * Channels;
            frames -= block;
        }
    }

    void Engine::MixBlock(float* output, int frames)
    {
        std::size_t samples = static_cast<std::size_t>(frames) * 2;
        std::fill(Master.begin(), Master.begin() + static_cast<std::ptrdiff_t>(samples), 0.0f);
        for (BusEngine* bus : Buses)
        {
            if (bus)
            {
                std::fill(bus->Buffer.begin(), bus->Buffer.begin() + static_cast<std::ptrdiff_t>(samples), 0.0f);
            }
        }

        for (std::size_t index = 0; index < Voices.size();)
        {
            Voice& voice = *Voices[index];
            BusEngine* bus = voice.Bus >= 0 && voice.Bus < static_cast<int>(Buses.size()) ? Buses[static_cast<std::size_t>(voice.Bus)] : nullptr;
            MixVoice(voice, bus ? bus->Buffer.data() : Master.data(), frames);
            if (voice.Finished)
            {
                Finish(voice);
                Voices[index] = Voices.back();
                Voices.pop_back();
            }
            else
            {
                ++index;
            }
        }

        for (BusEngine* bus : Buses)
        {
            if (bus)
            {
                ProcessBus(*bus, frames);
            }
        }

        for (int frame = 0; frame < frames; ++frame)
        {
            float gain = MasterVolume.Next();
            float left = SoftClip(Master[static_cast<std::size_t>(frame) * 2] * gain);
            float right = SoftClip(Master[static_cast<std::size_t>(frame) * 2 + 1] * gain);
            float* out = output + static_cast<std::ptrdiff_t>(frame) * Channels;
            if (Channels == 1)
            {
                out[0] = (left + right) * 0.5f;
                continue;
            }
            out[0] = left;
            out[1] = right;
            for (int channel = 2; channel < Channels; ++channel)
            {
                out[channel] = 0.0f;
            }
        }
    }

    void Engine::TargetMatrix(const Voice& voice, float* matrix) const
    {
        float scale = voice.Paused ? 0.0f : 1.0f;
        if (voice.Positional)
        {
            Vector3 offset = voice.Position - ListenerPosition;
            float distance = Length(offset);
            float minimum = std::max(voice.MinimumDistance, 0.001f);
            float maximum = std::max(voice.MaximumDistance, minimum);
            float gain = 0.0f;
            if (distance < maximum)
            {
                // Half as loud at twice the minimum distance, as in the open, and
                // fading to silence over the last fifth of the way to the maximum.
                gain = minimum / std::max(distance, minimum);
                float fadeFrom = minimum + (maximum - minimum) * 0.8f;
                if (distance > fadeFrom && maximum > fadeFrom)
                {
                    gain *= (maximum - distance) / (maximum - fadeFrom);
                }
            }
            float pan = distance > 1e-4f ? Clamp(Dot(offset, ListenerRight) / distance, -1.0f, 1.0f) : 0.0f;
            float angle = (pan + 1.0f) * 0.25f * Pi;
            float left = std::cos(angle) * gain * scale;
            float right = std::sin(angle) * gain * scale;
            if (voice.ChannelCount > 1)
            {
                // A stereo sound in the world is heard as one point.
                matrix[0] = matrix[1] = left * 0.5f;
                matrix[2] = matrix[3] = right * 0.5f;
            }
            else
            {
                matrix[0] = left;
                matrix[1] = 0.0f;
                matrix[2] = right;
                matrix[3] = 0.0f;
            }
            return;
        }

        // The same panning as a Web Audio stereo panner: equal power for mono
        // sounds, and for stereo sounds one side moved into the other.
        float pan = Clamp(voice.Pan, -1.0f, 1.0f);
        if (voice.ChannelCount == 1)
        {
            float angle = (pan + 1.0f) * 0.25f * Pi;
            matrix[0] = std::cos(angle) * scale;
            matrix[1] = 0.0f;
            matrix[2] = std::sin(angle) * scale;
            matrix[3] = 0.0f;
        }
        else if (pan == 0.0f)
        {
            matrix[0] = scale;
            matrix[1] = 0.0f;
            matrix[2] = 0.0f;
            matrix[3] = scale;
        }
        else if (pan < 0.0f)
        {
            float angle = (pan + 1.0f) * 0.5f * Pi;
            matrix[0] = scale;
            matrix[1] = std::cos(angle) * scale;
            matrix[2] = 0.0f;
            matrix[3] = std::sin(angle) * scale;
        }
        else
        {
            float angle = pan * 0.5f * Pi;
            matrix[0] = std::cos(angle) * scale;
            matrix[1] = 0.0f;
            matrix[2] = std::sin(angle) * scale;
            matrix[3] = scale;
        }
    }

    void Engine::MixVoice(Voice& voice, float* destination, int frames)
    {
        float target[4];
        TargetMatrix(voice, target);
        if (!voice.Started)
        {
            std::copy(target, target + 4, voice.Matrix);
            voice.Started = true;
        }
        bool silent = voice.Matrix[0] == 0.0f && voice.Matrix[1] == 0.0f && voice.Matrix[2] == 0.0f && voice.Matrix[3] == 0.0f;
        if (voice.Paused && silent)
        {
            // Paused and already faded out: hold still.
            return;
        }

        double step = static_cast<double>(voice.SourceRate) * voice.Pitch / Rate;
        if (voice.Feed && !voice.Feed->EndReached.load(std::memory_order_acquire) &&
            static_cast<double>(voice.Feed->Available()) < voice.Fraction + step * frames + 4.0)
        {
            // The streaming thread fell behind: wait for it rather than skip.
            return;
        }

        float delta[4];
        for (int index = 0; index < 4; ++index)
        {
            delta[index] = (target[index] - voice.Matrix[index]) / static_cast<float>(frames);
        }
        float matrix[4] = { voice.Matrix[0], voice.Matrix[1], voice.Matrix[2], voice.Matrix[3] };
        bool stereo = voice.ChannelCount > 1;
        bool filtered = voice.Filters.Active();

        for (int frame = 0; frame < frames; ++frame)
        {
            if (Ended(voice))
            {
                voice.Finished = true;
                break;
            }
            float amount = static_cast<float>(voice.Fraction);
            float first = Hermite(voice.Window[0][0], voice.Window[1][0], voice.Window[2][0], voice.Window[3][0], amount);
            float second = stereo ? Hermite(voice.Window[0][1], voice.Window[1][1], voice.Window[2][1], voice.Window[3][1], amount) : 0.0f;
            if (filtered)
            {
                voice.Filters.Process(first, second);
            }
            for (int index = 0; index < 4; ++index)
            {
                matrix[index] += delta[index];
            }
            float gain = voice.Volume.Next() * voice.StopGain.Next();
            destination[frame * 2] += gain * (matrix[0] * first + matrix[1] * second);
            destination[frame * 2 + 1] += gain * (matrix[2] * first + matrix[3] * second);
            if (voice.Stopping && !voice.StopGain.Moving())
            {
                voice.Finished = true;
                break;
            }
            Advance(voice, step);
        }

        std::copy(target, target + 4, voice.Matrix);
        if (voice.SourceRate > 0)
        {
            voice.Status->Time.store((static_cast<double>(voice.Current) + voice.Fraction) / voice.SourceRate, std::memory_order_relaxed);
        }
    }

    void Engine::ProcessBus(BusEngine& bus, int frames)
    {
        bool filtered = bus.Filters.Active();
        bool echo = bus.Echo.Mix > 0.0f && bus.EchoLine.size() >= 4;
        std::size_t lineFrames = bus.EchoLine.size() / 2;
        std::size_t delay = 1;
        float feedback = 0.0f;
        if (echo)
        {
            delay = std::clamp<std::size_t>(Frames(bus.Echo.Delay), 1, lineFrames - 1);
            feedback = Clamp(bus.Echo.Feedback, 0.0f, 0.95f);
        }
        for (int frame = 0; frame < frames; ++frame)
        {
            float left = bus.Buffer[static_cast<std::size_t>(frame) * 2];
            float right = bus.Buffer[static_cast<std::size_t>(frame) * 2 + 1];
            if (filtered)
            {
                bus.Filters.Process(left, right);
            }
            if (echo)
            {
                std::size_t read = (bus.EchoAt + lineFrames - delay) % lineFrames;
                float delayedLeft = bus.EchoLine[read * 2];
                float delayedRight = bus.EchoLine[read * 2 + 1];
                bus.EchoLine[bus.EchoAt * 2] = left + delayedLeft * feedback;
                bus.EchoLine[bus.EchoAt * 2 + 1] = right + delayedRight * feedback;
                bus.EchoAt = (bus.EchoAt + 1) % lineFrames;
                left += delayedLeft * bus.Echo.Mix;
                right += delayedRight * bus.Echo.Mix;
            }
            float gain = bus.Volume.Next() * bus.Mute.Next();
            Master[static_cast<std::size_t>(frame) * 2] += left * gain;
            Master[static_cast<std::size_t>(frame) * 2 + 1] += right * gain;
        }
    }

    void Engine::Finish(Voice& voice)
    {
        voice.Finished = true;
        voice.Status->Finished.store(true, std::memory_order_release);
        // The queue holds twice the most voices there can be, so this always fits.
        FinishedVoices.Push(&voice);
    }
}
