# Names and patterns

Every easyforge library follows the same conventions. Once you know them for
one library, you know them for all.

## One namespace

Everything is in the `easyforge` namespace. Writing `using namespace easyforge;`
once gives the short names used throughout these pages:

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Vector3 position = { 1.0f, 2.0f, 3.0f };
Log("position {}", position);
```

Interface elements, when `ui` arrives, will be in `easyforge::ui`, so they read
as `ui::Button` and `ui::Label`. That library has dozens of small types named with
common words, and keeping them apart avoids clashes with your own names.

## Whole words

Names are written out: `Vector3`, `Quaternion`, `Rectangle`, `ThreadCount`.
The exceptions are proper names of things outside easyforge, which keep their
own spelling: `Direct3D12`, `WebGPU`, `PNG`, `HLSL`.

## Settings in braces

Anything that takes several settings takes them as a struct, filled with C++20
designated initializers. Anything left out keeps its default.

```cpp
Jobs jobs = Jobs::New({ .ThreadCount = 4 });

Transform crate = {
    .Position = { 0.0f, 2.0f, 0.0f },
    .Scale = { 2.0f, 2.0f, 2.0f },
};
```

C++ requires the fields in the order they are declared. Each page lists the
fields in that order, and every settings struct is declared in the order people
usually write them.

## Making objects

- **Objects are made with `Type::New`**: `Jobs::New(...)`. Later libraries follow
  the same shape: `Window::New(...)`, `Server::New(...)`.
- **Objects read from files use `Type::Load`**, for example `Sound::Load("jump.wav")`
  once `sound` exists.
- **Small values are written directly**: `Vector2 { 1, 2 }`, `Color::Hex("#15151A")`,
  `Random random(42);`.

Objects are handles. Copying one is cheap and every copy refers to the same
object, which lives as long as any copy does. A `Jobs` works this way today:
copies share the same threads.

## Changing settings later

Objects expose their settings as [properties](../core/properties.md), which you
read and assign like variables:

```cpp
window.Title = "Notes";                  // once window exists
std::string title = window.Title;
```

## Failures

Nothing in easyforge throws an exception. Anything that can fail can be tested,
and says why it failed:

```cpp
Result<int> count = CountLines("notes.txt");
if (!count)
{
    Log(count.Error());
}
```

Functions return [Result](../core/results.md) for this. Objects that can fail to
be made, such as a window the system refuses to open, will offer the same
`if (!object)` test and an `Error()` function.

## Nothing global, with three exceptions

You can have as many of anything as you like: two thread pools, two windows,
two physics worlds. Three things belong to the whole program instead:

- where [log messages](../core/logging.md) go,
- the [shared background threads](../core/background-jobs.md) from `Jobs::Shared()`,
- and, once `assets` exists, the list of folders and packs that files are read from.

## Headers

Each library has one header that includes all of it, such as
`<easyforge/core.h>`. The parts are also available on their own, such as
`<easyforge/core/Vector.h>`, when you want to include less.

easyforge's headers never include platform headers. You can include
`windows.h` before or after them, with or without `NOMINMAX`: easyforge avoids
every name that `windows.h` turns into a macro, such as `DrawText`, `min`, and
`near`, and the build checks this for every public header.
