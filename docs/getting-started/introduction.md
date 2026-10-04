# Introduction

easyforge is a set of C++20 libraries for making programs with windows,
graphics, interfaces, sound, physics, networking, and scripting. Every part of
it is written from scratch: there is no third-party code, and the only things it
uses besides the C++ standard library are each platform's own SDK, such as
Win32 and Direct3D 12 on Windows or Metal on Apple platforms.

## The libraries

easyforge is one repository with one library per subject. You link only the
ones you use.

| Library | CMake target | Header | Status |
|---|---|---|---|
| core | `easyforge::core` | `<easyforge/core.h>` | Available |
| assets | `easyforge::assets` | `<easyforge/assets.h>` | Available |
| window | `easyforge::window` | `<easyforge/window.h>` | Available on Windows |
| input | `easyforge::input` | `<easyforge/input.h>` | Available |
| graphics | `easyforge::graphics` | `<easyforge/graphics.h>` | Available on Windows |
| data | `easyforge::data` | `<easyforge/data.h>` | Available |
| ui | `easyforge::ui` | `<easyforge/ui.h>` | Available on Windows |
| script | `easyforge::script` | `<easyforge/script.h>` | Available |
| sound | `easyforge::sound` | `<easyforge/sound.h>` | Available on Windows |
| physics | `easyforge::physics` | `<easyforge/physics.h>` | Available |
| network | `easyforge::network` | `<easyforge/network.h>` | Available on Windows |

`easyforge::easyforge` links every library that was built, for when you want
all of them, and `<easyforge/easyforge.h>` includes every library's header and
the bridge headers that join some of them:

```cpp
#include <easyforge/easyforge.h>   // with easyforge::easyforge
```

## What depends on what

Each library depends only on the libraries listed for it here. The build
enforces this: a library that links or includes anything else fails to build.

| Library | Depends on |
|---|---|
| core | nothing |
| assets, data, input, network, physics, script | core |
| window, graphics, sound | core, assets |
| ui | core, assets, data, graphics |

So a program that only needs networking links `easyforge::network` and gets
`network` and `core`, nothing else. `core` holds what the others share: math,
colors, and the plain types one library hands to another.

## Platforms

Windows comes first. Linux, the web, macOS and iOS, and Android follow, one at a
time, each adding its graphics, window, and sound backends.
[Platform requirements](../installation/platform-requirements.md) has the
details and what has been tested.

## Reading these pages

Each page starts with code you can copy, then goes through what the feature can
do and what it cannot. Code examples assume `using namespace easyforge;`.

[Names and patterns](names-and-patterns.md) covers the conventions every
library follows, [how the libraries connect](how-the-libraries-connect.md)
explains how they work together without depending on each other, and the
[roadmap](../reference/roadmap.md) says what the later stages add.
