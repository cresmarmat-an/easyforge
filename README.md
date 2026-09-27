# easyforge

C++ libraries for windows, input, graphics, interfaces, sound, physics,
networking, data, and scripting, written from scratch for Windows, Linux,
macOS, iOS, Android, and the web.

**[Documentation](https://cresmarmat-an.github.io/easyforge/)** ·
[Examples](https://github.com/cresmarmat-an/easyforge-examples) ·
[Changelog](CHANGELOG.md)

Each library works on its own or together with the others. easyforge has no
third-party code: it uses only the C++ standard library and each platform's own
SDK.

## Status

easyforge is at **0.0.1-alpha**. The `core`, `assets`, and `window` libraries
are finished and tested on Windows. The other libraries are being built one at a time, in the
order listed below; each one arrives with its tests, its documentation, and an
example.

| Library | What it does | Status |
|---|---|---|
| `core` | Math, colors, properties, results, logging, background jobs, testing | Available |
| `assets` | Reads images, 3D models, sounds, and fonts from files | Available |
| `window` | Windows, the frame loop, and everything the system sends | Available |
| `input` | Named actions from keyboard, mouse, touch, and gamepads | Planned |
| `graphics` | 2D and 3D drawing on Direct3D 12, Vulkan, Metal, and WebGPU | Planned |
| `data` | A tree-shaped table with change tracking, undo, and saving | Planned |
| `ui` | Interfaces: layout, elements, themes, effects, and displays | Planned |
| `script` | The easyforge scripting language, embeddable in any program | Planned |
| `sound` | Mixing, streaming, effects, and positional sound | Planned |
| `physics` | Bodies, collisions, and joints in 2D, later 3D | Planned |
| `network` | One-way messages and two-way requests between programs | Planned |

## A first program

```cpp
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "Notes", .Icon = "icon.png", .Width = 1280, .Height = 720 });

    window.OnEvent = [window](const Event& event) {
        if (event.Type == EventType::KeyPressed && event.Key == Key::Escape)
        {
            window.Close();
        }
    };

    window.Run();
}
```

That is an empty window that closes with Escape. When `ui` is ready, a window
with an interface will look like this:

```cpp
#include <easyforge/window.h>
#include <easyforge/ui.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "Notes", .Icon = "icon.png", .Width = 1280, .Height = 720 });

    window.Content = ui::Column({
        .Padding = 24,
        .Gap = 12,
        .Children = {
            ui::Label("Hello", { .FontSize = 32 }),
            ui::Button("Click me", { .OnClick = [] { /* ... */ } }),
        },
    });

    window.Run();
}
```

## Getting it

Fetch it with CMake and link the libraries you use:

```cmake
include(FetchContent)
FetchContent_Declare(easyforge
    GIT_REPOSITORY https://github.com/cresmarmat-an/easyforge.git
    GIT_TAG main)
FetchContent_MakeAvailable(easyforge)

target_link_libraries(my_program PRIVATE easyforge::window)
```

Only the libraries you link are compiled. easyforge needs CMake 3.22 or later
and a C++20 compiler; on Windows that is Visual Studio 2022 or later.
[Installation](https://cresmarmat-an.github.io/easyforge/installation/fetching-with-cmake.html)
also covers installing it once and using `find_package`, and every build option.

## Building and testing

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Documentation

The documentation is at
**[cresmarmat-an.github.io/easyforge](https://cresmarmat-an.github.io/easyforge/)**.
Its pages are the Markdown files in [`docs/`](docs), and the site is rebuilt
every time they change. [`DESIGN.md`](DESIGN.md) describes the whole plan,
including the libraries that are not written yet.

## License

easyforge is released under the [MIT License](LICENSE). Copyright (c) 2026
Cresmar Mat-an.
