# Changelog

Every change to easyforge that affects people using it is listed here, newest
first. Version numbers follow [Semantic Versioning](https://semver.org); before
1.0.0, any release can change the interface.

## 0.0.1 (2026-10-05)

The first release: every library, built and tested on Windows.

### Added

- The `core` library: vectors, matrices, quaternions, transforms, rectangles,
  bounding boxes, colors, random numbers, properties, results, logging,
  background jobs, a clock, the version, and a test framework.
- CMake packaging: fetch easyforge with `FetchContent`, or install it and use
  `find_package(easyforge COMPONENTS ...)`. When fetched, only the libraries a
  program links are built.
- `EASYFORGE_ONLY` builds one library and the libraries it depends on.
- Build checks: every public header compiles on its own and after `windows.h`,
  public headers avoid names that `windows.h` defines as macros, and each
  library includes only the headers of libraries it depends on.
- The documentation site, built from `docs/` and published on every push.
- The `assets` library: images (PNG, JPEG, BMP, TGA, QOI), sounds (WAV and QOA,
  whole or streamed), TrueType fonts with kerning and an anti-aliased
  rasterizer, OBJ models with MTL materials, file search with mountable folders
  and packs, deflate and zlib decompression, and loading in the background.
  Every decoder is tested against files cut short or with bytes changed.
- The `window` library for Windows: windows sized in points, the frame loop,
  keyboard, text, mouse, touch, and file drop events, custom title bars with
  snap layouts, screens and scaling, icons, cursors, a locked mouse, and the
  clipboard.
- `Event`, `Key`, `View`, `Host`, and `HostListener` in `core`, which let
  libraries work together without depending on each other.
- `ImageData::Resized`, which scales images in linear light.
- `easyforge_app_icon`, a CMake function that builds an image into a program as
  its icon, and the `easyforge-icon` tool behind it.
- `EASYFORGE_INSTALL`, so projects that fetch easyforge do not get its install
  rules.
- The `input` library: named actions bound to keys, mouse buttons, the wheel,
  gamepad buttons, axes, sticks, and key directions; Xbox controllers through
  XInput with dead zones and rumble; bindings saved as text; and recording and
  exact playback of input.
- The `graphics` library on Direct3D 12: renderers for windows and for images in
  memory; a `Canvas` with anti-aliased rounded rectangles, borders, circles,
  lines, images, and text with kerning, clipping, and transforms; textures,
  fonts, and models sent to the GPU when first drawn; and 3D scenes of OBJ
  models lit by a sun.
- The easyforge shader language: pixel shaders with values, helper functions,
  loops, and built-in math, compiled to HLSL, drawn over areas or over layers
  with `Canvas::Shaded` and `Canvas::EndLayer`, and checked when a program is
  built with `easyforge_add_shaders` and the `easyforge-shader` tool.
- Layers in `Canvas`, which fade or shade a group of drawings as one.
- Linear gradients, softened edges for shadows and glows, images sliced into nine
  pieces that stretch without distorting, and `Canvas::BlurBehind` for frosted
  glass, at full or part strength.
- Rectangles with a hole left undrawn, for shadows under see-through boxes, and
  `Canvas::ClipArea`, where drawing can still show.
- `Host::RequestPlacement`, which a view calls when it wants another size, such
  as a title bar made taller.
- The language reader in `core`: a tokenizer and parser for the syntax the
  script and shader languages share.
- Assigning braced values such as `{ 0, 0, 0 }` to a `Property` no longer fails
  to compile.
- The `data` library: a tree of named nodes stored as one column per property,
  values of nine kinds, types that give nodes default values, a list of every
  change that readers follow by version, named edits that undo and redo, and a
  readable `.tree` file format.
- `Node::Get` and `Node::Set`, which read and set a property without making a
  cell.
- The `ui` library: rows, columns, stacks, panels, grids, and scrolling with
  sizes in points, shares, fill, or fit; labels, images, buttons, checkboxes,
  toggles, sliders, progress bars, text fields and text areas with input
  methods, drawing areas, and 3D scene views; dropdowns, menus, and dialogs;
  lists, and trees that follow a data table; custom title bars with the window's
  own buttons; displays that switch with fades, slides, scaling, or a shader, go
  back, or show as tabs; backgrounds, shadows, glows, outlines, gradients,
  frosted glass, color adjustment, masks, and custom shaders on any element;
  light and dark themes that follow the system, animate, and load from files;
  animated properties; keyboard focus, tooltips, and drag and drop; and labels
  that follow values in a data table. Every element is stored as a node of a
  data table.
- The `script` library: the easyforge scripting language, with variables and
  constants, closures, lists, tables, colors, types of your own, errors that
  `try` catches, modules that import each other, and functions that run across
  frames with `spawn`, `wait`, and `yield`. Names, constants, and written types
  are checked before a script runs. Programs give scripts functions, values,
  and objects of their own, call script functions back, and set limits on
  memory and instructions.
- A C interface to the script engine, for C and for languages that call C.
- The `easyforge-script` tool, which runs scripts, checks them, and opens a
  prompt, and `easyforge_add_scripts`, which checks a program's scripts when it
  is built.
- Bridge headers that let scripts drive a `ui` interface
  (`<easyforge/bridges/ui_script.h>`) and change a `data` table
  (`<easyforge/bridges/data_script.h>`).
- The `sound` library: a mixer on its own thread that programs control from any
  thread without it ever waiting; sounds from memory or streamed from WAV and
  QOA files, with volume, pan, pitch, looping, fades, pausing, and starting part
  way; buses with volume, mute, low-pass and high-pass filters, and echo; sounds
  placed around a listener; output through WASAPI that follows the default
  device; and offline mixing into memory.
- `OnPress`, `OnMove`, and `OnRelease` on `ui::DrawingArea`, in the points
  `OnDraw` uses.
- `Rotation` on `RectangleStyle` and `ImageStyle`, which turns a rectangle or an
  image about its center.
- The `physics` library: 2D bodies with circles, boxes, capsules, and convex
  polygons; a sub-stepped soft solver that keeps stacks still; distance, hinge,
  slider, weld, and motor joints; touch events, sensors, and collision layers;
  ray and circle casts and finding bodies by place; and the same result on every
  run.
- The `network` library on Winsock: servers and clients over UDP with a
  handshake, keep-alives, and timeouts; messages of named values sent reliably
  in order, reliably in any order, or unreliably with the newest winning;
  messages of up to 4 MB in pieces; requests answered with a reply or a reason;
  a send rate limit; packet loss and delay on purpose for testing, which can be
  switched while running; an optional thread of its own; and finding servers on
  the local network.
- The `data_network` bridge, which keeps a `data` table the same on a server
  and its clients, one way or with each client changing the nodes it added.
- `<easyforge/easyforge.h>`, which includes every library and the bridges at
  once, for programs that link `easyforge::easyforge`.

### Fixed

- A window asked its title bar view about points below the title bar, so a
  title bar view that did not check the height made the whole window drag.
- A window still open when the program ended detached its views after the rest
  of the window was gone.
- A layer larger than the GPU could hold stopped the frame; it is now cut down
  to what shows.
- Sliced images showed thin seams at fractional positions.
- The language reader treated a byte order mark at the start of a file, which
  some editors write, as an unknown character.
