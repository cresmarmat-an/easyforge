# Playing sounds

```cpp
Mixer mixer = Mixer::New();
Sound coin = Sound::Load("coin.wav");
if (!coin)
{
    Log(LogLevel::Error, coin.Error());
}

PlayingSound playing = mixer.Play(coin, { .Volume = 0.8f, .Pitch = 1.2f });
playing.Pan = -0.5f;
```

## Sounds

`Sound::Load(path)` reads a WAV or QOA file and decodes all of it into memory,
which suits short sounds played often. `Sound::Load(path, { .Stream = true })`
only checks the file, and decodes it a piece at a time while it plays, which
suits music and other long sounds. Paths are found the way `assets` finds files,
in mounted folders and packs too.

`Sound::FromData(data)` makes a sound from a `SoundData` already in memory,
such as one a program made itself:

```cpp
std::vector<float> samples(48000);
for (std::size_t frame = 0; frame < samples.size(); ++frame)
{
    samples[frame] = 0.3f * std::sin(2.0f * Pi * 440.0f * frame / 48000.0f);
}
Sound beep = Sound::FromData(SoundData(48000, 1, std::move(samples)));
```

A sound that failed tests as false and its `Error()` says why. `SampleRate()`,
`ChannelCount()`, `FrameCount()`, `Duration()` in seconds, and `IsStreamed()`
describe it. A sound is a handle: copies share the samples, and one sound can
play any number of times at once, on any number of mixers.

## The settings of a play

| Setting | Default | Does |
|---|---|---|
| `Volume` | `1` | `0.5` is half as loud; above 1 is louder |
| `Pan` | `0` | from `-1`, the left speaker, to `1`, the right |
| `Pitch` | `1` | speed and pitch together, from `0.125` to `8`: `2` is an octave higher and twice as fast |
| `Loop` | `false` | starts again from the beginning at the end |
| `Bus` | none | the [bus](buses-and-effects.md) to play through, made the first time it is named |
| `Position` | none | a [place in the world](positional-sound.md); `Pan` is then not used |
| `MinimumDistance`, `MaximumDistance` | `1`, `50` | how distance is heard, for a sound with a place |
| `FadeIn` | `0` | seconds to rise from silence |
| `Start` | `0` | where in the sound to begin, in seconds |
| `LowPass`, `HighPass` | `0` | filters in hertz, as on a [bus](buses-and-effects.md#filters); 0 is off |
| `Paused` | `false` | starts paused, to `Resume` later |

A mono sound panned to the middle reaches each speaker at 71% (equal power),
so it sounds as loud in the middle as at either side. A stereo sound in the
middle plays exactly as it was recorded; panned, one channel moves into the
other. This is the same panning a Web Audio stereo panner does.

## While it plays

`Play` gives a `PlayingSound`, which changes the sound while it plays:

```cpp
PlayingSound engine = mixer.Play(engineLoop, { .Loop = true });

engine.Pitch = 1.0f + speed / 40.0f;
engine.Volume = 0.6f;
engine.FadeTo(0.0f, 1.5f);     // to silence over a second and a half
engine.Pause();
engine.Resume();
engine.Stop(0.2f);             // fades out over a fifth of a second, then ends
```

`Volume`, `Pan`, `Pitch`, and `Position` are properties. Changes reach the mixer
within a few milliseconds, and volume and pan slide to their new values over
ten milliseconds so that nothing clicks. Reading a property gives the value
last set, without asking the mixer.

`IsPlaying()` is false once the sound ends or is stopped, `IsPaused()` tells
whether it is paused, and `Time()` gives the seconds into the sound, counting
from 0 again each time a looping sound starts over. A paused sound keeps its
place. Once a sound has ended, its handle stays safe to use and does nothing.

`mixer.StopAll(fadeSeconds)` stops every sound, and `mixer.PlayingCount()`
counts the sounds playing, paused ones too.

## Streaming

A streamed sound opens its file again each time it plays, so many copies of
the same music can play at different places. A thread of the mixer's own
decodes each about a second ahead. If that thread falls behind, the sound waits
rather than skips. Streamed and whole sounds play identically, sample for
sample.

## Limitations

- At most 512 sounds play at once; past that, `Play` gives a handle that is not
  playing.
- There is no event when a sound ends; check `IsPlaying()`.
- A looping sound loops the whole file; there are no loop points inside it yet.
