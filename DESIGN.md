# easyforge design

This is the design of easyforge as a whole, including the libraries that are not
written yet. The documentation in `docs/` describes what exists and how to use
it; this file describes where it is all going.

easyforge is a set of C++ libraries written from scratch: windows, file loading,
input, graphics, an interface toolkit, sound, physics, networking, a data table,
and a scripting language. The only dependencies are the platform SDKs, because
the goal is to understand every layer and how the layers connect. Targets are
Windows, Linux, macOS, iOS, Android, and the web.

Every library is designed to be used together with the others or on its own by
anyone. The interface in particular should feel like this: make a window with
its settings in braces and get an empty window on any platform, then assign
interface pieces to it, with custom title bars, images, effects, and shaders
when you want them.

---

## The project

| | |
|---|---|
| Name | easyforge |
| Author | Cresmar Mat-an |
| Version | `0.0.1-alpha` until the first official release, which is `0.0.1` |
| License | MIT, `Copyright (c) 2026 Cresmar Mat-an` |
| Repository | `github.com/cresmarmat-an/easyforge` |
| Documentation | `cresmarmat-an.github.io/easyforge`, built from `docs/` |
| Examples | `github.com/cresmarmat-an/easyforge-examples` |

Only Cresmar Mat-an appears as author anywhere: LICENSE, README, documentation,
and commits. Commits use the git identity already configured on this machine and
carry no co-author or tool attribution lines.

**Versioning.** CMake's `project(VERSION ...)` accepts only numbers, so the
version is `project(easyforge VERSION 0.0.1)` plus
`set(EASYFORGE_VERSION_LABEL "alpha")`. Code reads it from
`<easyforge/version.h>`:

```cpp
easyforge::Version.Major    // 0
easyforge::Version.Minor    // 0
easyforge::Version.Patch    // 1
easyforge::Version.Label    // "alpha"
easyforge::VersionText      // "0.0.1-alpha"
```

When stage 1 below is finished, the label is removed and `v0.0.1` is tagged as
the first official release. Numbering after that is decided when those stages
ship.

**Getting it.** One repository, fetched by CMake:

```cmake
include(FetchContent)
FetchContent_Declare(easyforge
    GIT_REPOSITORY https://github.com/cresmarmat-an/easyforge.git
    GIT_TAG main)                 # v0.0.1 once it is released
FetchContent_MakeAvailable(easyforge)

target_link_libraries(my_app PRIVATE easyforge::window easyforge::ui)
# or everything at once:
target_link_libraries(my_app PRIVATE easyforge::easyforge)
```

When easyforge is fetched by another project rather than built on its own, its
tests and tools are off and each library is left out of the build until
something links it. Linking only `easyforge::network` compiles only `network`
and `core`. Installing once and using
`find_package(easyforge COMPONENTS window ui)` works too.

---

## Names

### The libraries

| Library | Header | What it does | Main types |
|---|---|---|---|
| `window` | `<easyforge/window.h>` | Windows, the frame loop, everything the system sends | `Window`, `Cursor`, `Monitor` |
| `assets` | `<easyforge/assets.h>` | Reads images, 3D models, sounds, and fonts from files | `ImageData`, `ModelData`, `SoundData`, `FontData`, `Files` |
| `input` | `<easyforge/input.h>` | Named actions from keyboard, mouse, touch, and gamepads | `Controls`, `Key`, `MouseButton`, `GamepadButton` |
| `graphics` | `<easyforge/graphics.h>` | Draws 2D and 3D on Direct3D 12, Vulkan, Metal, WebGPU | `Renderer`, `Canvas`, `Scene`, `Camera`, `Texture`, `Font`, `Model`, `Shader` |
| `ui` | `<easyforge/ui.h>` | Interfaces: layout, elements, themes, effects, displays | `ui::Label`, `ui::Button`, `ui::Column`, `ui::TitleBar`, `ui::Displays`, ... |
| `sound` | `<easyforge/sound.h>` | Mixing, streaming, effects, positional sound | `Mixer`, `Sound`, `PlayingSound` |
| `physics` | `<easyforge/physics.h>` | Bodies, collisions, and joints in 2D, later 3D | `Physics2D`, `Body2D`, `Contact2D`, later `Physics3D` |
| `network` | `<easyforge/network.h>` | One-way messages and two-way requests | `Server`, `Client`, `Connection`, `Message`, `Reply` |
| `script` | `<easyforge/script.h>` | The easyforge scripting language | `ScriptEngine`, `ScriptValue` |
| `data` | `<easyforge/data.h>` | A tree-shaped table with change tracking, undo, saving | `Table`, `Node`, `Change` |
| `core` | `<easyforge/core.h>` | Math, colors, and what libraries share | `Vector2`, `Vector3`, `Color`, `Property`, `Event`, `Log` |

`<easyforge/easyforge.h>` includes all of them, for quick starts.

### Why these names

- **Library names are nouns for what they hold**: `window`, `graphics`,
  `sound`, `physics`.
- **`window`** makes windows, and its main type is `Window`, so
  `Window::New({ ... })` says exactly what it makes. On phones and the web a
  `Window` is the whole screen, which is still accurate.
- **`assets`** is plural because it reads many kinds of files.
- **`ui`** keeps its short name. The whole-word alternative, `interface`, is
  defined as a macro by Windows' own headers (`#define interface struct`), so a
  namespace with that name breaks any program that includes `windows.h`. `ui` is
  also what everyone searches for.
