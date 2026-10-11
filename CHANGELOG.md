# Changelog

## Unreleased

### Added

- `FrameTimer` in `core` turns measured frame times into whole refreshes of the
  screen, carrying what it rounds off, and follows the screen's true rate.
- `Physics2D::Update` runs fixed steps from each frame's time, with
  `StepSeconds` and `MaximumStepsPerUpdate` settings, and bodies have
  `SmoothPosition`, `SmoothRotation`, and `SmoothWorldPoint` for drawing
  evenly at any frame rate.
- `Smoothed` and `SmoothedAngle` in `network` show values another program sends
  again and again smoothly, however unevenly they arrive, and
  `Message::SentAt` tells when a received message was sent.
- `FramePacer` and `Host::SetFramePacer` in `core`, so a renderer can start its
  window's frames when the screen can take another image.
- Physics bodies sleep once they and everything they touch have been still for
  half a second, and wake when anything reaches, pushes, or changes them, with
  `IsAwake`, `Wake`, and `AllowSleep` on bodies and worlds.
- Fast physics bodies are swept through each step, so they cannot pass through
  thin walls, and `Bullet` bodies are swept against dynamic bodies too.
- Renderers start again on a new device by themselves when the GPU is reset,
  and textures, fonts, models, and shaders follow.
- `PathFromText` and `TextFromPath` in `core`, for UTF-8 file paths.
- `BUILD_SHARED_LIBS` builds each library as a DLL that exports exactly its
  public types and functions, marked `EASYFORGE_<LIBRARY>_EXPORT` through a
  generated `<easyforge/<library>/Export.h>`; programs and DLLs then share one
  folder, `bin/<configuration>`.
- The test runner fails a Debug run built with Visual Studio that leaves memory
  allocated when it ends, and lists each block.
- `Result::LogIfFailed()` logs a failed result's error and gives whether it
  succeeded, for results where logging is all a program would do.
- `WriteWholeFile` in `core` writes a file whole or not at all, through a file
  beside it that is flushed to the disk and then takes the old one's place.
- The `EASYFORGE_SANITIZE` build option, and tests that feed damaged input to
  every reader and run jobs, sounds, and network ends from many threads at once.
- A `Font` or `Shader` can be named by the path of its file wherever one is
  expected, such as `.Font = "fonts/Inter.ttf"` or `.Shader = "ripple.shader"`
  in settings; everything that names the same file shares one.
- Versioning and Threads pages in the reference: what each kind of release
  may change, and what every type allows when a program uses several threads.
- Every network connection is encrypted and authenticated with the system's
  cryptography (ECDH, HKDF, and AES-256-GCM through CNG), and servers can prove
  who they are with a `NetworkIdentity` that clients pin as `ServerKey`.
- `RollingResistance` on physics bodies and in every shape's settings, 0.01 by
  default, so round bodies slow down as they roll and piles of them come to
  rest.
- `Window::RequestFrame`, `Host::RequestFrame`, and `SecondsUntilNextFrame` on
  views and listeners, so a window can wait while nothing changes.

### Changed

- A window's `Icon`, in its settings and as a property, is an `ImageData`, so it
  takes an image loaded anywhere as well as a path: `.Icon = icon` or
  `.Icon = ImageData::Load("art/icon.png")`. An `ImageData` can be made from a
  path, which is the same as `Load`.
- Interfaces skip drawing elements wholly outside what can show, such as rows
  scrolled out of view.
- An interface measures again only the elements that changed and what holds
  them, keeping every other measurement, so changing one label among 5,000
  elements costs 3.2 milliseconds a frame instead of 18.
- Drawing keeps its lists of shapes from frame to frame instead of growing new
  ones, and text keeps how it was laid out, so recording 20,000 rectangles
  went from 10.2 to 2.7 milliseconds and 2,000 lines of text from 21 to 11.
- `Window::Run` returns when its window closes even while another open window
  waits with nothing to do.
- `Window::Run` runs frames of a window without `OnFrame` only while something
  changes, and waits without using the processor in between. An interface that
  is still uses no processor time; `Window::RunOneFrame` still runs every frame.
