# Names and patterns

Every easyforge library follows the same conventions. Once you know them for
one library, you know them for all.

## One namespace

Everything is in the `easyforge` namespace. Writing `using namespace easyforge;`
once gives the short names used throughout these pages:

```cpp
#include <easyforge/window.h>
#include <easyforge/ui.h>

using namespace easyforge;

Window window = Window::New({ .Title = "Notes" });
window.Content = ui::Button("Save");
```

The main types of the libraries already have distinct names (`Window`, `Mixer`,
`Physics2D`, `Server`), so a namespace for each library would only repeat itself
at every call. Interface elements are the exception: they live in
`easyforge::ui`, since `ui` has dozens of small types named with common words
(`Label`, `Image`, `List`, `Menu`), and `ui::` keeps them apart from your own
names and lists them all in the editor.

## Whole words

Names are written out: `Vector3`, `Quaternion`, `Rectangle`, `ThreadCount`.
The exceptions are proper names of things outside easyforge, which keep their
own spelling: `Direct3D12`, `WebGPU`, `PNG`, `HLSL`.

The libraries are nouns for what they hold: `window`, `graphics`, `sound`,
`physics`. `ui` is the one short name, because the whole word, `interface`, is a
macro in Windows' own headers.

## Settings in braces

Anything that takes several settings takes them as a struct, filled with C++20
designated initializers. Anything left out keeps its default.

```cpp
Window window = Window::New({ .Title = "Notes", .Width = 1280 });
ui::Button save("Save", { .Width = 120 });
```

C++ requires the fields in the order they are declared. Each page lists the
fields in that order, and every settings struct is declared in the order people
usually write them: `Name` first when there is one, and `Children` always last.

## Making objects

- **`Type::New` makes an object**: `Window::New`, `Mixer::New`, `Server::New`.
- **`Type::Load` reads one from a file**: `Texture::Load`, `Sound::Load`,
  `ImageData::Load`.
- **Interface elements and small values are written directly**:
  `ui::Button("Save")`, `Vector2 { 1, 2 }`, `Color::Hex("#15151A")`. A tree of
  thirty elements would be hard to read with `::New` on every one.

## What a file holds, and what is ready to use

A name ending in `Data` is what a file contained, in memory; the plain name is
ready to use. `ImageData` is pixels and `Texture` is an image on the GPU;
`SoundData` is samples and `Sound` is playable; the same goes for `ModelData` and
`Model`, and `FontData` and `Font`. The `Data` types come from
[assets](../assets/overview.md), which knows nothing about GPUs or sound devices,
so any program can use it to read files.

## Handles

Objects are handles. Copying one is cheap, every copy refers to the same object,
and the object lives as long as any copy does, so keeping a copy, or capturing
one in a lambda, never leaves it dangling.

A handler that an object keeps, and that holds a copy of that same object, keeps
it alive for as long as the handler is set. The pages that have such handlers,
such as [servers and clients](../network/servers-and-clients.md), say how that
ends.

## Changing settings later

Every setting that can change is also a [property](../core/properties.md),
read and assigned like a variable:

```cpp
window.Title = "Notes";
button.Text = "Play";
crate.Velocity = { 2, 0 };

std::string title = window.Title;
```

A property is not the value itself, so `auto title = window.Title;` does not
compile: write the type, as above, or call `window.Title.Get()`.

## Events

Events are functions you assign, named `On...`:

```cpp
button.OnClick = [] { Log("saved"); };
window.OnFrame = [](float deltaSeconds) { /* every frame */ };
```

## Failures

Nothing in easyforge throws an exception. Anything that can fail can be tested,
and says why it failed:

```cpp
Window window = Window::New({ .Title = "Notes" });
if (!window)
{
    Log(window.Error());
}
```

Functions return [Result](../core/results.md) for this, and objects that can fail
to be made offer the same `if (!object)` test and an `Error()` function.

## Nothing global, with three exceptions

You can have as many of anything as you like: two windows, two mixers, two
physics worlds. Three things belong to the whole program instead:

- where [log messages](../core/logging.md) go,
- the [shared background threads](../core/background-jobs.md) from `Jobs::Shared()`,
- and the list of folders and packs that [files](../assets/files-and-packs.md) are read from.

## Headers

Each library has one header that includes all of it, such as
`<easyforge/core.h>`, and `<easyforge/easyforge.h>` includes every library. The
parts are also available on their own, such as `<easyforge/core/Vector.h>`, when
you want to include less.

easyforge's headers never include platform headers. You can include
`windows.h` and `windowsx.h` before or after them, with or without `NOMINMAX`:
easyforge avoids every name those headers turn into a macro, such as `DrawText`,
`SendMessage`, `LoadImage`, `min`, and `near`, and the build checks this for
every public header. That is why drawing text is `canvas.Text(...)` and sending
is `client.Send(...)`. It is also why the text element is `ui::Label`: a class
named `Text` could not have a `Text` property, and `label.Text = "Saved"` reads
well.

One name still needs care. `windows.h` declares a function called `Rectangle`,
so a program that includes it and writes `using namespace easyforge;` has two
things named `Rectangle`, and the compiler asks which one is meant. Write
`easyforge::Rectangle` in that program.
