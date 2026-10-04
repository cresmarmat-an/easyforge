# Your first window

This page builds a program that opens a window, reacts to the keyboard and the
mouse, and closes cleanly. It needs CMake 3.22 or later, a C++20 compiler, and
Windows for now.

## The project

Two files in an empty folder. `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(first_window LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(easyforge
    GIT_REPOSITORY https://github.com/cresmarmat-an/easyforge.git
    GIT_TAG v0.0.1)
FetchContent_MakeAvailable(easyforge)

add_executable(first_window main.cpp)
target_link_libraries(first_window PRIVATE easyforge::window)
```

`main.cpp`:

```cpp
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "First window", .Width = 800, .Height = 500 });
    if (!window)
    {
        Log(LogLevel::Error, window.Error());
        return 1;
    }

    window.OnEvent = [window](const Event& event) {
        if (event.Type == EventType::KeyPressed && event.Key == Key::Escape)
        {
            window.Close();
        }
        if (event.Type == EventType::MouseButtonPressed)
        {
            Log("clicked at {}", event.Position);
        }
    };

    float seconds = 0.0f;
    int shown = -1;
    window.OnFrame = [&](float deltaSeconds) {
        seconds += deltaSeconds;
        if (static_cast<int>(seconds) != shown)
        {
            shown = static_cast<int>(seconds);
            window.Title = std::format("First window - {} seconds", shown);
        }
    };

    window.Run();
    Log("open for {:.1f} seconds", seconds);
}
```

Build and run it:

```
cmake -S . -B build
cmake --build build
build\Debug\first_window.exe
```

A window 800 by 500 points opens in the middle of the screen. Its title counts
the seconds, clicks are logged to the console, and Escape or the close button
ends the program.

## What happened

- `Window::New` opened the window with the settings in braces. Anything left
  out has a default; [the window overview](../window/overview.md#settings)
  lists them all.
- `OnEvent` receives [every event](../window/events.md): keys, text, the mouse,
  resizing.
- `OnFrame` runs once a frame, at the screen's refresh rate, with the seconds
  since the last frame.
- Assigning `window.Title` changes the title bar at once. Every setting works
  that way.
- `window.Run()` returns once the window has closed.

The lambda for `OnEvent` captures the window by value. A `Window` is a handle,
so the copy refers to the same window.

## Next

- Give the program an icon with [`easyforge_app_icon`](../installation/app-icons.md).
- Draw your own [title bar](../window/title-bars.md).
- Read about [points and screen scaling](../window/screens-and-scaling.md).