- Windows give `OnFrame` steady frame times while frames are shown in step with
  the screen.
- A window with a renderer now starts each frame when the screen can take
  another image, before reading input, and at most one finished frame waits to
  be shown, so what is shown answers input sooner.
- A physics body turns at most an eighth of a turn per substep.
- A sleeping physics body wakes when something touches it or comes at it,
  not whenever something moves close by, which kept a pile next to a
  trembling body from ever sleeping.
- A physics body counts as still for sleeping when its center and two points at
  its edge stay within 2.5 centimetres of where they were for half a second,
  as in Jolt, rather than by its speed at each moment, so bodies that tremble
  in place in a pile fall asleep.
- Input recordings and bindings, `.tree` files, and scripts open files named in
  any language on Windows; before, names outside the system's code page failed.
- A color in a script that the compiler could not read no longer ends the
  program.
- Scripts run about a third faster: each instruction that reads or writes a
  global remembers where it found it, and the checks between instructions are
  skipped when a script has no limits.
- A file path that is not valid UTF-8, given to any easyforge function, no
  longer throws from inside the standard library; and making a pack from a
  folder with a subfolder that cannot be read fails with an error instead of
  throwing.
- easyforge builds with GCC as well as Visual Studio, with every warning on
  and treated as an error. Every member of the public settings structs has a
  default, so filling them by name gives no missing-initializer warnings with
  GCC or Clang, and a script function that takes no arguments no longer gives
  an unused-variable warning. `Jobs::Settings` is now `JobsSettings`, which
  `Jobs::Settings` still names.
- `RunTests` turns off the error dialogs of Windows and of the C++ library's
  debug build, so a crashing test fails at once instead of waiting.
- With `EASYFORGE_SANITIZE`, the sanitizer's runtime is copied next to every
  program, so the sanitized tests start outside a developer prompt.
- Physics steps allocate no memory once a world has settled into its size, and
  bounds tree queries no longer allocate at all.
- Connections send as fast as the path to the other end carries, found with
  congestion control after TCP's NewReno and QUIC, instead of at most 1 MB a
  second; packets found lost have their messages sent again at once, and a
  packet that arrives out of order is acknowledged at once.
- Tables, themes, input bindings, recordings, network identities, and packs
  are saved whole or not at all, so a crash or power cut while saving leaves
  the old file instead of part of the new one.
- A server given a `NetworkIdentity` that did not load does not start.
- A threaded server or client ends with the program's last copy of it, waiting
  for its thread, instead of sometimes later on that thread, which left its
  port taken and its memory in use for a while after.
- When an interface ends, its elements let go of their callbacks, so a
  callback holding its own element no longer keeps it alive for good. Showing
  such an element again logs a warning.
- `Result` is `[[nodiscard]]`, and so is every function that makes a new
  object (`New`, `Load`, `FromText`, `Open`), so a failure or a new object
  cannot be dropped by accident. The script tool now prints errors from
  functions started with `spawn`, which it used to drop.
- The inline math of `Vector2` to `Vector4`, `Color`, `Rectangle`,
  `Quaternion`, `Matrix3`, `Matrix4`, `Transform`, and the number helpers is
  `noexcept`, and so are `Result`'s `Succeeded`, `Error`, and `operator bool`.
- File extensions are compared in lower case for ASCII letters only, so a
  program that sets a locale cannot change how a UTF-8 name is matched. Tests
  read and write tables and run scripts under a German locale, which writes
  `1,5`, to show numbers stay `1.5` everywhere.
- Servers keep nothing for a connect request until the client answers a
  cookie tied to its address, answer forged requests with less than they were
  sent, limit connect attempts from each address, and close a connection that
  sends more unfinished messages than 16 MB. The wire format's marker is now
  `EFN3`.
- Packets carry the sender's clock. The wire format's revision marker is now
  `EFN2`, so programs built with 0.0.1 cannot talk to programs built with this
  change.

## 0.0.1

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
