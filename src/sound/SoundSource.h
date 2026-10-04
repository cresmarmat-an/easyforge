#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <easyforge/assets/SoundData.h>

namespace easyforge::internal
{
    // What a Sound plays: samples in memory, or the path of a file to stream.
    struct SoundSource
    {
        std::shared_ptr<const SoundData> Data;
        std::string Path;
        int SampleRate = 0;
        int ChannelCount = 0;
        std::uint64_t FrameCount = 0;

        bool Streamed() const { return Data == nullptr; }
    };
}
