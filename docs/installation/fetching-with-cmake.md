# Fetching with CMake

The simplest way to use easyforge is to let CMake download and build it as part
of your project:

```cmake
cmake_minimum_required(VERSION 3.22)
project(my_program LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(easyforge
    GIT_REPOSITORY https://github.com/cresmarmat-an/easyforge.git
    GIT_TAG main
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(easyforge)

add_executable(my_program main.cpp)
target_link_libraries(my_program PRIVATE easyforge::core)
```

```cpp
// main.cpp
#include <easyforge/core.h>

using namespace easyforge;

int main()
{
    Log("easyforge {} is working", VersionText);
}
```

Then build it as usual:

```bash
cmake -S . -B build
cmake --build build --config Debug
```

## Choosing a version

`GIT_TAG main` follows the newest code. easyforge is at 0.0.1-alpha, and until
the first release that is the only choice. Once `v0.0.1` is tagged, use
`GIT_TAG v0.0.1` so your build does not change under you.

## Only what you link is built

When easyforge is fetched by another project, each of its libraries is left out
of the build until something links it. Linking `easyforge::core` compiles only
`core`. Linking a library that depends on others compiles those as well.

easyforge's own tests are also switched off when it is fetched. Set
`EASYFORGE_BUILD_TESTS` to `ON` before `FetchContent_MakeAvailable` if you want
them.

To link every library at once, use `easyforge::easyforge`.

## Using a copy on your computer

To build against a copy of easyforge you are changing, point CMake at it instead
of downloading:

```bash
cmake -S . -B build -D FETCHCONTENT_SOURCE_DIR_EASYFORGE=../easyforge
```

This is a standard CMake setting, and it overrides the `GIT_REPOSITORY` above
without editing your `CMakeLists.txt`.

## Requirements

- CMake 3.22 or later.
- A C++20 compiler with `std::format`. See
  [platform requirements](platform-requirements.md).
- Git, for CMake to download the repository.

easyforge sets C++20 on its own targets and passes that requirement on to
anything that links them, so your program is compiled as C++20 or later too.
Its warning settings stay private and do not change how your code is compiled.
