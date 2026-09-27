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

## Limitations

- A `Clock` measures time; it is not a timer that calls you back. For work that
  runs later, see [background jobs](background-jobs.md), and once `window` exists,
  its frame callback.
- The result is a `double`. Converting to `float` for per-frame arithmetic is
  fine, but keep long totals, such as the time since the program started, in
  `double`; after a few hours a `float` can no longer count single milliseconds.
