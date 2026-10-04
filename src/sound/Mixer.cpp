#include <easyforge/sound/Mixer.h>

#include <algorithm>
#include <cmath>

#include <easyforge/core/Scalar.h>

#include "MixerState.h"

namespace easyforge
{
    using internal::BusControl;
    using internal::Command;
    using internal::CommandKind;
    using internal::MixerState;
    using internal::VoiceControl;

    namespace
    {
        constexpr float LowestPitch = 0.125f;
        constexpr float HighestPitch = 8.0f;

        const VoiceControl& VoiceOf(const void* owner)
        {
            return *static_cast<const VoiceControl*>(owner);
        }

        VoiceControl& VoiceOf(void* owner)
        {
            return *static_cast<VoiceControl*>(owner);
        }

        const BusControl& BusOf(const void* owner)
        {
            return *static_cast<const BusControl*>(owner);
        }

        BusControl& BusOf(void* owner)
        {
            return *static_cast<BusControl*>(owner);
        }

        void SendToVoice(const VoiceControl& control, Command command)
        {
            std::shared_ptr<MixerState> state = control.Mixer.lock();
            if (!state || control.Id == 0)
            {
                return;
            }
            command.Voice = control.Id;
            std::lock_guard lock(state->ProgramLock);
            state->Reclaim();
            state->Send(command);
        }

        void SendToBus(const BusControl& control, Command command)
        {
            std::shared_ptr<MixerState> state = control.Mixer.lock();
            if (!state || control.Index < 0)
            {
                return;
            }
            command.Bus = control.Index;
            std::lock_guard lock(state->ProgramLock);
            state->Send(command);
        }

        void SendFilters(const BusControl& control)
        {
            Command command;
            command.Kind = CommandKind::BusFilters;
            {
                std::lock_guard lock(control.Lock);
                command.Value = control.LowPass;
                command.Other = control.HighPass;
            }
            SendToBus(control, command);
        }

        // With ProgramLock held.
        void SendListener(MixerState& state)
        {
            Command command;
            command.Kind = CommandKind::Listener;
            command.Vectors[0] = state.ListenerPosition;
            command.Vectors[1] = state.ListenerForward;
            command.Vectors[2] = state.ListenerUp;
            if (state.Working)
            {
                state.Send(command);
            }
        }

        // Listener properties on a mixer that has no state read as the defaults.
        template <Vector3 MixerState::*Member>
        Vector3 ReadListener(const void* owner)
        {
            if (!owner)
            {
                return Member == &MixerState::ListenerForward ? Vector3 { 0.0f, 0.0f, -1.0f }
                       : Member == &MixerState::ListenerUp    ? Vector3 { 0.0f, 1.0f, 0.0f }
                                                              : Vector3 {};
            }
            auto& state = *const_cast<MixerState*>(static_cast<const MixerState*>(owner));
            std::lock_guard lock(state.ProgramLock);
            return state.*Member;
        }

        template <Vector3 MixerState::*Member>
        void WriteListener(void* owner, const Vector3& value)
        {
            if (!owner)
            {
                return;
            }
            auto& state = *static_cast<MixerState*>(owner);
            std::lock_guard lock(state.ProgramLock);
            state.*Member = value;
            SendListener(state);
        }
    }

    // ---- PlayingSound ------------------------------------------------------------

    PlayingSound::PlayingSound() : PlayingSound(std::make_shared<VoiceControl>())
    {
    }

