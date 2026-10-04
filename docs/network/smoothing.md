# Smoothing values

```cpp
#include <easyforge/network.h>

using namespace easyforge;

Smoothed<Vector2> place;
SmoothedAngle facing;

client.OnMessage("Player", [&](const Message& message) {
    place.Add(message["Position"], message);
    facing.Add(message["Facing"], message);
});

window.OnFrame = [&](float deltaSeconds) {
    Vector2 shown = place.Get();
    float turned = facing.Get();
};
```

A value sent again and again, such as where another player is, reaches the
other end at uneven times: one message is held up for a moment, two come
together, one never comes. Drawing each value as it arrives makes things jump.
`Smoothed` shows the value a short moment in the past instead, blended between
the values sent around then, and uses when each value was sent rather than when
it arrived, so values sent at even times play back evenly.

## Adding and reading

| Written | Result |
|---|---|
| `Smoothed<Vector2> place;` | Holds nothing yet; `Get` gives a zero value |
| `Smoothed<float> height(2.0f);` | Starts at a value |
| `place.Add(value, message)` | Adds a value, stamped with when `message` was sent |
| `place.Add(value, sentAt, now)` | Adds a value sent at `sentAt` and arriving at `now`, both on `NetworkTime()`'s clock |
| `place.Get()` | The value to show now |
| `place.Get(now)` | The value to show at a given time, for tests and replays |
| `place.Reset(value)` | Jumps to `value` at once and forgets the values before it |
| `place.Newest()` | The value sent most recently, without smoothing |
| `place.HasValue()` | Whether any value was added |
| `place.Delay()` | How far in the past `Get` shows, in seconds |

`Smoothed` works with `float`, `double`, `Vector2`, `Vector3`, `Vector4`,
`Color`, and `Quaternion` (blended the short way round). `SmoothedAngle` does
the same for an angle in radians, turning the short way round, so an angle
going from just under pi to just over -pi passes through pi instead of
spinning back through zero. It returns angles from -pi to pi.

Call `Get` once a frame. The first two values show as they arrive; from the
third on, there are enough to tell how often values are sent, and playback
begins.

## How far in the past

The delay is enough to blend across one lost value, plus how late values have
been arriving lately, plus 4 milliseconds, and never more than half a second.
Values sent 20 times a second over a network that holds some up by 40
milliseconds are shown about 140 milliseconds in the past; values sent 60 times
a second on a calm local network, about 40.

The delay grows as soon as values come later than it allows, and shrinks again
only after a few calm seconds. It changes gradually: what is shown runs up to 5
percent slower while the delay grows and 2 percent faster while it shrinks,
which the eye does not catch, rather than jumping. Within a comfortable range it
does not change at all, so what is shown moves at exactly the speed it was
sent.

A gap of two or three intervals counts as lost values, not slower sending,
unless gaps stay that long, in which case the sender is taken to have slowed
down.

## When values stop

When no newer value comes, `Get` holds the last one rather than guessing where
it would go: a player whose messages stop stays put instead of drifting through
a wall. A value that arrives for a moment already shown is dropped.

`Reset` jumps at once and forgets earlier values, for when something is put
somewhere new on purpose, such as a player starting again: send a message
saying so, and reset the smoothed value when it arrives.

## When a message was sent

Every packet carries the clock of the computer that sent it, and every message
a program receives knows when it was sent: `message.SentAt()`, on this
computer's clock as `NetworkTime()` gives it. It is the moment the message would
have arrived if it had been held up no more than the quickest message on the
connection, so messages sent 50 milliseconds apart have `SentAt` times 50
milliseconds apart however unevenly they arrived. The two computers' clocks do
not need to agree. A message made on this computer has a `SentAt` of 0, which
`Add` counts as now.

Replies carry the time too: `reply.Message.SentAt()`.

## Limitations

- What is shown is behind what was sent by the delay above. For the player's
  own character, use the local value directly and smooth only what other
  computers send.
- `Smoothed` is used from one thread at a time. With a threaded client, add and
  get from the same thread, such as by copying values into the frame's thread.
- Values are blended in a straight line between the values sent. A body
  turning sharply between two values cuts the corner a little.
- Values are timed by when their packet left. Send each value when it is made,
  such as on frames where the physics stepped, rather than sending the same
  value again on frames where nothing moved, which would make it look as if it
  stopped and started.