- **`data`** holds the idea that everything is a mutable tree, stored like one
  big table. `ui` stores its elements in it, and games can keep their objects
  in it.
- **`core` is not a feature library.** It exists so that `physics`, `graphics`,
  and `sound` mean the same thing by `Vector3`.

### Why one namespace

Everything is in `easyforge`, and interface elements are in `easyforge::ui`.
With `using namespace easyforge;` a program reads:

```cpp
Window window = Window::New({ .Title = "Notes" });
window.Content = ui::Button("Save");
```

A namespace per library (`app::App`, `render::Renderer`, `script::Engine`)
repeats itself at every call. One namespace works because the main type names
are already distinct. `ui` keeps its own namespace because it has dozens of
small types named with common words (`Label`, `Image`, `List`, `Menu`), and
typing `ui::` lists them all in the editor.

### Naming patterns

- **`XData` is what a file contained; the plain name is ready to use.**
  `ImageData` is pixels in memory, `Texture` is on the GPU. `SoundData` is
  samples, `Sound` is playable. `ModelData` and `Model`, `FontData` and `Font`.
- **`Type::New` makes an object, `Type::Load` reads one from a file.**
  `Window::New`, `Mixer::New`, `Server::New`; `Texture::Load`, `Sound::Load`,
  `ImageData::Load`.
- **Interface elements and small values are written directly:**
  `ui::Button("Save")`, `Vector2 { 1, 2 }`, `Color::Hex("#15151A")`. A tree of
  thirty elements would be unreadable with `::New` on every one.
- **No names that Windows' headers turn into macros.** `windows.h` redefines
  `DrawText`, `SendMessage`, `LoadImage`, `CreateWindow`, `PlaySound`,
  `DeleteFile`, `Yield`, and `interface`. So drawing text is `canvas.Text(...)`,
  sending is `client.Send(...)`, and a test includes `windows.h` before and after
  every easyforge header to catch mistakes. easyforge's own headers never
  include platform headers.
- **The text element is `ui::Label`.** C++ does not allow a class with a
  constructor to have a member with the class's own name, so a `ui::Text`
  element could not have a `.Text` property. `ui::Label` can:
  `label.Text = "Saved"`.

---

## Rules every library follows

Once you know them for one library, you know them for all.

1. **Settings go in braces.** `Window::New({ .Title = "Notes", .Width = 1280 })`,
   `ui::Button("Save", { .Width = 120 })`. Anything left out has a sensible
   default.
2. **Change anything later by assigning it.** Every setting is also a property:
   `window.Title = "Notes"`, `button.Text = "Play"`, `crate.Velocity = { 2, 0 }`.
   Reading works the same way: `std::string title = window.Title;`.
3. **Events are functions you assign, named `On...`.**
   `button.OnClick = [] { ... };`, `window.OnFrame = [](float deltaSeconds) { ... };`.
4. **Anything that can fail can be tested.** `if (!window) { Log(window.Error()); }`.
   No exceptions are thrown.
5. **Objects are handles.** Copying one is cheap, every copy refers to the same
   object, and the object lives as long as any copy does. Capturing one by value
   in a lambda is safe.
6. **Nothing is global**, with three exceptions that belong to the whole
   program: where log messages go, the shared background threads from
   `Jobs::Shared()`, and the list of places files are read from. Two windows,
   two mixers, and two physics worlds can exist at once.

Two C++ details the documentation states up front:

- C++20 requires designated initializers in the order the fields are declared.
  Each settings struct is declared in the order people usually write them
  (`Name` first when there is one, `Children` always last), and the
  documentation lists that order.
- A property is not a plain value. `auto title = window.Title;` does not
  compile; write the type (`std::string title = window.Title;`) or call
  `window.Title.Get()`.

---

## window and ui: the usage

### An empty window

```cpp
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({
        .Title = "Notes",
        .Icon = "icon.png",
        .Width = 1280,
        .Height = 720,
    });

    window.Run();
}
```

That is an empty window on every platform. On the desktop `Run` returns when
the window closes. On the web, iOS, and Android the system decides when frames
happen, so `Run` hands the frame to the system instead of looping, and the same
`main` works everywhere. More windows can be made with `Window::New` at any time
and stay responsive while `Run` is going.

Changing it while it runs:

```cpp
window.Title = "Notes - untitled.txt";
window.Fullscreen = true;
window.Cursor = Cursor::Hand;

window.OnFrame = [&](float deltaSeconds) { /* every frame */ };
window.OnEvent = [&](const Event& event) { /* keys, mouse, touch, resize ... */ };
window.OnCloseRequested = [&] { return AskToSave(); };   // return false to stay open
```

Other settings: `Resizable`, `MinimumWidth`, `MinimumHeight`, `Fullscreen`,
`Transparent`, `AlwaysOnTop`, `VerticalSync`. Also on the window: clipboard,
dropped files, monitors, display scaling, and on phones `OnSuspend` and
`OnResume`.

The icon has two parts. `.Icon` sets the window's icon while it runs, where the
platform allows it (Windows and Linux). The icon people see in Explorer, the
Dock, and on a phone's home screen is part of the built program, so a CMake
function makes those files from one image:

```cmake
easyforge_app_icon(notes icon.png)   # .ico resource, .icns, Android and iOS icon sets, favicon
```

### Putting an interface in it

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

A window has two slots for what it shows: `Content`, and `TitleBar` (below).
`window` does not know about `ui`: the slots take anything that implements
`View` from `core`, and `ui` elements do. Everything assigned to one window
shares one theme, one focus, and one renderer. The theme follows the system's
light or dark setting unless changed:

