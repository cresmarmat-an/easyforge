#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <easyforge/core/Property.h>
#include <easyforge/core/Vector.h>
#include <easyforge/sound/Sound.h>

namespace easyforge
{
    namespace internal
    {
        class MixerState;
        struct VoiceControl;
        struct BusControl;
    }

    struct MixerSettings
    {
        // Frames a second to mix at. Zero uses the output device's own rate,
        // which saves converting twice.
        int SampleRate = 0;

        // How far ahead of the speakers the mixer works, in seconds. Shorter
        // answers sooner; too short and a busy computer makes the sound crackle.
        float Latency = 0.04f;
    };

    struct OfflineSettings
    {
        int SampleRate = 48000;

        // 1 or 2.
        int ChannelCount = 2;
    };

    struct EchoSettings
    {
        // Seconds between repeats, up to 2.
        float Delay = 0.25f;

        // How much of each repeat comes back in the next, from 0 to 0.95.
        float Feedback = 0.35f;

        // How loud the repeats are next to the sound itself; 0 turns echo off.
        float Mix = 0.0f;
    };

    struct PlaySettings
    {
        float Volume = 1.0f;

        // From -1 for the left speaker to 1 for the right.
        float Pan = 0.0f;

        // Speed and pitch together: 2 plays an octave higher in half the time.
        // From 0.125 to 8.
        float Pitch = 1.0f;

        bool Loop = false;

        // The bus to play through, made the first time it is named. Empty plays
        // straight to the mixer.
        std::string Bus;

        // A place in the world. The sound pans and fades with its place around
        // the mixer's listener, and Pan is not used.
        std::optional<Vector3> Position;

        // Within the minimum distance the sound plays at full volume; it gets
        // quieter with distance, as sound does in the open, and is silent past
        // the maximum.
        float MinimumDistance = 1.0f;
        float MaximumDistance = 50.0f;

        // Seconds to rise from silence to the volume.
        float FadeIn = 0.0f;

        // Where in the sound to start, in seconds.
        double Start = 0.0;

        // Filters, in hertz; 0 leaves the sound as it is. LowPass keeps what is
        // below it, as through a wall; HighPass keeps what is above it, as
        // through a small speaker.
        float LowPass = 0.0f;
        float HighPass = 0.0f;

        // Starts the sound paused, to Resume later.
        bool Paused = false;
    };

    // A sound a mixer is playing. Changes take effect within a few
    // milliseconds, smoothly. Once the sound ends or is stopped, the handle
    // stays valid and does nothing.
    //
    //     PlayingSound song = mixer.Play(music, { .Volume = 0.5f, .Loop = true });
    //     song.FadeTo(0.0f, 2.0f);
    //
    // PlayingSound is a handle: copies control the same sound.
    class PlayingSound
    {
    public:
        // A handle that controls nothing.
        PlayingSound();

        PlayingSound(const PlayingSound& other);
        PlayingSound& operator=(const PlayingSound& other);
        ~PlayingSound();

        // False once the sound has ended or been stopped. A paused sound is
        // still playing.
        bool IsPlaying() const;
        bool IsPaused() const;

        // Seconds into the sound; a looping sound starts again from 0.
        double Time() const;

        void Pause() const;
        void Resume() const;

        // Stops the sound, fading it out over the given seconds first.
        void Stop(float fadeSeconds = 0.0f) const;

        // Changes the volume gradually over the given seconds.
        void FadeTo(float volume, float seconds) const;

        // The values last set; reading them does not ask the mixer.
        Property<float> Volume;
        Property<float> Pan;
        Property<float> Pitch;

        // Setting a place makes the sound positional from then on.
        Property<Vector3> Position;

    private:
        explicit PlayingSound(std::shared_ptr<internal::VoiceControl> control);
        void RebindProperties();

        std::shared_ptr<internal::VoiceControl> Control;

        friend class Mixer;
    };

    // A group of sounds turned up, down, or filtered together, such as Music
    // or Effects. Buses play into the mixer.
    //
    //     mixer.Bus("Music").Volume = 0.3f;
    //     mixer.Bus("Music").LowPass = 600;   // under water
    //
    // MixerBus is a handle; copies refer to the same bus.
    class MixerBus
    {
    public:
        // A handle that refers to no bus.
        MixerBus();

        MixerBus(const MixerBus& other);
        MixerBus& operator=(const MixerBus& other);
        ~MixerBus();

        std::string Name() const;

        // Changes the volume gradually over the given seconds.
        void FadeTo(float volume, float seconds) const;

        Property<float> Volume;
        Property<bool> Muted;

        // In hertz; 0 is off.
        Property<float> LowPass;
        Property<float> HighPass;

        Property<EchoSettings> Echo;

    private:
        explicit MixerBus(std::shared_ptr<internal::BusControl> control);
        void RebindProperties();

        std::shared_ptr<internal::BusControl> Control;

        friend class Mixer;
    };

    // Where the mixer hears positional sounds from: usually the camera or the
    // player. It faces -Z with +Y up unless turned, as everything in easyforge
    // does.
    class SoundListener
    {
    public:
        SoundListener(const SoundListener&) = delete;
        SoundListener& operator=(const SoundListener&) = delete;

        Property<Vector3> Position;
        Property<Vector3> Forward;
        Property<Vector3> Up;

    private:
        explicit SoundListener(internal::MixerState* state);
        void Rebind(internal::MixerState* state);

        friend class Mixer;
    };

    // Mixes sounds and sends them to the speakers, on a thread of its own that
    // never waits on the rest of the program.
    //
    //     Mixer mixer = Mixer::New();
    //     Sound jump = Sound::Load("jump.wav");
    //     mixer.Play(jump, { .Position = { 4, 0, 0 } });
    //     mixer.Listener.Position = playerPosition;
    //
    // Mixer is a handle: copies share the same mixer, which stops when the last
    // copy goes. Every function can be called from any thread.
    class Mixer
    {
    public:
        // A mixer that plays nothing. It tests as false, with no error.
        Mixer();

        // A mixer playing to the system's default output device, following it
        // when the default changes. Without a device it tests as false and its
        // error says why; playing on it does nothing.
        static Mixer New(const MixerSettings& settings = {});

        // A mixer with no device, which mixes when asked with Render: for tests,
        // and for writing sound to a file.
        static Mixer NewOffline(const OfflineSettings& settings = {});

        Mixer(const Mixer& other);
        Mixer& operator=(const Mixer& other);
        ~Mixer();

        explicit operator bool() const;
        std::string Error() const;

        // Starts playing a sound. An empty or failed sound, or a mixer that tests
        // as false, gives a handle that controls nothing.
        PlayingSound Play(const Sound& sound, const PlaySettings& settings = {}) const;

        // The bus with this name, made the first time it is asked for.
        MixerBus Bus(std::string_view name) const;

        // Stops every sound, fading them out over the given seconds first.
        void StopAll(float fadeSeconds = 0.0f) const;

        // Sounds playing now, paused ones included.
        std::size_t PlayingCount() const;

        int SampleRate() const;
        int ChannelCount() const;

        // The output device's name, as the system shows it; empty when offline.
        std::string DeviceName() const;

        // For offline mixers: mixes the next frames into `output`, which holds
        // whole frames with channels interleaved. Does nothing for a mixer
        // playing to a device.
        void Render(std::span<float> output) const;

        // The volume of everything, after the buses.
        Property<float> Volume;

        SoundListener Listener;

    private:
        explicit Mixer(std::shared_ptr<internal::MixerState> state);
        void RebindProperties();

        std::shared_ptr<internal::MixerState> State;
    };
}
