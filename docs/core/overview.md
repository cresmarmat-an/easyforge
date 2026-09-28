# core

`core` holds what every other easyforge library shares: math, colors, and
small tools for writing programs. It has no platform code apart from writing to
the debugger's output window on Windows, and it depends on nothing but the C++
standard library.

```cmake
target_link_libraries(my_program PRIVATE easyforge::core)
```

```cpp
#include <easyforge/core.h>
```

## What is in it

| Page | Types and functions |
|---|---|
| [Vectors](vectors.md) | `Vector2`, `Vector3`, `Vector4`, `Dot`, `Cross`, `Length`, `Normalize`, `Lerp`, `Clamp`, `Radians`, `Pi` |
| [Matrices](matrices.md) | `Matrix3`, `Matrix4`, projections, `LookAt`, `Inverse`, `Transpose` |
| [Rotations and transforms](rotations-and-transforms.md) | `Quaternion`, `Slerp`, `Transform`, `Combine` |
| [Rectangles and boxes](rectangles-and-boxes.md) | `Rectangle`, `BoundingBox`, `Intersection`, `Union` |
| [Colors](colors.md) | `Color`, hex codes, sRGB and linear light |
| [Random numbers](random-numbers.md) | `Random` |
| [Properties](properties.md) | `Property`, settings you read and assign like variables |
| [Events, hosts, and views](events-and-views.md) | `Event`, `Key`, `View`, `Host`, `HostListener`, how libraries work together |
| [The language reader](language.md) | `language::Tokenize`, `language::Parse`, the syntax tree the script and shader languages share |
| [Results](results.md) | `Result`, `Failure` |
| [Logging](logging.md) | `Log`, `LogLevel`, `SetLogHandler` |
| [Background jobs](background-jobs.md) | `Jobs`, `Job`, `ParallelFor` |
| [Measuring time](measuring-time.md) | `Clock` |
| [Testing](testing.md) | `EASYFORGE_TEST`, `EASYFORGE_EXPECT`, `RunTests` |

## Including less

`<easyforge/core.h>` includes everything except the test framework. Each part
also has its own header, such as `<easyforge/core/Vector.h>` or
`<easyforge/core/Jobs.h>`. The test framework is always included on its own:
`<easyforge/core/Testing.h>`.

`<easyforge/core/Formatting.h>` teaches `std::format` to print vectors,
quaternions, rectangles, and colors. `core.h` includes it, which is what lets
`Log("position {}", position)` work.

## The version

```cpp
#include <easyforge/version.h>

easyforge::Version.Major    // 0
easyforge::Version.Minor    // 0
easyforge::Version.Patch    // 1
easyforge::Version.Label    // "alpha"; empty for an official release
easyforge::VersionText      // "0.0.1-alpha"
```

All of these are `constexpr`, so they can be used in `static_assert` and other
compile-time checks.

