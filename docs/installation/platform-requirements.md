# Platform requirements

## What you need

- CMake 3.22 or later.
- A C++20 compiler whose standard library has `std::format`: Visual Studio 2022
  or later, GCC 13 or later, or Clang 17 or later with libc++ 17.
- For graphics, a shader compiler for each backend, once `graphics` exists:
  `dxc` from the Windows SDK for Direct3D 12.

## Platforms

Platforms arrive one at a time. Each brings its window, graphics, sound, and
network backends.

| Platform | Stage | Uses | Status |
|---|---|---|---|
| Windows | 1 | Win32, Direct3D 12, WASAPI, XInput, Winsock | `core`, `assets`, and `window` available |
| Linux | 2 | X11, Wayland, Vulkan, ALSA, PulseAudio | Planned |
| Web | 3 | Emscripten, WebGPU, Web Audio, WebSocket | Planned |
| macOS and iOS | 4 | Cocoa, UIKit, Metal, CoreAudio | Planned |
| Android | 5 | NDK, GameActivity, Vulkan, AAudio | Planned |

## Windows

Programs that use `window` need Windows 10 version 1703 or later, the version
that added per-monitor scaling for each window. `window` links these system
libraries, which every Windows installation has: `user32`, `gdi32`, `shell32`,
`imm32`, `dwmapi`, `shcore`, and `advapi32`. CMake passes them on to your
program by itself.

## What has been tested

`core`, `assets`, and `window` are built and tested on Windows 10 with Visual
Studio 2026, in both the Debug and Release configurations. The repository's
build workflow repeats this on GitHub's Windows runners on every push.

`core` and `assets` use only the C++ standard library apart from two small
Windows functions: writing to the debugger's output window, and finding the
program's folder. Their Linux versions are written, so they are expected to
build on Linux and macOS with the compilers above. They have not been built
there yet; that happens in stage 2, along with `window` for X11 and Wayland.
