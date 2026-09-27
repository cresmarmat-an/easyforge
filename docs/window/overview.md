# window

`window` will open windows, run the frame loop, and pass on everything the
system sends: keys, the mouse, touches, resizing, display scaling, dropped
files, and on phones, the app being sent to the background.

> [!NOTE] Not available yet
> `window` is step 3 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- `Window::New({ .Title = ..., .Icon = ..., .Width = ..., .Height = ... })`
  opens an empty window on every platform, and `window.Run()` runs it.
- Settings such as `Title`, `Fullscreen`, and `Cursor` can be changed at any
  time by assigning them.
- `window.Content` and `window.TitleBar` hold what the window shows, such as an
  interface from `ui`, without `window` depending on `ui`.
- A custom title bar keeps the system's behavior: dragging, double-clicking to
  maximize, resizing from the edges, and the snap layouts on Windows 11.
- Clipboard, monitors, and several windows at once on the desktop.

Windows comes first with Win32, then X11 and Wayland, Cocoa, UIKit, Android, and
the browser.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#window-and-ui-the-usage).
