#pragma once

// Sounds made for the tests, and what to measure in a mixer's output.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include <easyforge/sound.h>

namespace easyforge::testing
{
    // The same value in every frame, one per channel.
    inline Sound Steady(int sampleRate, std::vector<float> values, std::size_t frames)
    {
        std::vector<float> samples;
        samples.reserve(frames * values.size());
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples.insert(samples.end(), values.begin(), values.end());
        }
        return Sound::FromData(SoundData(sampleRate, static_cast<int>(values.size()), std::move(samples)));
    }

    inline std::vector<float> ToneSamples(int sampleRate, int channelCount, float frequency, double seconds, float amplitude)
    {
        std::size_t frames = static_cast<std::size_t>(seconds * sampleRate);
        std::vector<float> samples(frames * static_cast<std::size_t>(channelCount));
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            float value = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(frame) / sampleRate));
            for (int channel = 0; channel < channelCount; ++channel)
            {
                samples[frame * static_cast<std::size_t>(channelCount) + static_cast<std::size_t>(channel)] = value;
            }
        }
        return samples;
    }

    inline Sound Tone(int sampleRate, int channelCount, float frequency, double seconds, float amplitude = 0.5f)
    {
        return Sound::FromData(SoundData(sampleRate, channelCount, ToneSamples(sampleRate, channelCount, frequency, seconds, amplitude)));
    }

    inline std::vector<float> Render(const Mixer& mixer, std::size_t frames)
    {
        std::vector<float> output(frames * static_cast<std::size_t>(mixer.ChannelCount()));
        mixer.Render(output);
        return output;
    }

    // One channel of interleaved stereo output.
    inline std::vector<float> Channel(const std::vector<float>& stereo, int channel)
    {
        std::vector<float> samples;
        for (std::size_t index = static_cast<std::size_t>(channel); index < stereo.size(); index += 2)
        {
            samples.push_back(stereo[index]);
        }
        return samples;
    }

    inline float RootMeanSquare(const std::vector<float>& samples, std::size_t from = 0)
    {
        double sum = 0.0;
        for (std::size_t index = from; index < samples.size(); ++index)
        {
            sum += static_cast<double>(samples[index]) * samples[index];
        }
        return samples.size() > from ? static_cast<float>(std::sqrt(sum / static_cast<double>(samples.size() - from))) : 0.0f;
    }

    inline int RisingCrossings(const std::vector<float>& samples)
    {
        int crossings = 0;
        for (std::size_t index = 1; index < samples.size(); ++index)
        {
            if (samples[index - 1] < 0.0f && samples[index] >= 0.0f)
            {
                ++crossings;
            }
        }
        return crossings;
    }

    // Writes 32-bit float WAV, which SoundData reads back exactly.
    inline bool WriteWave(const std::string& path, int sampleRate, int channelCount, const std::vector<float>& samples)
    {
        std::ofstream file(path, std::ios::binary);
        auto word = [&](std::uint32_t value, int bytes) {
            for (int index = 0; index < bytes; ++index)
            {
                file.put(static_cast<char>((value >> (8 * index)) & 0xFF));
            }
        };
        std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 4);
        file.write("RIFF", 4);
        word(36 + dataBytes, 4);
        file.write("WAVEfmt ", 8);
        word(16, 4);
        word(3, 2);
        word(static_cast<std::uint32_t>(channelCount), 2);
        word(static_cast<std::uint32_t>(sampleRate), 4);
        word(static_cast<std::uint32_t>(sampleRate * channelCount * 4), 4);
        word(static_cast<std::uint32_t>(channelCount * 4), 2);
        word(32, 2);
        file.write("data", 4);
        word(dataBytes, 4);
        for (float sample : samples)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &sample, 4);
            word(bits, 4);
        }
        return static_cast<bool>(file);
    }
}
