# Build options

These CMake options change what easyforge builds. Set them with `-D` when
configuring, or with `set()` before `FetchContent_MakeAvailable`.

| Option | Default | What it does |
|---|---|---|
| `EASYFORGE_BUILD_TESTS` | `ON` when easyforge is built on its own, `OFF` when fetched | Builds the tests and the build checks |
| `EASYFORGE_INSTALL` | `ON` when easyforge is built on its own, `OFF` when fetched | Adds the rules `cmake --install` uses |
| `EASYFORGE_ONLY` | empty | Builds one library and the libraries it depends on |
| `EASYFORGE_SANITIZE` | `OFF` | Builds everything with AddressSanitizer, so the tests catch memory errors |
| `EASYFORGE_BUILD_CORE` | `ON` | Builds the `core` library |
| `EASYFORGE_BUILD_ASSETS` | `ON` | Builds the `assets` library |
| `EASYFORGE_BUILD_WINDOW` | `ON` | Builds the `window` library, on platforms it supports |
| `EASYFORGE_BUILD_INPUT` | `ON` | Builds the `input` library |
| `EASYFORGE_BUILD_GRAPHICS` | `ON` | Builds the `graphics` library, on platforms it supports |
| `EASYFORGE_BUILD_DATA` | `ON` | Builds the `data` library |
| `EASYFORGE_BUILD_UI` | `ON` | Builds the `ui` library, where `graphics` is built |

Every library gets an `EASYFORGE_BUILD_<LIBRARY>` option once it exists.
Switching off a library that another library needs is an error, and the message
says which one. On a platform a library does not support yet, such as `window`
on Linux before stage 2, the library is left out with a message. The same goes
for `graphics`.

## Tools

`easyforge-icon`, which [`easyforge_app_icon`](app-icons.md) runs, is defined
whenever `assets` is built, and `easyforge-shader`, which
[`easyforge_add_shaders`](shaders.md) runs, whenever `graphics` is built. When
easyforge is fetched, each is only compiled if a program uses its function.

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
  needs. On Windows each header is also compiled after `windows.h` and
  `windowsx.h`, without `NOMINMAX`, because that is how many programs include
  them.
- **public-header-names** fails if a public header uses a name that
  `windows.h` or `windowsx.h` defines as a macro, such as `DrawText`, `min`,
  `near`, or `IsMaximized`. Programs that include those headers first would
  otherwise call functions that do not exist.
- **library-includes** fails if a library's code includes a header of a library
  it does not depend on.

## Checking memory

```bash
cmake -S . -B build-sanitized -D EASYFORGE_SANITIZE=ON
cmake --build build-sanitized --config Debug
ctest --test-dir build-sanitized -C Debug --output-on-failure
```

`EASYFORGE_SANITIZE` builds easyforge and its tests with AddressSanitizer, which
stops a test at the first read or write outside memory it owns, use of freed
memory, or similar mistake, and says where. The repository's checks run every
test this way on every change. Programs built this way run a few times slower,
so it is for testing, not for programs people use.

The tests also feed every reader damaged input: images, sounds, fonts, models,
compressed data, `.tree` files, values, scripts, shaders, network messages,
and packets damaged, made up, or repeated on their way between a client and a
server. Each reader must turn it away with an error, or read something it can
write back the same way, without crashing or hanging.

## Warnings

easyforge compiles its own code with warnings treated as errors (`/W4 /WX` with
Visual Studio, `-Wall -Wextra -Wpedantic -Werror` elsewhere). These settings
apply only to easyforge's targets, never to programs that use it.