```cpp
ui::Root::Of(window).Theme = ui::Theme::Dark();
```

### A custom title bar

```cpp
window.TitleBar = ui::TitleBar({
    .Height = 40,
    .Background = Color::Hex("#15151A"),
    .Children = {
        ui::Image("icon.png", { .Width = 20, .Height = 20 }),
        ui::Label("Notes"),
        ui::Spacer(),
        ui::WindowButtons(),     // minimize, maximize, close, styled by the theme
    },
});
```

Assigning `TitleBar` turns off the system's title bar and draws yours. The bar
still behaves like a title bar: dragging it moves the window, double-clicking
maximizes, the edges resize, and on Windows 11 hovering the maximize button
shows the snap layouts. On macOS the red, yellow, and green buttons stay where
macOS users expect them and your bar is drawn around them. On iOS, Android, and
the web there is no window frame, so the title bar is not shown.

### Images, effects, and shaders

```cpp
ui::Panel({
    .Width = 320,
    .Padding = 16,
    .CornerRadius = 12,
    .Background = ui::Background::Image("paper.png", { .Slice = 12 }),   // corners keep their shape
    .Effects = {
        ui::Shadow { .Offset = { 0, 6 }, .Blur = 18, .Color = Color::Black.WithAlpha(0.35f) },
        ui::BackgroundBlur { .Radius = 20 },
    },
    .Children = { ... },
})
```

Built-in effects: `Shadow`, `BackgroundBlur`, `Glow`, `Outline`, `Gradient`,
`ColorAdjust`, and `Mask`. Anything else is a custom shader, written in the
easyforge shader language:

```cpp
ui::Panel({
    .Shader = Shader::Load("ripple.shader"),
    .ShaderValues = { { "Speed", 2.0f }, { "Tint", Color::Hex("#66CCFF") } },
    .Children = { ... },
})
```

```
-- ripple.shader
value Speed: number = 1
value Tint: color = #FFFFFF

function Pixel(input: PixelInput) returns color then
    constant wave = Sine(input.Position.X * 20 + input.Time * Speed)
    return Sample(input.Content, input.UV + vector2(0, wave * 0.01)) * Tint
end
```

`input.Content` is the element as it would have been drawn, so a shader changes
how an element looks without redrawing it. Shaders are compiled when the program
is built: `easyforge_add_shaders(notes ripple.shader)`.

### Pieces you reuse

A reusable piece is an ordinary function:

```cpp
ui::Element Card(std::string title, std::string body)
{
    return ui::Panel({
        .Padding = 16,
        .CornerRadius = 12,
        .Effects = { ui::Shadow {} },
        .Children = {
            ui::Label(title, { .FontSize = 20 }),
            ui::Label(body),
        },
    });
}
```

Drawing your own, with the same `Canvas` that `graphics` uses everywhere:

```cpp
ui::DrawingArea({
    .Width = ui::Fill,
    .Height = 200,
    .OnDraw = [](Canvas& canvas) {
        canvas.Line({ 0, 0 }, { 100, 50 }, { .Color = Color::White, .Width = 2 });
        canvas.Circle({ 50, 50 }, 20, { .Color = Color::Hex("#FF5555") });
    },
})
```

### Changing the interface while it runs

Elements are handles, so keep one to change it later. Changes work before and
after the element is on screen.

```cpp
ui::Label status("Ready");
ui::Button save("Save", { .OnClick = [status] { status.Text = "Saved"; } });

save.Enabled = false;
status.Opacity.AnimateTo(0.0f, { .Duration = 0.4f, .Easing = ui::Easing::Out });
```

Elements given a `.Name` can be found later:
`ui::Root::Of(window).Find<ui::Button>("Save")`.

### Displays: switching what the user sees

```cpp
ui::Displays pages({
    .Start = "Menu",
    .Children = {
        ui::Display("Menu", { .Children = { ... } }),
        ui::Display("Settings", { .Children = { ... } }),
        ui::Display("Game", { .Children = { ... } }),
    },
});
window.Content = pages;

pages.Show("Settings", ui::Transition::Slide(0.25f));
pages.Back();   // the previous display, with the transition reversed
```

`ui::Displays` is an ordinary element, so it can fill the window or just one
area of it, such as the right side of a settings screen. Both displays are drawn
during a transition. A hidden display keeps its state (scroll position, typed
text) until it is shown again. Transitions: `Cut`, `Fade`, `Slide`, `Scale`,
or a shader.

### Showing live data

```cpp
Table game = Table::New();
Node player = game.Add("Player", { { "Name", "Ari" }, { "Health", 100 } });

ui::Label(ui::Bind(player["Health"], "Health: {}"))
```

When `Health` changes anywhere (in C++, in a script, or from the network) the
label updates. No manual refresh.

### A game with an interface on top

```cpp
Scene scene = Scene::New();
// ... fill the scene ...

window.Content = ui::Stack({
    .Children = {
        ui::SceneView(scene, { .Width = ui::Fill, .Height = ui::Fill }),
        ui::Column({
            .Padding = 16,
            .Children = { ui::Label(ui::Bind(player["Health"], "Health: {}")) },
        }),
    },
});
```

`ui::Stack` layers its children, so the game fills the window and the health
label sits on top. Clicks the interface uses do not reach the game's controls.

### ui without window

Someone with their own window or engine can still use `ui`:

```cpp
ui::Root root = ui::Root::New({ .Content = ui::Column({ ... }) });

root.HandleEvent(event);                 // returns true when the interface used it
root.Draw(canvas, deltaSeconds);         // into a graphics Canvas
```

