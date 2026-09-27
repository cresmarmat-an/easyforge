# sound

`sound` will play sound: short effects, long music streamed from disk, and
sounds placed in the world that get quieter with distance.

> [!NOTE] Not available yet
> `sound` is step 9 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- `Mixer::New()` to open the default output device, and `mixer.Play(sound)` to
  play a `Sound` loaded from a file.
- Volume, looping, fading, and buses such as Music and Effects that can be
  turned down together.
- Positional sound with a listener, distance fading, and panning.
- Low-pass and high-pass filters, echo, and resampling, all written from scratch.
- A mixer on its own thread that never waits on the rest of the program.

Windows comes first with WASAPI, then ALSA and PulseAudio, Web Audio, CoreAudio,
and AAudio.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#sound).
