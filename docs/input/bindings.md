# Bindings

A binding is what an action listens to. `Bind` adds bindings to an action; an
action can have as many as you like.

```cpp
controls.Bind("Jump", Key::Space);
controls.Bind("Jump", GamepadButton::South);                    // adds a second
controls.Bind("Fire", { MouseButton::Left, GamepadButton::RightTrigger });
```

## Kinds of binding

| Binding | Example | Held | Value | Axis |
|---|---|---|---|---|
| A key | `Key::Space` | While down | 0 or 1 | nothing |
| A mouse button | `MouseButton::Left` | While down | 0 or 1 | nothing |
| The wheel | `WheelDirection::Up` | In a frame the wheel turned that way | 0 or 1 | nothing |
| A gamepad button | `GamepadButton::South` | While down | 0 or 1; triggers give how far they are pulled | nothing |
| One gamepad axis | `GamepadAxis::RightTrigger` | Past halfway | How far, from 0 to 1 | Along X, or Y for the sticks' up and down |
| A stick | `Stick::Left` | Past halfway | How far it is pushed | Its direction |
| Four keys | `KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W }` | Past halfway | 1 while a direction is held | The direction the keys give |

Keys are named by their position on the keyboard, so `Key::W` is the same key
on every layout; see the window's [keys](../window/events.md#keys).

A `KeyAxis` can leave keys out. `KeyAxis { .Left = Key::Left, .Right = Key::Right }`
gives a single axis for steering. Two keys at once give a diagonal no longer
than 1, and opposite keys cancel.

Each turn of the wheel counts as a new press, even in consecutive frames, so
"next weapon" on `WheelDirection::Up` steps once for every turn.

## Changing bindings

```cpp
controls.Unbind("Jump");                          // the action has no bindings
controls.Bind("Jump", Key::W);

std::vector<Binding> jump = controls.Bindings("Jump");
std::vector<std::string> actions = controls.Actions();    // sorted by name
controls.UnbindAll();
```

A settings screen that asks the player to press a key uses `LastPressed`:

```cpp
if (std::optional<Binding> pressed = controls.LastPressed())
{
    controls.Unbind("Jump");
    controls.Bind("Jump", *pressed);
    Log("Jump is now {}", pressed->Name());
}
```

`LastPressed` gives the first key, mouse button, wheel turn, or gamepad button
that went down this frame.

`binding.Name()` is a name to show the player: `"Space"`, `"Left Mouse Button"`,
`"South Button"`, `"Left Stick"`, `"W A S D"`. `binding.As<Key>()` gives the key
a binding was made from, or null for any other kind.

## Saving bindings

```cpp
controls.SaveBindings("bindings.txt");
// ...
if (Result<> loaded = controls.LoadBindings("bindings.txt"); !loaded)
{
    Log(LogLevel::Warning, loaded.Error());
}
```

The file has one action a line, with the same words as the code:

```
# Player one
Fire = MouseButton Left, GamepadButton RightTrigger
Jump = Key Space, GamepadButton South
Move = Stick Left, KeyAxis A D S W
Next weapon = Wheel Up
Steer = KeyAxis Left Right None None
```

A `KeyAxis` lists its keys as left, right, down, up, with `None` for a key left
out. `#` starts a comment. Loading replaces every binding; if any line is wrong,
nothing changes and the error names the line:
`"bindings.txt: line 3: 'Spacebar' is not a key easyforge knows"`.

`BindingsText()` and `SetBindingsText(text)` do the same with text in memory,
for keeping bindings inside a larger settings file.

## Limitations

- A binding is one key or button. Combinations such as Control+S are read by
  checking both: `controls.Pressed(Key::S) && controls.Held(Key::LeftControl)`.
- Touches and mouse movement cannot be bound; read them from the controls or
  the window directly.
