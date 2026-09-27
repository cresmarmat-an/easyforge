# input

`input` turns keys, mouse buttons, and gamepads into named actions such as
"Jump" and "Move", so a program asks what the player wants to do instead of
which key was pressed. The same action can be bound to a key and a gamepad
button at once, and bindings can be changed and saved while the program runs.

```cmake
target_link_libraries(my_game PRIVATE easyforge::window easyforge::input)
```

```cpp
#include <easyforge/input.h>
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "Game" });
    Controls controls = Controls::New(window);

    controls.Bind("Jump", { Key::Space, GamepadButton::South });
    controls.Bind("Move", { Stick::Left, KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W } });
    controls.Bind("Fire", { MouseButton::Left, GamepadButton::RightTrigger });

    Vector2 position;
    window.OnFrame = [&](float deltaSeconds) {
        position += controls.Axis("Move") * 5.0f * deltaSeconds;
        if (controls.Pressed("Jump"))
        {
            Log("jump from {}", position);
        }
    };

    window.Run();
}
```

`input` depends only on `core`. It works with a `window`, with any other
[host](../core/events-and-views.md#hosts), or with events you pass in yourself.

## Following a window

`Controls::New(window)` makes controls that follow the window by themselves:
they receive its events and see each frame start, so the actions are up to date
in `OnFrame`. Everything that happened since the last frame is taken into
account at once, when the frame starts.

Events a view already used are skipped. When a text field in the interface has
the keyboard, typing a space does not also make the player jump. Releases always
count, so a key cannot get stuck down. Turn this off with
`controls.SkipHandledEvents = false`.

Without a window, make the controls with `Controls::New()`, pass them each event
with `Handle`, and call `NextFrame` once a frame before reading:

```cpp
Controls controls = Controls::New();
controls.Bind("Jump", Key::Space);

// every frame:
for (const Event& event : eventsFromYourWindow)
{
    controls.Handle(event);
}
controls.NextFrame(deltaSeconds);
if (controls.Pressed("Jump")) { /* ... */ }
```

## Reading actions

| Written | Result |
|---|---|
| `controls.Pressed("Jump")` | True in the frame the action started being held |
| `controls.Held("Jump")` | True while any of its bindings is held |
| `controls.Released("Jump")` | True in the frame it stopped being held |
| `controls.Value("Accelerate")` | How far it is held, from 0 to 1: part way for a trigger or stick |
| `controls.Axis("Move")` | The direction of its sticks and key axes together, Y positive for up, no longer than 1 |

A key pressed and released within the same frame still gives `Pressed` and
`Released` in that frame, so short taps are never lost. When an action has
several bindings, it is held while any of them is, and pressing a second one
while the first is held is not a new press.

The same questions work on a single binding, without an action:
`controls.Held(Key::LeftShift)`, `controls.Pressed(GamepadButton::Start)`.

Other things the controls keep for each frame:

| Written | Result |
|---|---|
| `controls.MousePosition()` | The mouse, in points from the top left of the window |
| `controls.MouseMovement()` | How far the mouse moved this frame |
| `controls.Wheel()` | How far the wheel turned this frame |
| `controls.Text()` | The text typed this frame, as UTF-8 |
| `controls.Gamepads()` | Each gamepad as read this frame ([gamepads](gamepads.md)) |
| `controls.LastPressed()` | The first key or button pressed this frame, for "press a key" screens |
| `controls.FrameSeconds()` | The seconds this frame took, or while playing a recording, the seconds the recorded frame took |

## More

- [Bindings](bindings.md): every kind of binding, changing them while the program
  runs, and saving them to a file.
- [Gamepads](gamepads.md): buttons, sticks, triggers, dead zones, rumble, and one
  player per gamepad.
- [Recording and playback](recording.md): repeating a session exactly.

## Settings

`Controls::New` takes a `ControlsSettings` after the host. Each is also a
property.

| Setting | Default | Meaning |
|---|---|---|
| `Gamepad` | `AnyGamepad` | Which gamepad to read, 0 to 3, or all of them together |
| `DeadZone` | 0.2 | How far a stick moves before it counts |
| `SkipHandledEvents` | true | Skip presses a view already used |

```cpp
Controls playerTwo = Controls::New(window, { .Gamepad = 1 });
```

`Controls` is a handle: copies share their bindings and state.

## Limitations

- Touches are not bound to actions yet; read them from the window's
  [events](../window/events.md#touch).
- Gamepads are read through XInput, so on Windows only Xbox controllers and
  controllers that act like one are seen, up to four. Others arrive with the
  later platforms.
- Actions are read once a frame. A program that needs every event as it happens
  reads the window's events directly.
