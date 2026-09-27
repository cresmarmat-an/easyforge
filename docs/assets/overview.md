# assets

`assets` reads files into plain data in memory: images, sounds, fonts, and 3D
models. It knows nothing about the GPU or the sound device, so any program can
use it to read files, and the other easyforge libraries use it when you give
them a file name. Every decoder is written from scratch.

```cmake
target_link_libraries(my_program PRIVATE easyforge::assets)
```

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

ImageData logo = ImageData::Load("logo.png");
if (!logo)
{
    Log(LogLevel::Error, logo.Error());
    return;
}
Log("logo is {} by {}", logo.Width, logo.Height);
```

## What it reads

| Kind | Type | Formats |
|---|---|---|
| [Images](images.md) | `ImageData` | PNG, JPEG (baseline and extended sequential), BMP, TGA, QOI |
| [Sounds](sounds.md) | `SoundData`, `SoundStream` | WAV (integer and floating point), QOA |
| [Fonts](fonts.md) | `FontData` | TrueType (`.ttf`) and TrueType collections (`.ttc`) |
| [Models](models.md) | `ModelData` | OBJ with MTL materials |

It also has [files and packs](files-and-packs.md), which decide where files are
read from, and [compression](compression.md): the deflate decoder and checksums
that PNG uses, available on their own.

## The same shape for every kind

Each type is loaded the same way:

| Written | What it does |
|---|---|
| `Type::Load(path)` | Reads the file through [Files](files-and-packs.md) and decodes it |
| `Type::Decode(bytes, name)` | Decodes bytes you already have; `name` appears in error messages |
| `Type::LoadInBackground(path)` | Loads on the shared background threads and returns a `Pending<Type>` |
| `if (!loaded)` | True when loading failed |
| `loaded.Error()` | Why, starting with the file's name: `"ship.obj: line 12: a face uses vertex 90, but 88 are defined so far"` |

Nothing throws. A failed load gives an empty object whose `Error()` says what went
wrong.

## Loading in the background

```cpp
Pending<ModelData> level = ModelData::LoadInBackground("levels/castle.obj");

// every frame:
if (level.Ready())
{
    const ModelData& model = level.Get();
    // ...
}
```

| Written | Result |
|---|---|
| `pending.Ready()` | True once loading has finished, successfully or not |
| `pending.Wait()` | Returns once loading has finished |
| `pending.Get()` | Waits if needed, then gives the loaded object. Test it the usual way: `if (!pending.Get())` |

A `Pending` made with `Pending<Type>()` has nothing to load: it is ready at once
and `Get()` gives an empty object. Background loads run on
[`Jobs::Shared()`](../core/background-jobs.md).

## Damaged and hostile files

Every decoder checks every read against the size of its data, and refuses sizes
that the data cannot possibly hold before reserving any memory. A damaged file
gives an error, never a crash. The test suite checks this by feeding every
decoder thousands of files cut short or with random bytes changed.

Images are limited to `ImageData::MaximumSide` (32768) pixels on a side and
`ImageData::MaximumPixels` (268 million) pixels in total.
