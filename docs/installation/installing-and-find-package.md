# Installing and find_package

Instead of fetching easyforge in every project, you can build and install it
once and let projects find it.

## Installing

From a copy of the repository:

```bash
cmake -S . -B build
cmake --build build --config Release
cmake --install build --config Release --prefix C:/libraries/easyforge
```

With Visual Studio you can install both configurations into the same folder, so
debug builds of your program link the debug libraries:

```bash
cmake --build build --config Debug
cmake --install build --config Debug --prefix C:/libraries/easyforge
```

The folder then holds the headers in `include/easyforge`, the libraries in
`lib`, and the files CMake reads in `lib/cmake/easyforge`.

## Using it

```cmake
cmake_minimum_required(VERSION 3.22)
project(my_program LANGUAGES CXX)

find_package(easyforge 0.0.1 REQUIRED COMPONENTS core)

add_executable(my_program main.cpp)
target_link_libraries(my_program PRIVATE easyforge::core)
```

Tell CMake where to look when configuring:

```bash
cmake -S . -B build -D CMAKE_PREFIX_PATH=C:/libraries/easyforge
```

## Components

`COMPONENTS` lists the libraries your program needs. `find_package` fails if the
installation does not have one of them, for example because it was built with
that library switched off. The variable `easyforge_LIBRARIES` lists the
libraries an installation has.

## Versions

Before 1.0.0 any release can change the interface, so an installation only
matches the exact version asked for: `find_package(easyforge 0.0.1)` accepts
0.0.1 and nothing else. Leaving the version out accepts whatever is installed.
