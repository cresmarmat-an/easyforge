# Roadmap

easyforge is built in stages. Each stage ends with something that runs, its
tests, and its documentation pages, and each platform brings its own window,
graphics, sound, and network backends while the libraries' interfaces stay the
same.

| Stage | Adds | State |
|---|---|---|
| 1 | Windows, with every library | Released as 0.0.1 |
| 2 | Linux | Next |
| 3 | The web | Planned |
| 4 | macOS and iOS | Planned |
| 5 | Android | Planned |
| 6 | 3D grows up | Planned |

## Stage 1: Windows

Done. Every library works on Windows with Win32, Direct3D 12, WASAPI, XInput,
and Winsock, with an example for each in
[easyforge-examples](https://github.com/cresmarmat-an/easyforge-examples).
[Capabilities and limitations](capabilities-and-limitations.md) lists what it
can and cannot do.

## Stage 2: Linux

- `window` on X11 and Wayland, and `graphics` on Vulkan, with the shader
  language writing SPIR-V.
- `sound` through ALSA and PulseAudio, `network` on the system's sockets, and
  `input` reading gamepads through evdev.
- Ogg Vorbis and MP3 sounds in `assets`.
- X11, Wayland, Vulkan, ALSA, and PulseAudio are loaded when the program starts
  rather than linked, so one build runs on any desktop.

This is the stage that shows `window` and `graphics` are really portable.

## Stage 3: the web

- `graphics` on WebGPU, with the shader language writing WGSL.
- `sound` through Web Audio, and the browser's own frame loop for `window`.
- `network` over WebSocket, since browsers cannot send UDP. Servers accept both
  kinds of client; in a browser, unreliable messages arrive reliably.
- Files read from what the page loaded beforehand.

## Stage 4: macOS and iOS

- `window` on Cocoa and UIKit, with touch, and `graphics` on Metal, with the
  shader language writing MSL.
- `sound` through CoreAudio, and gamepads through GameController.
- Custom title bars that leave room for the window buttons where macOS puts
  them.

## Stage 5: Android

- `graphics` on Vulkan, `sound` through AAudio, and the app's lifecycle:
  pausing, resuming, and the system ending the program.
- Files read from inside the app's package.

## Stage 6: 3D

- Lighting with shadows, and skinned, animated models.
- glTF 2.0 and FBX models, and progressive JPEG, HDR, DDS, and KTX2 images in
  `assets`.
- `Physics3D`, following `Physics2D`: convex hulls, triangle meshes, height
  fields, and a character controller.

## Not decided yet

- Encryption for `network`, through the TLS the operating system provides
  (SChannel on Windows, Network.framework on Apple platforms); Linux has no TLS
  in its base system.
- OpenType fonts with CFF outlines.
- The version numbers of the stages after 0.0.1.