### What is in ui

- **Layout:** `Row`, `Column`, `Stack` (layered), `Grid`, `Scroll`, `Spacer`.
  Sizes are pixels (`200`), `ui::Fill`, `ui::Percent(50)`, or `ui::Fit`
  (as small as the content). Padding, margin, gap, alignment, grow and shrink.
- **Elements:** `Label`, `Image`, `Button`, `TextField`, `TextArea`,
  `Checkbox`, `Toggle`, `Slider`, `Dropdown`, `List`, `Tree`, `Tabs`, `Menu`,
  `Dialog`, `Tooltip`, `ProgressBar`, `Panel`, `DrawingArea`, `SceneView`,
  `Displays`, `Display`, `TitleBar`, `WindowButtons`.
- **Behavior:** focus and Tab order, keyboard navigation, text selection, copy
  and paste, input methods for Chinese, Japanese, and Korean text, drag and
  drop, scrolling with momentum on touch screens.
- **Themes:** colors, fonts, spacing, and corner radii are named values in a
  `ui::Theme`. Elements use the theme unless given their own values. Switching
  themes is one assignment and can be animated. Themes can be loaded from files.
- **Storage:** elements are rows in a `data::Table`, so only what changed is
  laid out again, and scripts can edit the interface through the same data.

---

## The other libraries

### core

