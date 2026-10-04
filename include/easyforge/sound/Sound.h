#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <easyforge/assets/SoundData.h>

namespace easyforge
{
    namespace internal
    {
        struct SoundSource;
    }

    struct SoundSettings
    {
        // Decodes the file a piece at a time while it plays, instead of all at
        // once when it is loaded, for music and other long sounds.
        bool Stream = false;
    };

    // A sound ready to play: decoded into memory, or streamed from its file.
    //
    //     Sound jump = Sound::Load("jump.wav");
    //     Sound music = Sound::Load("theme.qoa", { .Stream = true });
    //     mixer.Play(jump);
    //
    // Sound is a handle: copies share the samples, and one sound can play many
    // times at once, on any number of mixers. A streamed sound opens its file
    // again for every play.
    class Sound
    {
    public:
        // An empty sound. It tests as false, with no error.
        Sound() = default;

        // Reads WAV and QOA files, as SoundData does.
        static Sound Load(std::string_view path, const SoundSettings& settings = {});

        // A sound from samples already in memory, such as ones a program made.
        static Sound FromData(SoundData data);

        explicit operator bool() const { return Source != nullptr; }
        const std::string& Error() const { return ErrorText; }

        bool IsStreamed() const;
        int SampleRate() const;
        int ChannelCount() const;
        std::uint64_t FrameCount() const;

        // Length in seconds.
        double Duration() const;

    private:
        std::shared_ptr<const internal::SoundSource> Source;
        std::string ErrorText;

        friend class Mixer;
    };
}
