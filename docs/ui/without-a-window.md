# Without a window

`ui` does not need the window library. With your own window or engine, make a
root, pass it your events, and let it draw into a canvas each frame:

```cpp
#include <easyforge/graphics.h>
#include <easyforge/ui.h>

using namespace easyforge;

ui::Root root = ui::Root::New({ .Content = ui::Column({ .Padding = 16, .Children = { ui::Button("Play") } }) });

// For each event from your window, turned into an easyforge Event:
bool used = root.HandleEvent(event);     // true when the interface used it

// Each frame:
Canvas canvas = renderer.BeginFrame(Color::Hex("#202020"));
root.Draw(canvas, deltaSeconds);
renderer.EndFrame();
```

The root fills the canvas it is given, lays out what changed, moves animations
on by `deltaSeconds`, and draws. `HandleEvent` expects positions in points from
the top left of that canvas. `root.Content` changes what it shows.

A root without a window uses the light theme unless given one in
`RootSettings::Theme`, and does not follow the system's light or dark setting.
It keeps a clipboard of its own for text fields, and there is no input method
for text, cursor shape, or title bar, since those belong to a window.

## Your own window, with ui's help

To show `ui` in a window of your own through its slots instead, implement
[`Host`](../core/events-and-views.md#hosts) for your window. Every element
converts to a `View`, so your window can hold `std::shared_ptr<View> content =
ui::Column(...)`, attach it, and call it for events and frames as easyforge's
window does. The root then finds your host's renderer, cursor, clipboard, and
input method on its own.

## Limitations

- `Draw` needs a canvas from the `graphics` library.
- A root drawn into several canvases in one frame lays out for the last canvas
  it was given.
