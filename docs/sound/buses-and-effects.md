# Buses and effects

```cpp
Mixer mixer = Mixer::New();
mixer.Play(theme, { .Loop = true, .Bus = "Music" });
mixer.Play(footstep, { .Bus = "Effects" });

MixerBus music = mixer.Bus("Music");
music.Volume = 0.4f;                 // the music setting in an options screen
music.LowPass = 600;                 // muffled, as from another room
mixer.Bus("Effects").Echo = { .Delay = 0.3f, .Feedback = 0.4f, .Mix = 0.5f };
```

## Buses

A bus is a group of sounds that are turned up, down, filtered, or echoed
together, such as Music, Effects, and Voices. A sound plays through the bus its
`Bus` setting names, and `mixer.Bus(name)` gives the same bus to change.
Either one makes the bus the first time the name is used. Sounds without a bus
play straight to the mixer. Buses play into the mixer, not into each other.

| Member | Does |
|---|---|
| `Volume` | the bus's volume; slides over ten milliseconds when changed |
| `FadeTo(volume, seconds)` | changes the volume gradually |
| `Muted` | silences the bus without forgetting its volume |
| `LowPass`, `HighPass` | filters, in hertz; 0 is off |
| `Echo` | repeats of the sound, below |
| `Name()` | the name it was made with |

`MixerBus` is a handle; copies, and handles from asking by the same name again,
refer to the same bus. A mixer has at most 64 buses; past that, a new name
gives a bus whose sounds play straight to the mixer.

## Filters

```cpp
music.LowPass = 450;     // under water
radio.HighPass = 1200;   // through a small speaker
music.LowPass = 0;       // back to normal
```

`LowPass` keeps what is below its frequency and quietens what is above, as a
wall or water would. `HighPass` keeps what is above and quietens what is below,
like a telephone or a small speaker. Both fall away by 12 decibels for every
octave past their frequency. A frequency at or above most of what the mixer's
rate can hold turns the filter off, as 0 does. The same filters are on single
sounds, in their `LowPass` and `HighPass` settings.

## Echo

```cpp
hall.Echo = { .Delay = 0.25f, .Feedback = 0.35f, .Mix = 0.5f };
hall.Echo = { .Mix = 0.0f };   // off
```

| Setting | Default | Does |
|---|---|---|
| `Delay` | `0.25` | seconds between repeats, up to 2 |
| `Feedback` | `0.35` | how much of each repeat comes back in the next, from 0 to 0.95 |
| `Mix` | `0` | how loud the repeats are next to the sound; 0 turns echo off |

With a mix of 0.5 and a feedback of 0.5, a click repeats at half its loudness
after the delay, then a quarter, then an eighth.

## The mixer's volume

`mixer.Volume` turns everything down or up together, after the buses. Then the
sound passes a limiter: everything below 90% of full scale is untouched, and
louder peaks bend toward full scale instead of being cut off, so many sounds at
once do not crackle. Keeping the sum of loud sounds below full scale still sounds
best.

## Limitations

- There is no reverb, compressor, or equalizer beyond the two filters yet.
- Buses cannot play into other buses.
- Filters and echo cannot be put on the mixer itself; put the sounds on a bus.
