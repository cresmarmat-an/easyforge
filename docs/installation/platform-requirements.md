# Platform requirements

## What you need

- CMake 3.22 or later.
- A C++20 compiler whose standard library has `std::format`: Visual Studio 2022
  or later, GCC 13 or later, or Clang 17 or later with libc++ 17.
- Nothing else. `graphics` compiles its shaders while the program runs, with
  the shader compiler Windows includes.

## Platforms

Platforms arrive one at a time. Each brings its window, graphics, sound, and
network backends.

| Platform | Stage | Uses | Status |
|---|---|---|---|
| Windows | 1 | Win32, Direct3D 12, WASAPI, XInput, Winsock | `core`, `assets`, `window`, `input`, and `graphics` available |
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

`input` reads gamepads through XInput, which it loads when the program starts:
`xinput1_4.dll` on Windows 8 and later, or `xinput9_1_0.dll`. A computer without
either has no gamepads, and everything else keeps working.

`graphics` draws with Direct3D 12 on any GPU that supports feature level 11.0,
which covers GPUs from about 2012 on, and falls back to Windows' software
renderer when there is none. It links `d3d12`, `dxgi`, `d3dcompiler`, `dcomp`,
and `dxguid`, all part of Windows. Its tests draw with the software renderer, so
they give the same pixels on every computer, and run with Direct3D's validation
layer when the computer has the Graphics Tools feature installed.

## What has been tested

`core`, `assets`, `window`, `input`, and `graphics` are built and tested on
Windows 10 with Visual Studio 2026, in both the Debug and Release configurations. The repository's
build workflow repeats this on GitHub's Windows runners on every push.

`core` and `assets` use only the C++ standard library apart from two small
Windows functions: writing to the debugger's output window, and finding the
program's folder. Their Linux versions are written, so they are expected to
build on Linux and macOS with the compilers above. They have not been built
there yet; that happens in stage 2, along with `window` for X11 and Wayland.
`input` builds on every platform, and reads no gamepads outside Windows until
their stages.
