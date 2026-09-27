# Build options

These CMake options change what easyforge builds. Set them with `-D` when
configuring, or with `set()` before `FetchContent_MakeAvailable`.

| Option | Default | What it does |
|---|---|---|
| `EASYFORGE_BUILD_TESTS` | `ON` when easyforge is built on its own, `OFF` when fetched | Builds the tests and the build checks |
| `EASYFORGE_ONLY` | empty | Builds one library and the libraries it depends on |
| `EASYFORGE_BUILD_CORE` | `ON` | Builds the `core` library |
| `EASYFORGE_BUILD_ASSETS` | `ON` | Builds the `assets` library |

Every library gets an `EASYFORGE_BUILD_<LIBRARY>` option once it exists, such as
`EASYFORGE_BUILD_WINDOW`. Switching off a library that another library needs is
an error, and the message says which one.

## Building one library

```bash
cmake -S . -B build -D EASYFORGE_ONLY=core
```

This builds exactly what someone who uses only that library gets. The
repository's own checks build every library this way, to prove each one really
stands alone. An unknown name is an error that lists the libraries.

## Running the tests

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Besides each library's tests, three checks run:

- **Every public header compiles on its own**, so each one includes what it
  needs. On Windows each header is also compiled after `windows.h`, without
  `NOMINMAX`, because that is how many programs include it.
- **public-header-names** fails if a public header uses a name that
  `windows.h` defines as a macro, such as `DrawText`, `min`, or `near`.
  Programs that include `windows.h` first would otherwise call functions that do
  not exist.
- **library-includes** fails if a library's code includes a header of a library
  it does not depend on.

## Warnings

easyforge compiles its own code with warnings treated as errors (`/W4 /WX` with
Visual Studio, `-Wall -Wextra -Wpedantic -Werror` elsewhere). These settings
apply only to easyforge's targets, never to programs that use it.
