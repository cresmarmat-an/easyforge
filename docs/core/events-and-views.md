# Events, hosts, and views

These types in `core` let easyforge's libraries work together without depending
on each other. `window` produces events and is a host; `ui` elements are views;
`input` is a listener. Most programs use them through those libraries and never
name them. They matter when you write your own view, when you connect easyforge
to a window from another library, or when you write a library of your own.

```cpp
#include <easyforge/core.h>
```

| Type | What it is |
|---|---|
| `Event`, `EventType` | Something a window reports: a key, the mouse, a resize. The fields are described with the [window's events](../window/events.md) |
| `Key`, `MouseButton`, `KeyModifiers` | The names events use for keys and buttons |
| `View` | Anything that can fill a window |
| `Host` | What a view or a library can ask of the window it is in |
| `HostListener` | Something that follows a host's events and frames without drawing |
| `Surface` | The platform's own handles for a window, which a renderer draws into |

## Views

A view fills part of a window. A window has two slots for them, `Content` and
`TitleBar`, and calls the view in each for everything that happens:

```cpp
class Stopwatch final : public View
{
public:
    void Attach(Host& host) override { TheHost = &host; }
    void Detach() override { TheHost = nullptr; }
    void Place(easyforge::Rectangle area) override { Area = area; }

    void HandleEvent(Event& event) override
    {
        if (event.Type == EventType::KeyPressed && event.Key == Key::Space)
        {
            Running = !Running;
            event.Handled = true;
        }
    }

    void Frame(float deltaSeconds) override
    {
        if (Running)
        {
            Seconds += deltaSeconds;
        }
        // draw Seconds into Area with your renderer
    }

private:
    Host* TheHost = nullptr;
    easyforge::Rectangle Area;
    bool Running = false;
    float Seconds = 0.0f;
};

window.Content = std::make_shared<Stopwatch>();
```

| Function | Called |
|---|---|
| `Attach(host)` | When the view is put into a slot. Keep the host to ask it things later |
| `Detach()` | When the view is taken out, replaced, or the window closes. Do not use the host after this |
| `Place(area)` | After `Attach`, and whenever the view's area changes: its position and size in points, from the top left of the window |
| `HandleEvent(event)` | For every event. Set `event.Handled` when the view used it |
| `Frame(deltaSeconds)` | Once a frame, after the window's `OnFrame`. Update and draw here |
| `PreferredSize(available)` | Optional. A title bar view returns its height here |
| `HitTest(point)` | Optional. A title bar view says which parts drag the window and which are buttons ([custom title bars](../window/title-bars.md)) |

A view is in one slot of one window at a time. The window holds it with a
`std::shared_ptr`, so it lives at least as long as it is in the slot.

The title bar view is called before the content view, for events and for
frames.

## Hosts

`Host` is the window side. `window`'s `Window` is a host, and a window from
another library can implement `Host` to show easyforge views.

| Function | Result |
|---|---|
| `NativeSurface()` | The platform's handles, for a renderer |
| `Size()`, `PixelSize()`, `Scale()` | The content's size in points and in pixels, and pixels per point |
| `IsOpen()`, `IsFocused()`, `IsTransparent()` | |
| `Mode()` | `WindowMode::Normal`, `Minimized`, `Maximized`, or `Fullscreen` |
| `VerticalSync()` | Whether a renderer should wait for the screen before showing each frame |
| `ClaimSurface()`, `ReleaseSurface()` | A renderer claims the surface while it draws into it; a second claim fails |
| `SetCursor(cursor)` | Changes the mouse cursor |
| `SetTextInput(enabled, caret)` | Turns input methods on or off, placed next to the caret |
| `SystemColorScheme()` | `ColorScheme::Light` or `ColorScheme::Dark` |
| `ClipboardText()`, `SetClipboardText(text)` | The clipboard |
| `AddListener(listener)`, `RemoveListener(listener)` | See below |
| `Shared(name)` | One object per host that libraries share, found by name |

`Shared` is how two parts of a program find the same object for one window
without a global. `graphics` keeps a window's renderer there, so the `ui` in the
content slot and the `ui` in the title bar slot draw with the same one:

```cpp
std::shared_ptr<void>& slot = host.Shared("my-library.cache");
if (!slot)
{
    slot = std::make_shared<Cache>();
}
auto cache = std::static_pointer_cast<Cache>(slot);
```

Names starting with `easyforge.` belong to easyforge's libraries. Everything a
host shares is released when it closes.

## Listeners

A listener follows a host without being drawn. `input`'s controls are one: they
watch key presses and track which keys are held.

```cpp
class KeyCounter final : public HostListener
{
public:
    void HandleEvent(const Event& event) override
    {
        if (event.Type == EventType::KeyPressed && !event.Handled)
        {
            ++Presses;
        }
    }
    void FrameStarted(float) override {}
    void FrameEnded() override {}

    int Presses = 0;
};

KeyCounter counter;
std::shared_ptr<Host> host = window;
host->AddListener(counter);
// ...
host->RemoveListener(counter);
```

Listeners get each event after the views, so `event.Handled` tells them whether
a view already used it. `FrameStarted` comes before the window's `OnFrame`,
and `FrameEnded` after the views have drawn. A listener must stay alive until
it is removed. Removing one while events are being delivered is safe.

## The order of everything

For each event:

1. The title bar view's `HandleEvent`, then the content view's.
2. Each listener's `HandleEvent`, in the order they were added.
3. The window's `OnEvent`.

For each frame:

1. Each listener's `FrameStarted`.
2. The window's `OnFrame`.
3. The title bar view's `Frame`, then the content view's.
4. Each listener's `FrameEnded`.

## Keys and names

`Key` names keys by their position on a US keyboard, so `Key::W` is the same
key on every layout; the [window's events](../window/events.md#keys) explain
why. `KeyName(key)` gives a name for people, such as `"Left Shift"`.
`Key::Count` and `MouseButton::Count` are the number of values, for arrays
indexed by key or button.

## Limitations

- `Host` has no way yet for a view to ask for a frame only when something
  changes; hosts run frames continuously.
