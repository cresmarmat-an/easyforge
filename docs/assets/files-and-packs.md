# Files and packs

Every `Load` function in easyforge reads through `Files`, which decides where a
path is found. Paths use forward slashes on every platform and are UTF-8.

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

Files::Mount("game.pack");          // look inside the pack first

ImageData logo = ImageData::Load("textures/logo.png");
Result<std::vector<std::uint8_t>> settings = Files::Read("settings.tree");
```

## Where files are found

A relative path is looked for in this order:

1. The folders and packs given to `Files::Mount`, most recent first.
2. The working directory.
3. The folder the program is in.

The third place means a program finds the files shipped next to it, however it
was started. An absolute path is read exactly as it is.

When a file is not found, the error says every place that was searched.

## Reading

| Written | Result |
|---|---|
| `Files::Read(path)` | The whole file, as `Result<std::vector<std::uint8_t>>` |
| `Files::Open(path)` | A `FileReader` for reading parts of a large file |
| `Files::Exists(path)` | True when the search finds the file |
| `Files::ProgramFolder()` | The folder of the running program, with a trailing slash |
| `Files::FolderOf(path)` | `"models/ship.obj"` gives `"models/"` |
| `Files::ExtensionOf(path)` | `"Ship.OBJ"` gives `".obj"`, in lowercase |

A `FileReader` has `Size()` and `ReadAt(offset, buffer)`, which reads up to the
buffer's size starting at `offset` and returns how many bytes it read. It works
the same for files on disk and files inside packs. Copies share the open file, and
reading from several threads at once is safe.

## Mounting

| Written | What it does |
|---|---|
| `Files::Mount(folderOrPack)` | Adds a folder or a pack to search first. Fails if it does not exist or is not a pack |
| `Files::Unmount(folderOrPack)` | Removes it again |
| `Files::UnmountAll()` | Removes every mounted place |

The list of mounted places belongs to the whole program. Mounting while other
threads are reading is safe.

Mounting a folder is useful for mods and for development: mount `"assets/"` and
edit files there without rebuilding anything.

## Packs

A pack is one file that holds many, which is tidier to ship and faster to open
than thousands of small files.

```cpp
Result<> packed = Files::CreatePack("assets/", "build/game.pack");
```

`CreatePack` stores every file under the folder with its path relative to the
folder, so `assets/textures/logo.png` is found in the pack as
`textures/logo.png`. Packs are not compressed; the formats inside them, such as
PNG and QOA, already are.

## Limitations

- `Files` only reads. Write files with the C++ standard library.
- There is no watching for changes yet.
- On Android and the web, reading from the app package and from preloaded files
  arrives with those platforms in stages 3 and 5.
