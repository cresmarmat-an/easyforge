# How the libraries connect

```cpp
Window window = Window::New({ .Title = "Notes" });

window.Content = ui::Column({ .Children = { ui::Label("Hello") } });   // window and ui
Controls controls = Controls::New(window);                               // window and input
```

Neither line makes one library depend on the other. The libraries meet in three
ways, and none of them makes a library require another: plain types that one
library makes and another uses, two small interfaces in `core`, and bridge
headers.

## Plain types

One library produces them, another takes them, and both only need the library
that defines the type.

| Type | Made by | Used by |
|---|---|---|
| `ImageData` | assets | graphics (`Texture`), window (icons and cursors), and ui through its images' files |
| `ModelData` | assets | graphics (`Model`) |
| `SoundData` | assets | sound (`Sound`) |
| `FontData` | assets | graphics (`Font`) |
| `Event` | window | input, ui |

This is why `assets` sits under `window`, `graphics`, and `sound`: they all
accept file names (`.Icon = "icon.png"`, `Texture::Load("logo.png")`,
`Sound::Load("jump.wav")`) and read them through it.

## Host and View

Two interfaces in `core` let a window and what fills it meet without knowing
each other:

- **`Host`** is the window side: its surface for drawing, its events and frames,
  size, scaling, cursor, clipboard, text input, and whether the system title
  bar is shown. `Window` is a `Host`.
- **`View`** is anything that can fill a window. A window has two slots for
  views, `Content` and `TitleBar`, and every `ui` element is a view.

That is why `window.Content = ui::Column(...)` works while neither library
depends on the other, and why `Renderer::New(window)` and
`Controls::New(window)` work while `graphics` and `input` know nothing about
`window`. Your own code can be a `View` too, or a `Host` for a window system of
its own; [events and views](../core/events-and-views.md) shows how.

## Bridge headers

A few libraries work together directly, through headers that are compiled only
in programs using both. Neither library links the other.

| Header | Joins | Does |
|---|---|---|
| `<easyforge/bridges/data_script.h>` | data, script | Scripts read and change a table: `scripts.Define("game", ScriptObjectFor(game))` |
| `<easyforge/bridges/ui_script.h>` | ui, script | Scripts drive an interface: `ui::DefineInterface(scripts, ui::Root::Of(window))` |
| `<easyforge/bridges/data_network.h>` | data, network | A table stays the same on a server and its clients: `Share(game, server)` |

[Interfaces and tables](../script/interfaces-and-tables.md) covers the two
script bridges, and [sharing tables](../network/sharing-tables.md) the network
one. `<easyforge/easyforge.h>` includes all three.

## What depends on what

Each library depends only on the libraries listed for it, and the build enforces
it: a library that links or includes anything else fails to build.

| Library | Depends on |
|---|---|
| core | nothing |
| assets, data, input, network, physics, script | core |
| window, graphics, sound | core, assets |
| ui | core, assets, data, graphics |

So a program that needs only networking links `easyforge::network` and gets
`network` and `core`, nothing else. [Fetching with
CMake](../installation/fetching-with-cmake.md) builds only the libraries a program
links.
