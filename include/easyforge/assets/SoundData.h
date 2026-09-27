#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/assets/Pending.h>
#include <easyforge/core/Result.h>

namespace easyforge
{
    namespace internal
    {
        class SoundDecoder;
    }

    // A whole sound decoded into memory: samples from -1 to 1, one per channel
    // for each frame, channels interleaved (left, right, left, right, ...).
    //
    //     SoundData jump = SoundData::Load("jump.wav");
    //
    // Reads WAV (8, 16, 24, and 32-bit integer, 32 and 64-bit float) and QOA.
    // For long music, SoundStream decodes a little at a time instead.
    class SoundData
    {
    public:
        int SampleRate = 0;
        int ChannelCount = 0;
        std::vector<float> Samples;

        // An empty sound. It tests as false, with no error.
        SoundData() = default;

        // A sound from samples you already have.
        SoundData(int sampleRate, int channelCount, std::vector<float> samples);

        static SoundData Load(std::string_view path);
        static SoundData Decode(std::span<const std::uint8_t> bytes, std::string_view name = "sound");
        static Pending<SoundData> LoadInBackground(std::string_view path);

        explicit operator bool() const { return SampleRate > 0 && ChannelCount > 0 && ErrorText.empty(); }
        const std::string& Error() const { return ErrorText; }

        // Frames are one sample for every channel.
        std::size_t FrameCount() const { return ChannelCount > 0 ? Samples.size() / static_cast<std::size_t>(ChannelCount) : 0; }

        // Length in seconds.
        double Duration() const { return SampleRate > 0 ? static_cast<double>(FrameCount()) / SampleRate : 0.0; }

    private:
        std::string ErrorText;
    };

    // A sound file decoded a piece at a time, for music and other long sounds.
    // The file stays open while the stream exists; copies share one position.
    //
    //     SoundStream music = SoundStream::Open("theme.qoa");
    //     std::vector<float> buffer(1024 * music.ChannelCount());
    //     std::size_t frames = music.Read(buffer);
    class SoundStream
    {
    public:
        SoundStream() = default;

        static SoundStream Open(std::string_view path);

        explicit operator bool() const { return Decoder != nullptr; }
        const std::string& Error() const { return ErrorText; }

        int SampleRate() const;
        int ChannelCount() const;
        std::uint64_t FrameCount() const;

        // Decodes into `output`, which holds whole frames, and returns how many frames
        // were written. Returns 0 at the end of the sound.
        std::size_t Read(std::span<float> output);

        // Moves to a frame; the next Read starts there.
        Result<> Seek(std::uint64_t frame);

    private:
        std::shared_ptr<internal::SoundDecoder> Decoder;
        std::string ErrorText;
    };
}
