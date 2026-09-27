#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <easyforge/core/Result.h>

namespace easyforge::internal
{
    // Bytes a decoder reads from: a file, or a block of memory.
    class ByteInput
    {
    public:
        virtual ~ByteInput() = default;
        virtual std::uint64_t Size() const = 0;
        virtual std::size_t ReadAt(std::uint64_t offset, std::span<std::uint8_t> output) = 0;
    };

    class SoundDecoder
    {
    public:
        virtual ~SoundDecoder() = default;

        int SampleRate = 0;
        int ChannelCount = 0;
        std::uint64_t FrameCount = 0;

        // Fills `output` with whole frames and returns how many were written.
        virtual std::size_t Read(std::span<float> output) = 0;
        virtual Result<> Seek(std::uint64_t frame) = 0;
    };

    Result<std::unique_ptr<SoundDecoder>> OpenWav(std::shared_ptr<ByteInput> input);
    Result<std::unique_ptr<SoundDecoder>> OpenQoa(std::shared_ptr<ByteInput> input);

    // Picks the decoder from the first bytes of the input.
    Result<std::unique_ptr<SoundDecoder>> OpenSound(std::shared_ptr<ByteInput> input);
}
