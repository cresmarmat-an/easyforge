#include <easyforge/sound/Sound.h>

#include <format>

#include "SoundSource.h"

namespace easyforge
{
    Sound Sound::Load(std::string_view path, const SoundSettings& settings)
    {
        Sound sound;
        if (settings.Stream)
        {
            // Opened once here to check the file; every play opens it again.
            SoundStream stream = SoundStream::Open(path);
            if (!stream)
            {
                sound.ErrorText = stream.Error().empty() ? std::format("cannot open {}", path) : stream.Error();
                return sound;
            }
            auto source = std::make_shared<internal::SoundSource>();
            source->Path = std::string(path);
            source->SampleRate = stream.SampleRate();
            source->ChannelCount = stream.ChannelCount();
            source->FrameCount = stream.FrameCount();
            sound.Source = std::move(source);
            return sound;
        }
        SoundData data = SoundData::Load(path);
        if (!data)
        {
            sound.ErrorText = data.Error().empty() ? std::format("{} holds no sound", path) : data.Error();
            return sound;
        }
        return FromData(std::move(data));
    }

    Sound Sound::FromData(SoundData data)
    {
        Sound sound;
        if (!data)
        {
            sound.ErrorText = data.Error().empty() ? std::string("the sound data is empty") : data.Error();
            return sound;
        }
        auto source = std::make_shared<internal::SoundSource>();
        source->SampleRate = data.SampleRate;
        source->ChannelCount = data.ChannelCount;
        source->FrameCount = data.FrameCount();
        source->Data = std::make_shared<const SoundData>(std::move(data));
        sound.Source = std::move(source);
        return sound;
    }

    bool Sound::IsStreamed() const
    {
        return Source && Source->Streamed();
    }

    int Sound::SampleRate() const
    {
        return Source ? Source->SampleRate : 0;
    }

    int Sound::ChannelCount() const
    {
        return Source ? Source->ChannelCount : 0;
    }

    std::uint64_t Sound::FrameCount() const
    {
        return Source ? Source->FrameCount : 0;
    }

    double Sound::Duration() const
    {
        return Source && Source->SampleRate > 0 ? static_cast<double>(Source->FrameCount) / Source->SampleRate : 0.0;
    }
}
