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
| data | Available and tested on Windows |
| ui | Available and tested on Windows |
| script | Available and tested on Windows |
| sound | Available and tested on Windows, with WASAPI |
| physics, network | Planned, in that order |

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
  images (scaled, cut from a sheet, tinted, rounded, or sliced into nine pieces
  that stretch without distorting), and text with kerning, batched into few GPU
  draws.
- Fill shapes with linear gradients, soften their edges into shadows and glows,
  and blur what is behind an area like frosted glass.
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

## data

It can:

- Hold a tree of named nodes with any properties, stored as one column per
  property, with tree links as row numbers.
- Hold nothing, booleans, 64-bit whole numbers, numbers, text, vectors, and
  colors, read as any of those types.
- Give nodes default values through types, without copying them.
- Find nodes by path, identifier, property, or type.
- Record every change in one list that readers can follow by version.
- Undo and redo named edits, including removed trees, which come back with the
  same handles.
- Save to and load from a readable `.tree` text format, reporting the line of
  anything it cannot read.

It cannot yet:

- Be used from two threads at once.
- Save only what changed, or save the undo history.
- Let a type extend another, or change a node's type after it is added.
- Share a table over a network; that comes with `network`.

## ui

It can:

- Lay out rows, columns, stacks, panels, grids, and scrolling areas, with sizes
  in points, shares of the parent, fill, or fit, padding, margins, gaps,
  alignment, and distribution, laying out again only when something changed.
- Show labels with wrapping, images with fits and slices, buttons in three
  styles, checkboxes, toggles, sliders, progress bars, text fields and text
  areas with selection, clipboard, undo, and input methods, drawing areas, and
  3D scenes.
- Show dropdowns, menus with shortcuts and separators, and dialogs that keep the
  keyboard until they close.
- Show lists to choose from, and trees of a data table's nodes that follow its
  changes.
- Draw a window's title bar, with the window's own buttons and snap layouts.
- Switch between displays with fades, slides, scaling, or a shader, go back, or
  show pages as tabs.
- Put backgrounds (colors, gradients, sliced images), borders, shadows, glows,
  outlines, gradients, frosted glass, color adjustment, masks, and custom
  shaders on any element.
- Follow the system's light or dark theme, switch themes gradually, and load
  them from files.
- Animate opacity, position, and scale; move the keyboard with Tab; show
  tooltips; drag text from element to element and take files dropped from the
  system; and keep clicks and keys the interface used from the rest of the
  program.
- Show values from a data table as they change, and store the interface itself
  as a data table.

It cannot yet:

- Show menus inside menus, choose several rows of a list at once, or drag to
  other programs.
- Show the system's own menus or file dialogs.
- Shape text for scripts such as Arabic, or show color emoji.
- Run anywhere but Windows, since it draws with `graphics`.

## script

It can:

- Run the easyforge language: variables and constants, functions that keep
  the names around them, lists, tables, colors, types of your own, errors that
  can be caught, and modules that import each other.
- Check names, constants, and the types written in a script before it runs,
  and report every problem with its file, line, and column.
- Run functions across frames with `spawn`, `wait`, and `yield`.
- Give scripts C++ functions, values, and objects of your own, and call script
  functions from C++, including callbacks a script handed over.
- Be used from C and from languages that call C.
- Stop runaway scripts with limits on memory, instructions, and call depth,
  collect memory scripts no longer use, and keep scripts away from files and
  the network.
- Let scripts drive a `ui` interface and change a `data` table, through bridge
  headers.
- Check scripts during the build, and run them or try the language at a prompt
  with `easyforge-script`.

It cannot yet:

- Be used from two threads at once.
- Key tables by anything but names, or sort with an order of your own.
- Let scripts make new interface elements; they change the ones the program
  built.
- Step through a script in a debugger.

## sound

It can:

- Mix up to 512 sounds at once on a thread of its own that never waits on the
  program, controlled from any thread.
- Play sounds from memory or streamed from WAV and QOA files, at any volume,
  pan, and pitch, looping, fading, paused, or started part way.
- Group sounds into buses with their own volume, mute, low-pass and high-pass
  filters, and echo.
- Place sounds in the world around a listener, panned by direction and quieter
  with distance.
- Play to the default output device through WASAPI, following it when it
  changes, and carry on without one.
- Mix offline into memory, the same every time, for tests and files.

It cannot yet:

- Play to a device anywhere but Windows, choose a device other than the
  default, or record.
- Play to more than two speakers, or tell sounds ahead from sounds behind.
- Add reverb, Doppler shift, or loop points inside a sound.

## Platforms

| Platform | Status |
|---|---|
| Windows | `core`, `assets`, `window`, `input`, `graphics`, `data`, `ui`, `script`, and `sound` built and tested with Visual Studio 2026 |
| Linux | Planned for stage 2 |
| Web | Planned for stage 3 |
| macOS and iOS | Planned for stage 4 |
| Android | Planned for stage 5 |
