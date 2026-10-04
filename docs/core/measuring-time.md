# Measuring time

A `Clock` measures the time since it was made or last restarted.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Clock frameClock;

while (running)
{
    float deltaSeconds = static_cast<float>(frameClock.Restart());
    Update(deltaSeconds);
}
```

| Written | Result |
|---|---|
| `Clock clock;` | Starts counting now |
| `clock.Seconds()` | Seconds since it started, as a `double` |
| `clock.Restart()` | Returns the seconds since it started, and starts counting from zero again |

`Restart` is how a frame loop measures each frame: the value it returns is the
time the last frame took.

The clock uses the system's steady clock, which only moves forward. Changing
the computer's date or time, or the clocks going forward for summer time, does
not affect it.

## Steady frame times

When frames are shown in step with the screen, the screen shows them at even
intervals, but the time measured between them wobbles. Moving things by the
measured time makes them move unevenly. A `FrameTimer` turns measured times
into whole refreshes of the screen, carrying what it rounds off into later
frames:

```cpp
FrameTimer timer;
timer.SetRefreshRate(60.0f);

while (running)
{
    float deltaSeconds = static_cast<float>(timer.Next(frameClock.Restart()));
    Update(deltaSeconds);
}
```

| Written | Result |
|---|---|
| `timer.SetRefreshRate(hertz)` | The screen's rate; 0 turns the evening out off |
| `timer.Next(measured)` | The seconds to use for a frame that measured `measured` |
| `timer.RefreshInterval()` | The screen's refresh interval as followed so far, or 0 when off |

- Over many frames the times given add up to the times measured, give or take
  part of one refresh still being carried.
- A frame that took two or more refreshes is given two or more refreshes. A
  frame that started late and the early one after it are given one refresh each,
  which is how the screen showed them.
- The timer follows the screen's true rate, since screens say they run at 60
  hertz when they run at 59.94: after a few seconds the times given match the
  screen's true interval to within a few microseconds.
- Frames longer than eight refreshes, frames that keep coming faster than the
  screen refreshes, and extra frames within one refresh are given as measured.
- Calling `SetRefreshRate` with the same rate keeps what was learned; another
  rate, as when a window moves to a different screen, starts over.

`Window` already does this for its frames, so `OnFrame` gets steady times; see
[frame times](../window/overview.md#frame-times).

## Limitations

- A `Clock` measures time; it is not a timer that calls you back. For work that
  runs later, see [background jobs](background-jobs.md), or a window's
  [frame callback](../window/overview.md).
- The result is a `double`. Converting to `float` for per-frame arithmetic is
  fine, but keep long totals, such as the time since the program started, in
  `double`; after a few hours a `float` can no longer count single milliseconds.
