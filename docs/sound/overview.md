# sound

`sound` plays sound: short effects from memory, long music streamed from its
file, groups of sounds turned down or filtered together, and sounds placed in
the world that pan and fade around a listener.

```cpp
#include <easyforge/sound.h>

using namespace easyforge;

int main()
{
    Mixer mixer = Mixer::New();          // the default output device
    if (!mixer)
    {
        Log(LogLevel::Warning, mixer.Error());   // such as "there is no sound output device"
    }

    Sound jump = Sound::Load("jump.wav");
    Sound music = Sound::Load("theme.qoa", { .Stream = true });

    PlayingSound song = mixer.Play(music, { .Volume = 0.5f, .Loop = true, .Bus = "Music" });
    mixer.Play(jump, { .Position = Vector3 { 4, 0, 0 } });   // to the right of the listener
    mixer.Listener.Position = Vector3 { 0, 0, 0 };

    song.FadeTo(0.0f, 2.0f);
    mixer.Bus("Music").Volume = 0.3f;
}
```

Link `easyforge::sound`. It needs `core` and `assets`, which reads the files.

## Pages

- [Playing sounds](playing-sounds.md): loading, streaming, the settings of a
  play, and changing a sound while it plays.
- [Buses and effects](buses-and-effects.md): groups of sounds, filters, echo,
  and the mixer's own volume.
- [Positional sound](positional-sound.md): the listener, places, and how
  distance and direction are heard.
- [Devices and offline mixing](devices-and-offline-mixing.md): the output
  device, what happens without one, and mixing into memory for tests or files.

[Example 07](https://github.com/cresmarmat-an/easyforge-examples/tree/main/07-sound)
loops music and plays a sound wherever you click around a listener.

## How it works

The mixer runs on a thread of its own, woken by the output device whenever it
wants more sound. Programs talk to it through a queue it reads without ever
waiting, so a busy program cannot make the sound stutter, and the mixer never
waits on the program. Every function can be called from any thread.

Each playing sound is read at its own rate and pitch with cubic interpolation,
filtered, panned, and added to its bus; buses are filtered, echoed, and added
together; and the result passes a soft limiter that bends loud peaks instead of
cutting them. Streamed sounds are decoded by a separate thread about a second
ahead of where they play. Everything from the mixing to the filters is written
for easyforge.

## Limitations

- Playing to a device works on Windows, through WASAPI. ALSA and PulseAudio,
  Web Audio, CoreAudio, and AAudio come with their platforms; offline mixing
  works everywhere.
- The mixer mixes in stereo. A device with more speakers gets the left and
  right channels, and the rest stay silent.
- Files are WAV and QOA. Ogg Vorbis and MP3 come in stage 2.
- At most 512 sounds play at once, on at most 64 buses.
