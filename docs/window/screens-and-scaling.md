# Screens and scaling

Screens differ in how many pixels they pack into an inch, and people set Windows
to scale everything up to 125%, 150%, or more to match. easyforge measures
windows and events in points so programs look the same size on every screen,
and only renderers deal in pixels.

```cpp
Window window = Window::New({ .Width = 800, .Height = 600 });

float scale = window.Scale();           // 1.5 on a screen at 150%
Vector2 points = window.Size;           // { 800, 600 }
Vector2 pixels = window.PixelSize();    // { 1200, 900 }
```

## Points and pixels

A point is one pixel at 100% scaling. `window.Scale()` is the number of pixels
per point on the screen the window is on.

| In points | In pixels |
|---|---|
| `WindowSettings.Width`, `Height`, `MinimumWidth`, `MinimumHeight` | `window.PixelSize()` |
| The properties `Width`, `Height`, `Size`, `MinimumWidth`, `MinimumHeight` | `WindowSettings.Position` and the `Position` property |
| Every position and size in an [event](events.md) | `Monitor.Area` and `Monitor.WorkArea` |

Positions on the desktop are in pixels because screens side by side can have
different scaling, and points would not line up across them.

## Moving between screens

When a window moves to a screen with different scaling, Windows asks for its new
size, and `window` answers with the size that keeps the content the same number
of points. The window grows or shrinks on screen to look the same size, then
reports `Resized` and `ScaleChanged`. The icon is rebuilt at the new size too.

easyforge tells Windows at startup that it handles scaling for each screen, so
Windows never stretches a blurry copy of the window. A program that already
told Windows something else keeps its own choice.

## Monitors

```cpp
for (const Monitor& monitor : Monitor::All())
{
    Log("{}: {} at {}% and {} Hz", monitor.Name, monitor.Area, monitor.Scale * 100, monitor.RefreshRate);
}
```

| Member | Meaning |
|---|---|
| `Name` | The name the screen reports, such as `"DELL U2720Q"` |
| `Area` | The whole screen on the desktop, in pixels |
| `WorkArea` | The part the taskbar does not cover |
| `Scale` | Pixels per point on that screen |
| `RefreshRate` | Frames per second the screen shows |
| `IsPrimary` | True for the primary screen, whose top left is `{ 0, 0 }` |

`Monitor::All()` lists every screen with the primary one first, and
`Monitor::Primary()` gives just that one. `window.Monitor()` gives the screen
that holds most of a window.

To open a window on a particular screen, place it inside that screen's area:

```cpp
std::vector<Monitor> monitors = Monitor::All();
const Monitor& second = monitors.size() > 1 ? monitors[1] : monitors[0];
Window window = Window::New({
    .Width = 800,
    .Height = 600,
    .Position = Vector2 { second.WorkArea.X + 40, second.WorkArea.Y + 40 },
});
```

A window starts at its screen's scale, so the 800 by 600 points above are the
right size on the second screen even when it scales differently from the first.

## Limitations

- Positions can only be given in desktop pixels, not relative to a screen.
- Screens being plugged in or unplugged are not reported as events; call
  `Monitor::All()` again to see the current list.
- A screen's refresh rate is reported in whole numbers of frames per second.
