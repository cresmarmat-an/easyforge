# window

`window` opens windows, runs the frame loop, and passes on everything the
system sends: keys, the mouse, touches, resizing, display scaling, and dropped
files. It draws nothing itself. What a window shows comes from the `graphics`
and `ui` libraries, or from your own code, through the window's `Content` and
`TitleBar` slots.

```cmake
target_link_libraries(my_program PRIVATE easyforge::window)
```

```cpp
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({
        .Title = "Notes",
        .Icon = "icon.png",
        .Width = 1280,
        .Height = 720,
    });
    if (!window)
    {
        Log(LogLevel::Error, window.Error());
        return 1;
    }

    window.OnFrame = [](float deltaSeconds) {
        // runs once a frame, at the screen's refresh rate
    };

    window.Run();
}
```

That opens an empty window in the middle of the primary screen and returns from
`Run` when it is closed. The same program is
[01-empty-window](https://github.com/cresmarmat-an/easyforge-examples/tree/main/01-empty-window)
in the examples.

`window` is available on Windows. Linux, the web, macOS, iOS, and Android
follow in later stages.

## Settings

`Window::New` takes a `WindowSettings`. C++ requires designated initializers in
the order the fields are declared, which is the order of this table. Leave out
anything you do not need.

| Setting | Default | Meaning |
|---|---|---|
| `Title` | `"easyforge"` | The text in the title bar and the taskbar |
| `Icon` | empty | An image file for the window's icon; empty uses the program's own icon ([icons](icons-and-cursors.md)) |
| `Width`, `Height` | 1280, 720 | The size of the content in points, not counting the system's frame |
| `MinimumWidth`, `MinimumHeight` | 0, 0 | The smallest the person can make the content |
| `Position` | empty | The top left of the content in desktop pixels; empty centers the window on the primary screen |
| `Resizable` | true | Whether the edges can be dragged and the window maximized |
| `Maximized` | false | Start maximized |
| `Fullscreen` | false | Cover the whole screen, without a frame |
| `AlwaysOnTop` | false | Stay above other windows |
| `Visible` | true | Show the window at once |
| `Transparent` | false | See-through wherever the content is drawn with alpha below 1 |
| `VerticalSync` | true | Frames wait for the screen, so they come at its refresh rate |
| `Background` | empty | The color shown where nothing is drawn; empty follows the system's light or dark setting |

Sizes are in points. A point is one pixel at 100% display scaling and 1.5
pixels at 150%, so a window 1280 points wide looks the same size on every
screen. [Screens and scaling](screens-and-scaling.md) explains points, pixels,
and moving between screens.

A window that would not fit on its screen starts at the largest size that
does, so it never reaches under the taskbar.

`Position` is a `std::optional<Vector2>`. Write the type out when you set it:
`.Position = Vector2 { 100, 100 }`.

## Changing a window while it runs

Every setting except `Transparent` is also a [property](../core/properties.md)
of the same name, so it can be read and changed at any time:

```cpp
window.Title = "Notes - untitled.txt";
window.Size = { 800, 600 };
window.Fullscreen = true;
window.Cursor = Cursor::Hand;

std::string title = window.Title;
float width = window.Width;
if (window.Maximized)
{
    // ...
}
```

| Property | Type | Notes |
|---|---|---|
| `Title`, `Icon` | `std::string` | |
| `Width`, `Height` | `float` | The content's size in points |
| `Size` | `Vector2` | Both at once, with one resize |
| `MinimumWidth`, `MinimumHeight` | `float` | |
| `Position` | `Vector2` | The top left of the content, in desktop pixels |
| `Resizable`, `Maximized`, `Minimized`, `Fullscreen`, `AlwaysOnTop`, `Visible`, `VerticalSync` | `bool` | |
| `Background` | `Color` | |
| `Cursor` | `Cursor` | See [icons and cursors](icons-and-cursors.md) |
| `MouseLocked` | `bool` | See [events](events.md#a-locked-mouse) |
| `Content`, `TitleBar` | `std::shared_ptr<View>` | What the window shows; see below |
| `OnFrame` | `std::function<void(float)>` | Called once a frame with the seconds since the last frame |
| `OnEvent` | `std::function<void(const Event&)>` | Called for every [event](events.md) |
| `OnCloseRequested` | `std::function<bool()>` | Called when the person tries to close the window |

Changing the size of a maximized, minimized, or full screen window changes the
size it goes back to.

Other things a window can tell you or do:

| Written | Result |
|---|---|
| `window.IsOpen()` | False once the window has closed |
| `window.Close()` | Closes it at once, without calling `OnCloseRequested` |
| `window.Focus()` | Brings it to the front and gives it the keyboard |
| `window.IsFocused()` | Whether it has the keyboard |
| `window.Scale()` | Pixels per point on the window's screen |
| `window.PixelSize()` | The content's size in pixels |
| `window.Monitor()` | The screen that holds most of the window |
| `window.SystemColorScheme()` | `ColorScheme::Light` or `ColorScheme::Dark` |
| `window.ClipboardText()`, `window.SetClipboardText(text)` | The system clipboard, as UTF-8 text |
| `window.SetTextInput(enabled, caret)` | Turns input methods for Chinese, Japanese, and Korean on or off ([events](events.md#text)) |
| `window.SetIcon(image)`, `window.SetCursorImage(image, hotSpot)` | Icons and cursors from images in memory |
| `window.NativeSurface()` | The platform's own window handle |
| `window.IsTransparent()` | True for a window made with `Transparent` |

## Running frames

`window.Run()` handles what the system sends and runs frames until that window
closes. Each frame it:

1. Delivers the events that arrived since the last frame, to the views, the
   [listeners](../core/events-and-views.md#listeners), and `OnEvent`, in that
   order.
2. Calls `OnFrame` with the seconds since the last frame.
3. Calls `Frame` on the title bar view, then the content view, which is where
   `ui` and `graphics` draw.
4. Waits for the screen's next refresh, unless `VerticalSync` is off or a
   renderer from `graphics` is already waiting for it.

While the person drags an edge or moves the window, Windows holds the program
in a loop of its own. `window` keeps running frames during that time, so the
content keeps drawing at the new size instead of freezing.

A minimized or hidden window still runs frames, about 60 a second, so timers
and network code keep going.

Programs that already have a loop of their own call
`Window::RunOneFrame()` instead of `Run`. It does one round of the steps above
for every open window and returns.

## Closing

When the person clicks the close button or presses Alt+F4, the window calls
`OnCloseRequested`. Return `false` to keep it open, for example to ask about
unsaved changes first. Without `OnCloseRequested`, the window closes.

```cpp
window.OnCloseRequested = [&] {
    if (!document.HasChanges())
    {
        return true;
    }
    ShowSavePrompt();
    return false;
};
```

`window.Close()` closes it without asking. Once closed, the window detaches its
views, lets go of its callbacks, and ignores changes to its properties. The
handle still tests as true, since the window was made; `IsOpen()` says whether it
is still open.

## Several windows

`Window::New` can be called at any time, including from inside `OnFrame`. All
open windows run their frames together, whichever one `Run` was called on, and
`Run` returns when its own window closes. The others stay open until they are
closed too.

```cpp
Window main = Window::New({ .Title = "Editor" });
Window tools = Window::New({ .Title = "Tools", .Width = 300, .Height = 600 });
main.Run();    // tools keeps running while main is open
```

## Handles

A `Window` is a handle. Copies refer to the same window, and capturing one by
value in a lambda is safe:

```cpp
window.OnEvent = [window](const Event& event) {
    if (event.Type == EventType::KeyPressed && event.Key == Key::Escape)
    {
        window.Close();
    }
};
```

An open window stays open until it is closed, even when no handle refers to it
any more. `Window window;` makes a handle to no window, which tests as false.

Windows belong to the thread that made them. Make them and use them from one
thread, normally the main one.

## What goes in a window

`window.Content` and `window.TitleBar` hold anything that implements
[`View`](../core/events-and-views.md#views) from `core`. `window` does not
know about `ui`, and `ui` elements are views, so `window.Content = ui::Column(...)`
works while neither library depends on the other. Your own code can implement
`View` too, to draw with your own renderer.

A title bar view replaces the system's title bar;
[custom title bars](title-bars.md) explains how.

Libraries that work with any window take it as a `Host`. `graphics` and
`input` will, when they arrive: `Renderer::New(window)` and
`Controls::New(window)`. A `Window` converts to `std::shared_ptr<Host>` for
them, and `window.AsHost()` does the same by name.

## Without a console window

A program whose `main` is built as a console program opens a console window
next to its own window on Windows. That is useful while developing, since `Log`
writes there. To leave it out, build the program for the Windows subsystem and
keep `main` as the entry point:

```cmake
if(WIN32)
    set_target_properties(notes PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(notes PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()
```

`Log` still reaches the debugger's output window when one is attached.

## Limitations

- Windows only for now. Linux (X11 and Wayland), the web, macOS, iOS, and
  Android arrive with their stages.
- Windows 10 version 1703 or later, because points need the per-monitor
  scaling that version added.
- Full screen covers the screen with a borderless window. It does not change
  the screen's resolution.
- A window made with `Transparent` shows nothing where nothing draws; with no
  renderer it is invisible apart from its frame.
- There is no menu bar, and no dialogs for opening files or showing messages.
- Frames run all the time while a window is open. There is no mode that only
  runs frames when something changes, so an idle window still uses a little
  processor time each frame.
