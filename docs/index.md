# easyforge

C++20 libraries for apps and games, written from scratch.

easyforge covers windows, input, graphics, interfaces, sound, physics,
networking, data, and scripting, for Windows, Linux, macOS, iOS, Android, and
the web. Each library works on its own or together with the others, and none of
them contains third-party code.

> [!NOTE] Windows first
> easyforge 0.0.1 has every library, tested on Windows. Linux, the web, macOS
> and iOS, and Android come in later stages; [Capabilities and
> limitations](reference/capabilities-and-limitations.md) lists what works
> today.

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

That is an empty window that closes with Escape.
[Fetching with CMake](installation/fetching-with-cmake.md) shows how to add
easyforge to your own project.

## The libraries

| Library | What it does | Status |
|---|---|---|
| [core](core/overview.md) | Math, colors, properties, results, logging, background jobs, testing | Available |
| [assets](assets/overview.md) | Reads images, 3D models, sounds, and fonts from files | Available |
| [window](window/overview.md) | Windows, the frame loop, and everything the system sends | Available |
| [input](input/overview.md) | Named actions from keyboard, mouse, and gamepads | Available |
| [graphics](graphics/overview.md) | 2D and 3D drawing, on Direct3D 12 now and Vulkan, Metal, and WebGPU later | Available on Windows |
| [data](data/overview.md) | A tree-shaped table with change tracking, undo, and saving | Available |
| [ui](ui/overview.md) | Interfaces: layout, elements, themes, effects, and displays | Available on Windows |
| [script](script/overview.md) | The easyforge scripting language, embeddable in any program | Available |
| [sound](sound/overview.md) | Mixing, streaming, effects, and positional sound | Available |
| [physics](physics/overview.md) | Bodies, collisions, and joints in 2D, later 3D | Available |
| [network](network/overview.md) | One-way messages and two-way requests between programs | Available on Windows |

## Where to go next

- [Introduction](getting-started/introduction.md): what easyforge is made of and
  how the libraries depend on each other.
- [Your first window](getting-started/first-window.md): a program that opens a
  window and reacts to the keyboard and mouse.
- [Names and patterns](getting-started/names-and-patterns.md): the conventions
  every library follows, so learning one teaches the others.
- [core overview](core/overview.md): everything in the library you can use today.
