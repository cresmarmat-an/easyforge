# Events

Everything the system tells a window arrives as an `Event`: key presses, typed
text, the mouse, touches, resizing, and more. The window passes each event to
its views, then to its listeners, then to `OnEvent`.

```cpp
window.OnEvent = [](const Event& event) {
    switch (event.Type)
    {
    case EventType::KeyPressed:
        Log("pressed {}", KeyName(event.Key));
        break;
    case EventType::TextEntered:
        Log("typed {}", event.Text);
        break;
    case EventType::MouseButtonPressed:
        Log("clicked at {} ({} clicks)", event.Position, event.ClickCount);
        break;
    case EventType::FilesDropped:
        for (const std::string& file : event.Files)
        {
            Log("dropped {}", file);
        }
        break;
    default:
        break;
    }
};
```

For games, the [input](../input/overview.md) library turns these events into
named actions such as "Jump", with gamepads included. The events are what it is
built on, and they are there for anything it does not cover.

## What each event carries

`Event` is one struct for every kind of event. Only the members listed for its
`Type` are set; the rest keep their defaults.

| Type | Members set |
|---|---|
| `KeyPressed`, `KeyReleased` | `Key`, `Repeat`, `Modifiers` |
| `TextEntered` | `Text`: the characters typed, as UTF-8 |
| `TextComposition` | `Text`: what an input method is composing; empty when it stops |
| `MouseMoved` | `Position`, `Movement`, `Modifiers` |
| `MouseButtonPressed`, `MouseButtonReleased` | `Button`, `Position`, `ClickCount`, `Modifiers` |
| `MouseWheel` | `Wheel`, `Position`, `Modifiers` |
| `MouseEntered`, `MouseLeft` | nothing |
| `TouchBegan`, `TouchMoved`, `TouchEnded`, `TouchCancelled` | `Touch`, `Position` |
| `Resized` | `Size`: the content's new size in points |
| `ScaleChanged` | `Scale`: the new pixels per point |
| `Minimized`, `Maximized`, `Restored` | nothing |
| `FocusGained`, `FocusLost` | nothing |
| `CloseRequested` | nothing; see [closing](overview.md#closing) |
| `FilesDropped` | `Files`: full paths as UTF-8, and `Position` |
| `ColorSchemeChanged` | nothing; ask the window for `SystemColorScheme()` |
| `Suspended`, `Resumed` | nothing; sent on phones and the web, which arrive later |

Positions and sizes are in points from the top left of the window's content.
With a [custom title bar](title-bars.md), the content starts at the top edge
of the window, so the title bar's area is included.

`event.Handled` is set by a view that used the event, such as an interface
button that was clicked. Everything later in line still receives the event and
decides for itself whether to skip it. `input` skips handled events, so typing
in a text field does not also make a character jump.

## Keys

`Key` names a key by where it is on the keyboard, using the names of a US
keyboard. `Key::W` is the key to the left of E whatever the keyboard layout,
so `W`, `A`, `S`, and `D` stay in the same place on a French keyboard, where
that key types Z. That is what game controls want. For the characters a person
types, use `TextEntered`.

```cpp
if (event.Type == EventType::KeyPressed && event.Key == Key::S && event.Modifiers.Control)
{
    Save();
}
```

| Member | Meaning |
|---|---|
| `Key` | The key, from `Key::A` to `Key::NumberPadEnter`; `Key::Unknown` for keys easyforge does not name |
| `Repeat` | True for the presses a held key sends by itself |
| `Modifiers` | `Shift`, `Control`, `Alt`, and `Meta` (the Windows key), held at the time |

`KeyName(key)` gives a name to show people, such as `"Left Shift"` or
`"Page Down"`.

Some details the window takes care of:

- When the window loses the keyboard, every key still held gets a
  `KeyReleased`, so nothing stays stuck down while the person is in another
  program.
- The AltGr key on many European keyboards is reported as `RightAlt`
  alone. Windows also sends a Control press with it, which is left out.
- Print Screen gets a `KeyPressed` before its `KeyReleased`. Windows only
  reports the release.
- Pressing Alt alone or F10 does nothing. Windows would otherwise wait for a
  menu the window does not have and swallow the next key.
- Alt+F4 and Alt+Space still work as usual.

## Text

`TextEntered` carries the text a person types, in their own layout and
language, one or more characters at a time. Backspace, Enter, Tab, and Escape
are not text; they arrive as `KeyPressed`. Characters outside the first 65,536
of Unicode, such as emoji, arrive whole.

Input methods, which people use to type Chinese, Japanese, and Korean, are off
until turned on, so their windows do not open during a game. Turn them on while
the person is typing into something, and tell the window where the text cursor
is, so the input method's window appears next to it:

```cpp
window.SetTextInput(true, { .X = 40, .Y = 120, .Width = 1, .Height = 20 });
// ... when typing is done:
window.SetTextInput(false);
```

Interfaces from `ui` do this by themselves when a text field has focus. While
an input method is composing, `TextComposition` events carry what it has so
far, and `TextEntered` carries the result. The input method also shows the
composition in its own small window.

## The mouse

`MouseMoved` gives the position and the `Movement` since the last move, both in
points. `MouseButtonPressed` counts clicks in `ClickCount`: 2 for a double
click, 3 for a triple click, using the system's double-click time and distance.
`MouseButton` is `Left`, `Right`, `Middle`, `Back`, or `Forward`.

While a button is held, the window keeps receiving the mouse even outside its
edges, so a drag that leaves the window still gets its release. `MouseEntered`
and `MouseLeft` say when the pointer comes over the content and leaves it.

`MouseWheel` gives `Wheel.Y` in notches, positive away from the person, and
`Wheel.X` for tilting wheels and touchpads, positive to the right. Touchpads
send fractions of a notch.

### A locked mouse

A camera turned by the mouse needs movement without the pointer stopping at the
edge of the screen:

```cpp
window.MouseLocked = true;

window.OnEvent = [&](const Event& event) {
    if (event.Type == EventType::MouseMoved)
    {
        camera.Turn(event.Movement.X * 0.002f, event.Movement.Y * 0.002f);
    }
};
```

While `MouseLocked` is true, the pointer is hidden and held in the middle of
the window, and `Movement` comes from the mouse itself, without the system's
pointer speed. The unit is the mouse's own counts, not points. `Position` stays
at the middle of the window. The lock lets go while another program has the
keyboard and comes back when the window gets it again.

## Touch

Touch screens send `TouchBegan`, `TouchMoved`, and `TouchEnded` for each
finger, told apart by `Touch`. `TouchCancelled` means the system took the
touch over, for example for a gesture. Touches are not turned into mouse
events. Pens are reported as the mouse.

## The window itself

`Resized` comes whenever the content's size changes, including while the
person drags an edge. `Minimized` is sent instead of a resize to nothing when
the window is minimized. `ScaleChanged` comes when the window moves to a screen
with different scaling, followed by `Resized`; see
[screens and scaling](screens-and-scaling.md).

`FilesDropped` comes when files are dragged from Explorer onto the window. It
lists every file dropped at once.

`ColorSchemeChanged` comes when the person switches Windows between light and
dark. A window whose `Background` was never set changes its background to
match.

## Limitations

- Keys easyforge does not name, such as media keys and the extra keys of some
  Japanese and Korean keyboards, arrive as `Key::Unknown`.
- Text arriving through an input method is shown in the input method's own
  window while it is composed; views cannot draw the composition in place yet.
- Pens are reported as the mouse, without pressure or tilt.
- Only files can be dropped on a window, not text or images from other
  programs.
