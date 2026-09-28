# Capabilities and limitations

What easyforge 0.0.1-alpha can and cannot do, in one place. Each library's pages
go into more detail.

## Libraries

| Library | Status |
|---|---|
| core | Available and tested on Windows |
| assets | Available and tested on Windows |
| window | Available and tested on Windows |
| input | Available and tested on Windows |
| graphics | Available and tested on Windows, with Direct3D 12 |
| data, ui, script, sound, physics, network | Planned, in that order |

## core

It can:

- Do vector, matrix, quaternion, and transform math with one set of
  conventions: right-handed, Y up, -Z forward, depth from 0 to 1.
- Work with rectangles, bounding boxes, and colors in sRGB and linear light.
- Make random numbers that repeat exactly for the same seed on every platform.
- Give objects properties that are read and assigned like variables.
- Report failures as values with readable messages, without exceptions.
- Log at four levels to standard error, the debugger, or a handler of your own.
- Read source in the easyforge language into a syntax tree, reporting every
  problem with its line and column.
- Run work on background threads, wait without freezing the pool, and split
  loops across threads.
- Measure time with a clock that never jumps.
- Run tests with a small framework of its own.

It cannot yet:

- Use `double` or integer vectors and matrices.
- Split a matrix into position, rotation, and scale, or turn a quaternion back
  into angles.
- Convert colors to or from HSV or HSL.
- Prioritize or cancel background jobs.
- Write log messages to a file without a handler of your own.

## assets

It can:

- Read PNG at every color type and bit depth, interlaced or not, with
  transparency and checksum checking.
- Read baseline and extended sequential JPEG, grayscale or color, with any
  whole-number chroma subsampling and restart markers.
- Read BMP (palettes, bit masks, every header version), TGA (with run-length
  compression), and QOI.
- Read WAV with integer or floating point samples, and QOA, whole or streamed a
  piece at a time with seeking.
- Read TrueType fonts and collections: metrics, outlines including composites,
  kerning from GPOS or the kern table, and anti-aliased glyph bitmaps.
- Read OBJ models with MTL materials, with smooth normals made when missing.
- Find files in mounted folders and packs, then the working directory, then next
  to the program; write packs.
- Decompress deflate and zlib data, and compute CRC-32 and Adler-32.
- Load any of these on background threads.
- Survive damaged files, which give an error instead of a crash.

It cannot yet:

- Read progressive, CMYK, or 12-bit JPEG, run-length compressed BMP, or animated
  PNG beyond its first frame.
- Read Ogg Vorbis, MP3, or compressed WAV.
- Read OpenType fonts with CFF outlines, apply hinting, or shape text beyond
  kerning pairs.
- Read glTF or FBX models, skeletons, or animation.
- Compress data, or watch files for changes.

## window

It can:

- Open any number of windows, sized in points so they look the same on every
  screen, and run their frames together at the screen's refresh rate.
- Change the title, icon, size, position, minimum size, full screen,
  maximizing, minimizing, always on top, visibility, background, and cursor at
  any time.
- Report keys by their position on the keyboard, typed text as UTF-8 including
  input methods, the mouse with click counts and both wheels, touches, dropped
  files, focus, resizing, scaling changes, and light and dark mode changes.
- Lock the mouse for camera control, with movement straight from the mouse.
- Replace the system's title bar with a view of your own that still drags,
  maximizes, resizes, and shows the Windows 11 snap layouts.
- Keep drawing while the person drags an edge, and keep the content the same
  size in points when the window moves to a screen with different scaling.
- Read and write the clipboard, list the screens, and use image cursors.
- Build an image into the program as its icon with `easyforge_app_icon`.

It cannot yet:

- Run on anything but Windows 10 version 1703 or later.
- Show menus, message boxes, or dialogs for opening and saving files.
- Report pen pressure, media keys, or screens being plugged in.
- Accept dropped text or images, only files.
- Run frames only when something changes.

## input

It can:

- Bind named actions to keys, mouse buttons, the wheel, gamepad buttons, one
  gamepad axis, whole sticks, and four keys that make a direction, any number
  per action.
- Report each action as pressed, held, or released, with taps inside one frame
  kept, and give values from 0 to 1 and directions no longer than 1.
- Follow a window by itself, skipping presses an interface already used, or
  take events from any other source.
- Read up to four Xbox controllers through XInput, with a round dead zone,
  analog triggers, rumble, and one player per gamepad.
- Change bindings while running, and save and load them as readable text.
- Record input frame by frame and play it back exactly, from memory or a file.

It cannot yet:

- Bind touches, mouse movement, or key combinations.
- Read gamepads other than through XInput, or anywhere but Windows.
- Read motion sensors, touchpads, lights, or battery levels of controllers.

## graphics

It can:

- Draw into windows, following their size and scaling, or into images read back
  into memory.
- Draw anti-aliased rectangles with rounded corners and borders, circles, lines,
  images (scaled, cut from a sheet, tinted, rounded), and text with kerning,
  batched into few GPU draws.
- Clip to nested rectangles and move and scale what is drawn.
- Load textures, fonts, and models anywhere, sending them to the GPU once, when
  first drawn, and again only when a texture changes.
- Draw 3D scenes of OBJ models with a camera, a sun, and ambient light, smoothed
  with four samples per pixel, into all or part of a canvas.
- Pick the fast GPU, the power-saving one, or the software renderer, which draws
  the same pixels everywhere.
- Run pixel shaders written in the easyforge shader language over areas, or over
  layers of what was drawn, with values the program sets; check them when the
  program is built with `easyforge_add_shaders`.
- Fade a group of drawings as one with layers.

It cannot yet:

- Draw with Vulkan, Metal, or WebGPU, or run anywhere but Windows.
- Draw paths, or run shaders on 3D models or with more than one texture.
- Shape text beyond kerning, wrap it at a width, or draw color emoji.
- Draw shadows, point lights, transparent objects, or animated models in 3D.
- Give access to the explicit GPU layer underneath.

## Platforms

| Platform | Status |
|---|---|
| Windows | `core`, `assets`, `window`, `input`, and `graphics` built and tested with Visual Studio 2026 |
| Linux | Planned for stage 2 |
| Web | Planned for stage 3 |
| macOS and iOS | Planned for stage 4 |
| Android | Planned for stage 5 |
