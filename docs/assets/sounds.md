# Sounds

Short sounds are decoded whole into a `SoundData`. Long ones, such as music, are
decoded a piece at a time with a `SoundStream`.

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

SoundData jump = SoundData::Load("sounds/jump.wav");
Log("{} channels at {} Hz, {:.2f} seconds", jump.ChannelCount, jump.SampleRate, jump.Duration());

SoundStream music = SoundStream::Open("music/theme.qoa");
std::vector<float> buffer(1024 * music.ChannelCount());
std::size_t frames = music.Read(buffer);
```

## SoundData

| Member | What it holds |
|---|---|
| `SampleRate` | Frames per second, such as 44100 |
| `ChannelCount` | 1 for mono, 2 for stereo |
| `Samples` | Floats from -1 to 1, channels interleaved: left, right, left, right, ... |
| `FrameCount()` | Samples per channel |
| `Duration()` | Length in seconds |

A frame is one sample for each channel. To make a sound yourself, use
`SoundData(sampleRate, channelCount, samples)`; it checks that the samples hold
whole frames.

## SoundStream

A stream keeps its file open and decodes only what you ask for, so an hour of
music costs almost no memory.

| Written | What it does |
|---|---|
| `SoundStream::Open(path)` | Opens the file. `if (!stream)` and `stream.Error()` work as usual |
| `SampleRate()`, `ChannelCount()`, `FrameCount()` | As for `SoundData` |
| `Read(buffer)` | Decodes into `buffer`, which holds whole frames, and returns how many frames it wrote. 0 means the end |
| `Seek(frame)` | Moves to a frame; the next `Read` starts there |

Copies of a stream share one file and one position. Use one stream per voice
that plays it.

## Formats

### WAV

Reads 8, 16, 24, and 32-bit integer samples and 32 and 64-bit floating point
samples, in the plain and the extensible header, with any number of channels up
to 16. Other chunks, such as metadata, are skipped.

A WAV file cut short plays up to where it ends, as most players do.

Not done: compressed WAV (ADPCM, µ-law, A-law, and others) gives an error.

### QOA

Reads QOA, the Quite OK Audio format: compressed to about a fifth of the size of
16-bit WAV, fast to decode, and simple enough to seek to any sample. It is a good
choice for music.

Not done: QOA files written for streaming, without a total length in their
header, give an error.

### Coming later

Ogg Vorbis and MP3 arrive in stage 2.
