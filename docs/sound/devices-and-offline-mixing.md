# Devices and offline mixing

```cpp
Mixer mixer = Mixer::New({ .Latency = 0.03f });
if (mixer)
{
    Log("playing on {} at {} frames a second", mixer.DeviceName(), mixer.SampleRate());
}
else
{
    Log(LogLevel::Warning, mixer.Error());
}
```

## The output device

`Mixer::New(settings)` opens the system's default output device and starts
mixing for it on a thread of its own. When the person changes the default
device, or unplugs headphones, the mixer moves to the new default by itself,
and sounds carry on. With no device at all, sounds keep moving along in time,
unheard, and the mixer looks for a device again every second.

| Setting | Default | Does |
|---|---|---|
| `SampleRate` | `0` | frames a second to mix at; 0 uses the device's own rate, which saves converting twice |
| `Latency` | `0.04` | how far ahead of the speakers the mixer works, in seconds |

Shorter latency means a sound starts sooner after `Play`; too short, and a busy
computer makes the sound crackle. Most programs never change it.

When no device can be opened, `Mixer::New` gives a mixer that tests as false,
with an error such as `there is no sound output device`. Everything can still
be called on it and does nothing, so a program can carry on without sound.

`SampleRate()` and `ChannelCount()` give the device's format, which can change
when the device does, and `DeviceName()` its name as the system shows it.

## Offline mixing

```cpp
Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000, .ChannelCount = 2 });
mixer.Play(Sound::Load("theme.qoa", { .Stream = true }));

std::vector<float> output(48000 * 2 * 10);   // ten seconds, left and right
mixer.Render(output);
```

`Mixer::NewOffline` makes a mixer with no device and no thread: it mixes only
when asked, with `Render(output)`, which fills whole frames with the channels
interleaved and moves every sound along by that much. It mixes exactly as a
device mixer does and gives the same result every time, even for streamed
sounds, which makes it the way to test sound, and to write sound to a file or
mix it for something other than speakers. `ChannelCount` is 1 or 2.

## Threads

Every function of `Mixer`, `PlayingSound`, `MixerBus`, and `Sound` can be called
from any thread. The mixer's own thread never waits for the program: commands
go through a queue it reads without locking, voices that end are handed back to
the program to free, and streamed files are decoded by a third thread.

## Limitations

- Only Windows has a device so far, through WASAPI in shared mode. Exclusive
  mode, choosing a device other than the default, and recording are not
  supported yet.
- Devices with more than two speakers get sound in the first two.