Math (`Vector2`, `Vector3`, `Vector4`, `Matrix3`, `Matrix4`, `Quaternion`,
`Transform`, `Rectangle`, `BoundingBox`, `Color`, `Random`), `Property<T>`,
the failure pattern, `Log`, a job system for background work, a clock, the
version, and a small test framework. Also `Event`, `Host`, and `View`, which
let libraries work together without depending on each other (see "How the
libraries connect").

### assets

```cpp
#include <easyforge/assets.h>

ImageData logo = ImageData::Load("logo.png");     // logo.Width, logo.Height, logo.Pixels
ModelData ship = ModelData::Load("ship.fbx");     // meshes, materials, skeleton, animations
SoundData jump = SoundData::Load("jump.wav");     // samples, sample rate, channels
FontData inter = FontData::Load("Inter.ttf");

if (!ship)
{
    Log(ship.Error());   // "ship.fbx: unsupported FBX version 6100"
}

Pending<ModelData> level = ModelData::LoadInBackground("level.glb");
if (level.Ready()) { /* ... */ }

Files::Mount("game.pack");   // later loads look inside the pack first
```

Everything is read with code written from scratch. It returns plain data in
memory and knows nothing about the GPU or the sound device, so any program can
use it to read files. Most programs never touch it directly: `Texture::Load`,
`Sound::Load`, and `ui::Image` use it for you.

| Kind | Formats | Stage |
|---|---|---|
| Images | PNG, JPEG (baseline), BMP, TGA, QOI | 1 |
| Images | JPEG (progressive), HDR, DDS, KTX2 | 6 |
| Models | OBJ with MTL | 1 |
| Models | glTF 2.0, FBX (binary 7.x) | 6 |
| Sounds | WAV, QOA | 1 |
| Sounds | Ogg Vorbis, MP3 | 2 |
| Fonts | TrueType | 1 |
| Fonts | OpenType with CFF outlines | later |

Also written here: inflate (PNG and FBX both need it), a JSON reader (glTF needs
it), and pack files for shipping. File access goes through one layer that reads
from folders on the desktop, from the package on Android, and from preloaded
files on the web.

### input

```cpp
#include <easyforge/input.h>

Controls controls = Controls::New(window);          // listens to this window
controls.Bind("Jump", { Key::Space, GamepadButton::South });
controls.Bind("Move", Stick::Left);
controls.Bind("Move", KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W });

window.OnFrame = [&](float deltaSeconds) {
    if (controls.Pressed("Jump")) { /* ... */ }
    Vector2 move = controls.Axis("Move");
    Vector2 mouse = controls.MousePosition;
};
```

Given a window, `Controls` follows its events and frames by itself, and skips
events the interface already used (typing in a text field does not make the
character jump). Without a window, pass events in with `controls.Handle(event)`
and call `controls.NextFrame()`.

Gamepads do not need a window, so `input` reads them itself: XInput on Windows,
evdev on Linux, GameController on Apple platforms, the Android input queue, the
browser Gamepad API. Bindings can be changed and saved while the program runs.
Recording events and playing them back repeats a session exactly, which is how
the interface tests run.

### graphics

With `ui`, drawing is handled for you. Without it:

```cpp
#include <easyforge/graphics.h>

Renderer renderer = Renderer::New(window);
Texture logo = Texture::Load("logo.png");
Font font = Font::Load("Inter.ttf");

window.OnFrame = [&](float deltaSeconds) {
    Canvas canvas = renderer.BeginFrame(Color::Hex("#1A1A20"));
    canvas.Draw(scene);
    canvas.Rectangle({ .Position = { 100, 100 }, .Size = { 200, 80 }, .Color = Color::White, .CornerRadius = 8 });
    canvas.Image(logo, { .Position = { 400, 100 } });
    canvas.Text(font, "Hello", { .Position = { 100, 300 }, .Size = 32 });
    renderer.EndFrame();
};
```

3D:

```cpp
Scene scene = Scene::New();
Model ship = Model::Load("ship.obj");
SceneObject object = scene.Add(ship, { .Position = { 0, 0, 0 } });

scene.Camera = { .Position = { 0, 2, -6 }, .Target = { 0, 0, 0 } };
scene.Sun = { .Direction = { -1, -2, -1 }, .Color = Color::White };

object.Rotation = Quaternion::FromAngles(0, time, 0);
```

`Canvas` is the one 2D drawing API: the same type in `renderer.BeginFrame`, in
`ui::DrawingArea`, and in `ui::Root::Draw`. Textures, fonts, models, and
shaders are loaded without a renderer and sent to the GPU the first time
something draws them, so they can be loaded anywhere. A window has one renderer;
when the window's content is `ui`, the interface owns it and a second
`Renderer::New(window)` fails with an error that says to draw inside
`ui::DrawingArea` or `ui::SceneView` instead.

`Renderer` batches drawing into as few GPU calls as it can. Underneath it is
`GraphicsDevice`, the explicit layer (buffers, textures, pipelines, command
lists, fences) for people who want to write their own rendering. It is shaped
after WebGPU, the most limited of the four APIs, with anything beyond it offered
as optional features, so the web does not force a redesign later.

**The shader language** looks like the scripting language and shares its reader
and parser. Its compiler gains one output per platform, when that platform
arrives:

| Stage | Backend | The shader compiler writes |
|---|---|---|
| 1 | Direct3D 12 | HLSL, which `dxc` from the Windows SDK compiles |
| 2 | Vulkan | SPIR-V directly |
| 3 | WebGPU | WGSL |
| 4 | Metal | MSL |

It is in stage 1 because custom interface shaders are a stage 1 feature, and a
shader written then keeps working on every later platform without changes.

### data

```cpp
#include <easyforge/data.h>

Table game = Table::New();

Node player = game.Add("Player");
player["Health"] = 100;
player["Position"] = Vector2 { 10, 20 };

Node sword = player.Add("Sword");
sword["Damage"] = 12;

int health = player["Health"];

game.Save("save.tree");
```

The same data seen as a table:

| Node | Parent | Health | Position | Damage |
|---|---|---|---|---|
| Player | | 100 | 10, 20 | |
| Sword | Player | | | 12 |

Empty cells cost nothing. Each property is stored as one array of values, so
visiting every node with `Health` is one pass over one array, and tree links are
indices rather than pointers.

```cpp
game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });   // defaults
Node goblin = game.Add("Goblin", "Enemy");                             // Health reads 100

for (Node enemy : game.NodesWith("Health")) { /* ... */ }

game.BeginEdit("Hit");
goblin["Health"] = 90;
game.EndEdit();
game.Undo();

for (const Change& change : game.ChangesSince(lastVersion)) { /* node, property, old, new */ }
```

Every change is recorded in one list. That list is how `ui` updates only what
changed, how undo works, how a save file stores only differences, and how a
table is shared over the network.

The `.tree` file format is readable and diffs well:

```
Player
    Health = 100
    Position = 10, 20
    Sword
        Damage = 12
```

### sound

```cpp
#include <easyforge/sound.h>

Mixer mixer = Mixer::New();                        // the default output device
Sound jump = Sound::Load("jump.wav");
Sound music = Sound::Load("theme.qoa", { .Stream = true });

PlayingSound song = mixer.Play(music, { .Volume = 0.5f, .Loop = true, .Bus = "Music" });
mixer.Play(jump, { .Position = { 4, 0, 0 } });     // positional
mixer.Listener.Position = playerPosition;

song.FadeTo(0.0f, 2.0f);
mixer.Bus("Music").Volume = 0.3f;
```

The mixer runs on its own thread and takes commands through a queue that never
blocks, so it never waits on the game. Written from scratch: mixing, buses,
resampling, low-pass and high-pass filters, echo, distance fading, panning, and
streaming long files from disk. Devices: WASAPI in stage 1, then ALSA and
PulseAudio, Web Audio, CoreAudio, and AAudio with their platforms.

### physics

```cpp
#include <easyforge/physics.h>

Physics2D physics = Physics2D::New({ .Gravity = { 0, -9.8f } });

Body2D ground = physics.AddBox({ .Size = { 50, 1 }, .Type = BodyType::Static });
Body2D crate = physics.AddBox({ .Position = { 0, 10 }, .Size = { 1, 1 } });

crate.OnTouch = [](const Contact2D& contact) { /* ... */ };

physics.Step(1.0f / 60.0f);

Vector2 position = crate.Position;
crate.Velocity = { 2, 0 };
RayHit2D hit = physics.CastRay({ 0, 20 }, { 0, -1 }, 50);
```

2D in stage 1: circles, boxes, capsules, and convex polygons; a tree of bounding
boxes to find nearby pairs; exact contact tests; a solver with warm starting and
substeps so stacks stay still; distance, hinge, slider, weld, and motor joints;
ray casts, shape casts, sensors, and collision layers. It is deterministic: the
same inputs give the same result on every run. `Physics3D` follows the same
design in stage 6, with convex hulls, triangle meshes, height fields, and a
character controller. Triangle meshes are passed as plain vertex and index
lists, so `physics` does not need `assets`.

### network

Two ways to talk, and either end can use both:

- **One way:** send a message and expect nothing back. Positions, chat, events.
- **Two way:** send a request and get a reply, or a failure if it times out or
  the connection drops. Logging in, asking for a score, buying an item.

```cpp
#include <easyforge/network.h>

// server
Server server = Server::New({ .Port = 7777, .MaximumClients = 16 });

server.OnConnected = [](Connection client) { /* ... */ };

server.OnMessage("Chat", [&](Connection from, const Message& message) {
    server.SendToAll("Chat", message);                               // one way
});

server.OnRequest("GetScore", [&](Connection from, const Message& request) {
    return Message { { "Score", scores[from.Index()] } };           // two way: the answer
});

server.Update();   // each frame, or set .Threaded = true
```

```cpp
// client
Client client = Client::New({ .Address = "127.0.0.1", .Port = 7777 });

client.Send("Chat", { { "Text", "hello" } });                                     // one way
client.Send("Position", { { "X", x }, { "Y", y } }, Delivery::Unreliable);       // may be lost; newest wins

client.Request("GetScore", {}, [](const Reply& reply) {                          // two way
    if (reply)
    {
        int score = reply.Message["Score"];
    }
}, { .Timeout = 5.0f });
```

The server can `Request` from a client the same way.

Delivery: `Reliable` (the default: arrives once, in order), `Unreliable`, and
`ReliableUnordered`. Written from scratch over the platform's sockets: a
connection handshake, keep-alives and timeouts, sequence numbers and
acknowledgements, resending, ordering, splitting large messages, a send rate
limit, request identifiers so each reply finds its callback, and finding servers
on the local network without typing an address.

For tests: `.Conditions = { .Loss = 0.1f, .Latency = 0.08f }` drops and delays
packets on purpose.

Stated up front:

- **Browsers cannot send UDP.** On the web (stage 3) the connection uses
  WebSocket, written from scratch, and servers accept both kinds. There,
  `Unreliable` messages arrive reliably.
- **No encryption at first.** Writing cryptography from scratch for real use is
  a common way to ship something insecure, so stage 1 is for local networks and
  trusted servers. Encryption comes later through the TLS the operating system
  provides where it has one (SChannel on Windows, Network.framework on Apple
  platforms).

### script

A standalone language that any C or C++ program can embed. Files end in
`.script`.

```
function SomeName(parameter) returns string then
    print("Output: {parameter}")
    return "Output: {parameter}"
end

SomeName("Hello")
```

`then` ends every header that has a condition, a loop, a signature, or a caught
error. `else`, `try`, and `type` need nothing after them. Every block closes with
`end`.

```
-- a comment runs to the end of the line

--[[
    a comment that spans
    several lines
]]

variable count = 0
constant limit = 10

if count < limit then
    count = count + 1
else if count == limit then
    print("full")
else
    print("over")
end

while count < limit then
    count = count + 1
end

for index in 1 to 10 then
    print(index)
end

for item in items then
    print(item)
end

type Point
    X: number
    Y: number
end

function Distance(first: Point, second: Point) returns number then
    return SquareRoot((second.X - first.X) ^ 2 + (second.Y - first.Y) ^ 2)
end

try
    Risky()
catch problem then
    print("failed: {problem}")
end
```

- Keywords are whole words: `function`, `returns`, `then`, `end`, `variable`,
  `constant`, `if`, `else`, `while`, `for`, `in`, `to`, `return`, `break`,
  `continue`, `and`, `or`, `not`, `true`, `false`, `nothing`, `type`,
  `import`, `try`, `catch`.
- Types: `number`, `string`, `boolean`, `list`, `table`, `function`,
  `nothing`, `any`, and forms like `list of number`. Annotations are optional;
  what is annotated is checked before the script runs.
- `"Score: {score}"` puts values into text; `{{` writes a brace.
- One file is one module: `import "enemies"` loads `enemies.script`.
- `spawn`, `wait`, and `yield` run a function across frames, for animations and
  timers without callbacks.
- `--` starts a comment that runs to the end of the line. `--[[` starts a
  comment that runs until the next `]]`, across as many lines as needed. A
  multi-line comment can also sit inside a line: `constant speed --[[ in metres
  per second ]] = 4`. The shader language uses the same two forms.

```cpp
#include <easyforge/script.h>

ScriptEngine scripts = ScriptEngine::New({ .MemoryLimit = 64 * 1024 * 1024 });

scripts.Define("Shout", [](std::string text) { return text + "!"; });

scripts.RunFile("greet.script");
ScriptValue result = scripts.Call("SomeName", "Hello");   // "Output: Hello"
```

A plain C interface (`easyforge_script_new`, `easyforge_script_run_file`, ...)
makes it usable from C and from other languages. Limits on instructions and
memory, and no file or network access unless the host allows it, make
untrusted scripts safe to run.

Built in this order: a tokenizer, a parser that produces a syntax tree, name
resolution, a compiler to bytecode, and a virtual machine that works on
registers, with interned strings and mark-and-sweep garbage collection. The type
checker is added once the language runs, so scripts work early and gain checking
later. The `easyforge-script` tool runs a file, checks a file, or opens an
interactive prompt, and errors point at the file, line, and column.

---

## How the libraries connect

There are three ways libraries meet, and none of them makes one library require
another.

**1. Plain types.** One library produces them, another consumes them.

| Type | Made by | Used by |
|---|---|---|
| `ImageData` | assets | graphics, ui, window (icons and cursors) |
| `ModelData` | assets | graphics |
| `SoundData` | assets | sound |
| `FontData` | assets | graphics |
| `Event` (core) | window | input, ui |

**2. Two small interfaces in `core`.** `Host` is the window side: its surface
for drawing, its events and frames, size, scaling, cursor, clipboard, text
input, and whether the system title bar is shown. `View` is anything that can
fill a window. `Window` implements `Host` and has two `View` slots, `Content`
and `TitleBar`; `ui` elements implement `View`. That is why
`window.Content = ui::Column(...)` works with neither library depending on the
other, and why `Renderer::New(window)` and `Controls::New(window)` work while
`graphics` and `input` know nothing about `window`. Your own code can implement
`View` too.

**3. Bridge headers**, for the few places two libraries work together directly.
They are header-only, so a bridge only compiles when you use both libraries,
and neither library links the other.

```cpp
#include <easyforge/bridges/data_network.h>
Share(game, server, Sharing::OneWay);    // clients get a read-only copy
Share(game, client, Sharing::TwoWay);    // each node is changed only by its owner

#include <easyforge/bridges/data_script.h>
scripts.Define("game", game);            // scripts read and change the table

#include <easyforge/bridges/ui_script.h>
scripts.Define("ui", ui::Root::Of(window));
```

With `ui_script.h`, a script can drive an interface:

```
-- menu.script
constant pages = ui.Find("Pages")

function OnPlayClicked() then
    pages.Show("Game", Fade(0.3))
end

ui.Find("Play").OnClick = OnPlayClicked
```

**Dependencies**, enforced by the build so a library cannot quietly start
needing another:

| Library | Depends on |
|---|---|
| core | nothing |
| assets, data, input, network, physics, script | core |
| window, graphics, sound | core, assets |
| ui | core, assets, data, graphics |

`assets` sits under `window`, `graphics`, and `sound` because they all accept
file names (`.Icon = "icon.png"`, `Texture::Load("logo.png")`,
`Sound::Load("jump.wav")`). It is plain C++ with no platform code apart from
file access.

---

## Repository layout

Top-level `include/`, `src/`, and `docs/`, with one folder per library.
Public headers sit under `include/easyforge/`, so a program writes
`#include <easyforge/window.h>`; without that folder, an include such as
`<window/main.h>` would collide with any other library on the include path that
has a folder of the same name.

```
easyforge/
  CMakeLists.txt
  README.md
  LICENSE
  CHANGELOG.md
  DESIGN.md
  .gitignore
  .gitattributes
  .github/
    workflows/docs.yml          builds and deploys the documentation site
    workflows/build.yml         builds and tests on every push
    site/                       the site generator, page template, styles
  cmake/                        easyforge_app_icon, easyforge_add_shaders, package files
  include/easyforge/
    easyforge.h                 every library at once
    version.h
    core.h       core/...
    assets.h     assets/...
    window.h     window/...
    input.h      input/...
    graphics.h   graphics/...
    ui.h         ui/...
    sound.h      sound/...
    physics.h    physics/...
    network.h    network/...
    script.h     script/...
    data.h       data/...
    bridges/     data_network.h  data_script.h  ui_script.h
  src/
    core/  assets/  data/  input/  network/  script/  ui/
    window/     windows/  linux/  web/  apple/  android/
    graphics/   direct3d12/  vulkan/  webgpu/  metal/
    sound/      wasapi/  linux/  web/  apple/  android/
    physics/    2d/  3d/
  tests/                        one folder per library
  tools/
    easyforge-script/           run, check, interactive prompt
    easyforge-shader/           compiles .shader files for the build
  docs/
```

Platform code sits in a platform folder inside its library and is chosen by
CMake, so no file is full of `#ifdef` blocks.

Each library is a CMake target (`easyforge::ui`), plus `easyforge::easyforge`
for all of them. CMake 3.22 or later, C++20, warnings treated as errors.

Options: `EASYFORGE_BUILD_<LIBRARY>` for each library,
`EASYFORGE_BACKEND_DIRECT3D12`, `EASYFORGE_BACKEND_VULKAN`,
`EASYFORGE_BACKEND_METAL`, `EASYFORGE_BACKEND_WEBGPU`,
`EASYFORGE_BUILD_TESTS` and `EASYFORGE_BUILD_TOOLS` (both on only when
easyforge is the top-level project).

The C++ standard library is used (`std::string`, `std::vector`,
`std::function`); it comes with the compiler and users already know it. There is
no third-party code, so there is no third-party notices file.

Platform SDKs, the only dependencies:

| Platform | Stage | Used |
|---|---|---|
| Windows | 1 | Win32, Direct3D 12, DXGI, WASAPI, XInput, Winsock, `dxc` |
| Linux | 2 | X11, Wayland, Vulkan, ALSA, PulseAudio, evdev, sockets |
| Web | 3 | Emscripten, WebGPU, Web Audio, WebSocket, Gamepad API |
| macOS and iOS | 4 | Cocoa, UIKit, Metal, CoreAudio, GameController, sockets |
| Android | 5 | NDK, GameActivity, Vulkan, AAudio, sockets |

X11, Wayland, Vulkan, ALSA, and PulseAudio are loaded when the program starts
rather than linked, so one Linux build runs on any desktop.

---

## Documentation

The official documentation is the Markdown in `docs/`. GitHub Actions rebuilds
the site and deploys it to GitHub Pages on every push to `main`, so a page added
or changed in `docs/` is live a minute or two later.

- **Adding a page needs no site change.** The generator picks up every `.md`
  file and folder by itself, titles each page from its first `# ` heading, and
  treats a broken link as a warning, not a failure. An optional `docs/nav.json`
  sets order and titles where the default is not right.
- **The generator is written for easyforge** in `.github/site/`, using only
  Python's standard library, so the workflow installs nothing.
- **One-time setup on GitHub:** in the repository's settings, Pages source is
  set to "GitHub Actions". The repository's owner does this once.

```
docs/
  index.md                          what easyforge is, a first program, where to go next
  nav.json                          optional
  getting-started/
    introduction.md
    first-window.md
    first-interface.md
    using-one-library-alone.md
    how-the-libraries-connect.md
    names-and-patterns.md
  installation/
    fetching-with-cmake.md
    installing-and-find-package.md
    build-options.md
    platform-requirements.md
    app-icons.md
    shaders.md
  window/  assets/  input/  graphics/  ui/  sound/  physics/  network/  script/  data/  core/
  reference/
    capabilities-and-limitations.md
    changelog.md
    license.md
```

How the pages are written:

- Each page leads with a complete example that compiles, then walks through the
  API by task, with real calls and real parameters.
- Every public feature is documented somewhere, and each page says plainly what
  the feature does not do.
- No measured timings or machine-specific numbers; every reader's computer is
  different.
- Pages link to the matching program in `easyforge-examples` instead of copying
  it.
- A page is written in the same step that builds the feature it describes.

---

## easyforge-examples

A separate repository at `PROJECTS/c++/easyforge-examples`, fetching easyforge
the same way anyone else would, so the examples only use the public API.

```
easyforge-examples/
  CMakeLists.txt              fetches easyforge, one target per example
  README.md
  LICENSE                     MIT, Cresmar Mat-an
  .gitignore
  .github/workflows/build.yml
  01-empty-window/            main.cpp
  02-custom-title-bar/
  03-displays/
  04-effects-and-shaders/     main.cpp, ripple.shader
  05-live-data/
  06-scripted-interface/      main.cpp, menu.script
  07-sound/                   main.cpp, sounds/
  08-physics/
  09-chat/                    one-way messages
  10-lobby/                   two-way requests
  11-shared-table/            one-way and two-way sharing
  12-small-multiplayer-game/
```

Each example is one `main.cpp` plus its files, short enough to read in one
sitting. For working on both at once, configure the examples against the local
checkout so library changes show up without pushing:

```
cmake -B build -DFETCHCONTENT_SOURCE_DIR_EASYFORGE=../easyforge
```

---

## Stages

The version stays `0.0.1-alpha` through stage 1. Finishing stage 1 is the first
official release, `0.0.1`.

**Stage 1: Windows, every library working.** Each step ends with something that
runs, its tests, and its documentation pages.

| Step | Library | Done when |
|---|---|---|
| 1 | core | Math, properties, jobs, version, and test framework tests pass |
| 2 | assets | PNG, JPEG, WAV, QOA, TrueType, and OBJ files decode and match reference output |
| 3 | window | Example 01: an empty window with an icon that resizes and scales correctly |
| 4 | input | Actions fire from keyboard, mouse, and an Xbox controller; a recorded session plays back |
| 5 | graphics | Rectangles, images, and text on Direct3D 12; an OBJ model lit by a sun |
| 6 | data | A table is built, changed, undone, saved, and reloaded, and the change list is correct |
| 7 | ui | Examples 02 to 05: custom title bar with snap layouts, displays with transitions, effects, a custom shader, live data |
| 8 | script | `SomeName("Hello")` prints `Output: Hello`; example 06 runs an interface from a script |
| 9 | sound | Example 07: music and positional effects through WASAPI |
| 10 | physics | Example 08: a stack of boxes settles the same way every run |
| 11 | network | Examples 09 to 11 work with 10% of packets dropped |
| 12 | all | Example 12: a small multiplayer 2D game using every library; tag `v0.0.1` |

**Stage 2, Linux:** Vulkan, X11 and Wayland, ALSA and PulseAudio, Ogg Vorbis
and MP3. This is the stage that proves `window` and `graphics` are truly
portable.
**Stage 3, Web:** WebGPU, Web Audio, WebSocket, the browser's frame loop.
**Stage 4, macOS and iOS:** Metal, Cocoa, UIKit, CoreAudio, touch.
**Stage 5, Android:** Vulkan, AAudio, the app lifecycle, reading files from the
package.
**Stage 6, 3D:** lighting with shadows, skinned and animated models, glTF and
FBX, `Physics3D`.

Sizes to plan around: the script language and text rendering are the two
largest parts of stage 1. FBX is the largest single file format, because its
transforms, units, and axes vary between exporting programs; that is why OBJ
comes first and FBX arrives with 3D. Custom title bars take real per-platform
work (snap layouts on Windows, the button area on macOS, Wayland's decoration
rules).

---

## Testing

- Build with every library on, then each library alone with only what it
  depends on. The single-library builds prove standalone use, and they run in
  CI from the start.
- `ctest` after every step; each step adds its tests.
- A header test compiles every public header alone, and again with `windows.h`
  included before and after it.
- The examples repository builds against the local checkout, and each example
  it has so far runs. Examples 01 to 05 are checked against reference
  screenshots.
- The documentation generator runs locally and the result is previewed in the
  browser before pushing: every page renders, navigation lists every page, and
  broken links are reported.
- Interface: recorded input played back and the result compared with a
  reference image.
- Physics: the same scene run twice gives identical numbers.
- Network: server and clients in one process with simulated loss and delay;
  every one-way message arrives once and in order, and every request gets a
  reply or a timeout.
- Script: a folder of `.script` files with expected output, including
  `SomeName`.

## Open questions

None of these block stage 1.

- An editor: `data` and `ui` make one straightforward, and it would be a program
  in its own repository, not a library.
- Encryption on Linux, which has no TLS in its base system.
- How far 3D goes in stage 6.
- Version numbers for the stages after `0.0.1`.