    PlayingSound::PlayingSound(std::shared_ptr<VoiceControl> control)
        : Volume(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(VoiceOf(owner).Lock);
                  return VoiceOf(owner).Volume;
              },
              [](void* owner, const float& value) {
                  VoiceControl& voice = VoiceOf(owner);
                  float volume = std::max(value, 0.0f);
                  {
                      std::lock_guard lock(voice.Lock);
                      voice.Volume = volume;
                  }
                  Command command;
                  command.Kind = CommandKind::VoiceVolume;
                  command.Value = volume;
                  SendToVoice(voice, command);
              }),
          Pan(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(VoiceOf(owner).Lock);
                  return VoiceOf(owner).Pan;
              },
              [](void* owner, const float& value) {
                  VoiceControl& voice = VoiceOf(owner);
                  float pan = Clamp(value, -1.0f, 1.0f);
                  {
                      std::lock_guard lock(voice.Lock);
                      voice.Pan = pan;
                  }
                  Command command;
                  command.Kind = CommandKind::VoicePan;
                  command.Value = pan;
                  SendToVoice(voice, command);
              }),
          Pitch(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(VoiceOf(owner).Lock);
                  return VoiceOf(owner).Pitch;
              },
              [](void* owner, const float& value) {
                  VoiceControl& voice = VoiceOf(owner);
                  float pitch = Clamp(value, LowestPitch, HighestPitch);
                  {
                      std::lock_guard lock(voice.Lock);
                      voice.Pitch = pitch;
                  }
                  Command command;
                  command.Kind = CommandKind::VoicePitch;
                  command.Value = pitch;
                  SendToVoice(voice, command);
              }),
          Position(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(VoiceOf(owner).Lock);
                  return VoiceOf(owner).Position;
              },
              [](void* owner, const Vector3& value) {
                  VoiceControl& voice = VoiceOf(owner);
                  {
                      std::lock_guard lock(voice.Lock);
                      voice.Position = value;
                  }
                  Command command;
                  command.Kind = CommandKind::VoicePosition;
                  command.Vectors[0] = value;
                  SendToVoice(voice, command);
              }),
          Control(std::move(control))
    {
    }

    PlayingSound::PlayingSound(const PlayingSound& other) : PlayingSound(other.Control)
    {
    }

    PlayingSound& PlayingSound::operator=(const PlayingSound& other)
    {
        if (this != &other)
        {
            Control = other.Control;
            RebindProperties();
        }
        return *this;
    }

    PlayingSound::~PlayingSound() = default;

    void PlayingSound::RebindProperties()
    {
        Volume.Rebind(Control.get());
        Pan.Rebind(Control.get());
        Pitch.Rebind(Control.get());
        Position.Rebind(Control.get());
    }

    bool PlayingSound::IsPlaying() const
    {
        return Control->Status && !Control->Stopped.load() && !Control->Status->Finished.load() && !Control->Mixer.expired();
    }

    bool PlayingSound::IsPaused() const
    {
        return Control->Paused.load() && IsPlaying();
    }

    double PlayingSound::Time() const
    {
        return Control->Status ? Control->Status->Time.load() : 0.0;
    }

    void PlayingSound::Pause() const
    {
        Control->Paused.store(true);
        Command command;
        command.Kind = CommandKind::VoicePause;
        SendToVoice(*Control, command);
    }

    void PlayingSound::Resume() const
    {
        Control->Paused.store(false);
        Command command;
        command.Kind = CommandKind::VoiceResume;
        SendToVoice(*Control, command);
    }

    void PlayingSound::Stop(float fadeSeconds) const
    {
        Control->Stopped.store(true);
        Command command;
        command.Kind = CommandKind::VoiceStop;
        command.Seconds = std::max(fadeSeconds, 0.0f);
        SendToVoice(*Control, command);
    }

    void PlayingSound::FadeTo(float volume, float seconds) const
    {
        volume = std::max(volume, 0.0f);
        {
            std::lock_guard lock(Control->Lock);
            Control->Volume = volume;
        }
        Command command;
        command.Kind = CommandKind::VoiceVolume;
        command.Value = volume;
        command.Seconds = std::max(seconds, 0.0f);
        SendToVoice(*Control, command);
    }

    // ---- MixerBus ------------------------------------------------------------------

    MixerBus::MixerBus() : MixerBus(std::make_shared<BusControl>())
    {
    }

    MixerBus::MixerBus(std::shared_ptr<BusControl> control)
        : Volume(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(BusOf(owner).Lock);
                  return BusOf(owner).Volume;
              },
              [](void* owner, const float& value) {
                  BusControl& bus = BusOf(owner);
                  float volume = std::max(value, 0.0f);
                  {
                      std::lock_guard lock(bus.Lock);
                      bus.Volume = volume;
                  }
                  Command command;
                  command.Kind = CommandKind::BusVolume;
                  command.Value = volume;
                  SendToBus(bus, command);
              }),
          Muted(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(BusOf(owner).Lock);
                  return BusOf(owner).Muted;
              },
              [](void* owner, const bool& value) {
                  BusControl& bus = BusOf(owner);
                  {
                      std::lock_guard lock(bus.Lock);
                      bus.Muted = value;
                  }
                  Command command;
                  command.Kind = CommandKind::BusMuted;
                  command.Value = value ? 1.0f : 0.0f;
                  SendToBus(bus, command);
              }),
          LowPass(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(BusOf(owner).Lock);
                  return BusOf(owner).LowPass;
              },
              [](void* owner, const float& value) {
                  BusControl& bus = BusOf(owner);
                  {
                      std::lock_guard lock(bus.Lock);
                      bus.LowPass = std::max(value, 0.0f);
                  }
                  SendFilters(bus);
              }),
          HighPass(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(BusOf(owner).Lock);
                  return BusOf(owner).HighPass;
              },
              [](void* owner, const float& value) {
                  BusControl& bus = BusOf(owner);
                  {
                      std::lock_guard lock(bus.Lock);
                      bus.HighPass = std::max(value, 0.0f);
                  }
                  SendFilters(bus);
              }),
          Echo(control.get(),
              [](const void* owner) {
                  std::lock_guard lock(BusOf(owner).Lock);
                  return BusOf(owner).Echo;
              },
              [](void* owner, const EchoSettings& value) {
                  BusControl& bus = BusOf(owner);
                  EchoSettings echo {
                      Clamp(value.Delay, 0.001f, internal::LongestEcho),
                      Clamp(value.Feedback, 0.0f, 0.95f),
                      std::max(value.Mix, 0.0f),
                  };
                  {
                      std::lock_guard lock(bus.Lock);
                      bus.Echo = echo;
                  }
                  Command command;
                  command.Kind = CommandKind::BusEcho;
                  command.Echo = echo;
                  SendToBus(bus, command);
              }),
          Control(std::move(control))
    {
    }

    MixerBus::MixerBus(const MixerBus& other) : MixerBus(other.Control)
    {
    }

    MixerBus& MixerBus::operator=(const MixerBus& other)
    {
        if (this != &other)
        {
            Control = other.Control;
            RebindProperties();
        }
        return *this;
    }

    MixerBus::~MixerBus() = default;

    void MixerBus::RebindProperties()
    {
        Volume.Rebind(Control.get());
        Muted.Rebind(Control.get());
        LowPass.Rebind(Control.get());
        HighPass.Rebind(Control.get());
        Echo.Rebind(Control.get());
    }

    std::string MixerBus::Name() const
    {
        return Control->Name;
    }

    void MixerBus::FadeTo(float volume, float seconds) const
    {
        volume = std::max(volume, 0.0f);
        {
            std::lock_guard lock(Control->Lock);
            Control->Volume = volume;
        }
        Command command;
        command.Kind = CommandKind::BusVolume;
        command.Value = volume;
        command.Seconds = std::max(seconds, 0.0f);
        SendToBus(*Control, command);
    }

    // ---- SoundListener --------------------------------------------------------------

    SoundListener::SoundListener(MixerState* state)
        : Position(state, ReadListener<&MixerState::ListenerPosition>, WriteListener<&MixerState::ListenerPosition>),
          Forward(state, ReadListener<&MixerState::ListenerForward>, WriteListener<&MixerState::ListenerForward>),
          Up(state, ReadListener<&MixerState::ListenerUp>, WriteListener<&MixerState::ListenerUp>)
    {
    }

    void SoundListener::Rebind(MixerState* state)
    {
        Position.Rebind(state);
        Forward.Rebind(state);
        Up.Rebind(state);
    }

    // ---- Mixer ---------------------------------------------------------------------

    Mixer::Mixer() : Mixer(std::shared_ptr<MixerState>())
    {
    }

    Mixer::Mixer(std::shared_ptr<MixerState> state)
        : Volume(state.get(),
              [](const void* owner) {
                  if (!owner)
                  {
                      return 1.0f;
                  }
                  auto& mixer = *const_cast<MixerState*>(static_cast<const MixerState*>(owner));
                  std::lock_guard lock(mixer.ProgramLock);
                  return mixer.MasterVolume;
              },
              [](void* owner, const float& value) {
                  if (!owner)
                  {
                      return;
                  }
                  auto& mixer = *static_cast<MixerState*>(owner);
                  std::lock_guard lock(mixer.ProgramLock);
                  mixer.MasterVolume = std::max(value, 0.0f);
                  if (mixer.Working)
                  {
                      Command command;
                      command.Kind = CommandKind::MasterVolume;
                      command.Value = mixer.MasterVolume;
                      mixer.Send(command);
                  }
              }),
          Listener(state.get()),
          State(std::move(state))
    {
    }

    Mixer Mixer::New(const MixerSettings& settings)
    {
        auto state = std::make_shared<MixerState>(false, 48000, 2);
        state->Self = state;
        std::string error;
        state->Device = internal::StartOutputDevice(*state, { settings.SampleRate, std::max(settings.Latency, 0.003f) }, error);
        state->Working = state->Device != nullptr;
        state->ErrorText = error;
        return Mixer(std::move(state));
    }

    Mixer Mixer::NewOffline(const OfflineSettings& settings)
    {
        int rate = Clamp(settings.SampleRate, 8000, 384000);
        int channels = Clamp(settings.ChannelCount, 1, 2);
        auto state = std::make_shared<MixerState>(true, rate, channels);
        state->Self = state;
        state->Working = true;
        return Mixer(std::move(state));
    }

    Mixer::Mixer(const Mixer& other) : Mixer(other.State)
    {
    }

    Mixer& Mixer::operator=(const Mixer& other)
    {
        if (this != &other)
        {
            State = other.State;
            RebindProperties();
        }
        return *this;
    }

    Mixer::~Mixer() = default;

    void Mixer::RebindProperties()
    {
        Volume.Rebind(State.get());
        Listener.Rebind(State.get());
    }

    Mixer::operator bool() const
    {
        return State && State->Working;
    }

    std::string Mixer::Error() const
    {
        return State ? State->ErrorText : std::string();
    }

    PlayingSound Mixer::Play(const Sound& sound, const PlaySettings& settings) const
    {
        if (!State || !State->Working || !sound.Source || sound.Source->SampleRate <= 0)
        {
            return {};
        }
        const internal::SoundSource& source = *sound.Source;
        int rate = State->Rate.load();
        auto voice = std::make_unique<internal::Voice>();
        voice->Source = sound.Source;
        voice->Status = std::make_shared<internal::VoiceStatus>();
        voice->FrameCount = source.FrameCount;
        voice->ChannelCount = std::max(source.ChannelCount, 1);
        voice->SourceRate = source.SampleRate;
        voice->Loop = settings.Loop;
        voice->Pan = Clamp(settings.Pan, -1.0f, 1.0f);
        voice->Pitch = Clamp(settings.Pitch, LowestPitch, HighestPitch);
        voice->Positional = settings.Position.has_value();
        voice->Position = settings.Position.value_or(Vector3 {});
        voice->MinimumDistance = std::max(settings.MinimumDistance, 0.001f);
        voice->MaximumDistance = std::max(settings.MaximumDistance, voice->MinimumDistance);
        voice->LowPass = std::max(settings.LowPass, 0.0f);
        voice->HighPass = std::max(settings.HighPass, 0.0f);
        voice->Paused = settings.Paused;
        float volume = std::max(settings.Volume, 0.0f);
        voice->Volume.Value = volume;
        voice->Volume.Target = volume;
        if (settings.FadeIn > 0.0f)
        {
            voice->Volume.Value = 0.0f;
            voice->Volume.To(volume, static_cast<std::uint32_t>(settings.FadeIn * static_cast<float>(rate)));
        }

        std::uint64_t start = 0;
        if (settings.Start > 0.0 && source.FrameCount > 0)
        {
            start = std::min(static_cast<std::uint64_t>(settings.Start * source.SampleRate), source.FrameCount);
            if (settings.Loop && start == source.FrameCount)
            {
                start = 0;
            }
        }

        if (source.Streamed())
        {
            SoundStream stream = SoundStream::Open(source.Path);
            if (!stream || (start > 0 && !stream.Seek(start)))
            {
                return {};
            }
            // Enough for the fastest pitch to take a whole block, and at least a
            // second, so the streaming thread is never in a hurry.
            double fastest = static_cast<double>(internal::BlockFrames) * HighestPitch * source.SampleRate / std::max(rate, 1);
            std::size_t capacity = std::max<std::size_t>(static_cast<std::size_t>(source.SampleRate),
                static_cast<std::size_t>(fastest) + 256);
            voice->Feed = std::make_shared<internal::StreamFeed>(std::move(stream), settings.Loop, capacity);
            voice->Feed->Fill();
        }
        else
        {
            voice->Samples = source.Data->Samples.data();
        }
        internal::PrepareVoice(*voice, start);

        auto control = std::make_shared<VoiceControl>();
        control->Mixer = State;
        control->Status = voice->Status;
        control->Paused.store(settings.Paused);
        control->Volume = volume;
        control->Pan = voice->Pan;
        control->Pitch = voice->Pitch;
        control->Position = voice->Position;

        std::lock_guard lock(State->ProgramLock);
        State->Reclaim();
        if (State->LiveVoices >= internal::MaximumVoices)
        {
            if (voice->Feed)
            {
                voice->Feed->Abandoned.store(true);
            }
            return {};
        }
        if (!settings.Bus.empty())
        {
            std::shared_ptr<BusControl> bus = State->BusNamed(settings.Bus);
            voice->Bus = bus ? bus->Index : -1;
        }
        voice->Id = State->NextVoiceId++;
        control->Id = voice->Id;
        if (voice->Feed)
        {
            State->AddFeed(voice->Feed);
        }
        Command command;
        command.Kind = CommandKind::AddVoice;
        command.NewVoice = voice.release();
        State->Send(command);
        ++State->LiveVoices;
        return PlayingSound(std::move(control));
    }

    MixerBus Mixer::Bus(std::string_view name) const
    {
        if (!State)
        {
            return {};
        }
        std::lock_guard lock(State->ProgramLock);
        std::shared_ptr<BusControl> control = State->BusNamed(name);
        return control ? MixerBus(std::move(control)) : MixerBus();
    }

    void Mixer::StopAll(float fadeSeconds) const
    {
        if (!State || !State->Working)
        {
            return;
        }
        Command command;
        command.Kind = CommandKind::StopAll;
        command.Seconds = std::max(fadeSeconds, 0.0f);
        std::lock_guard lock(State->ProgramLock);
        State->Send(command);
    }

    std::size_t Mixer::PlayingCount() const
    {
        if (!State)
        {
            return 0;
        }
        std::lock_guard lock(State->ProgramLock);
        State->Reclaim();
        return State->LiveVoices;
    }

    int Mixer::SampleRate() const
    {
        return State ? State->Rate.load() : 0;
    }

    int Mixer::ChannelCount() const
    {
        return State ? State->Channels.load() : 0;
    }

    std::string Mixer::DeviceName() const
    {
        return State ? State->DeviceName() : std::string();
    }

    void Mixer::Render(std::span<float> output) const
    {
        if (!State || !State->Offline)
        {
            return;
        }
        std::size_t channels = static_cast<std::size_t>(State->Channels.load());
        State->RenderOffline(output.data(), output.size() / channels);
    }
}
