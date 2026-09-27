# Changelog

Every change to easyforge that affects people using it is listed here, newest
first. Version numbers follow [Semantic Versioning](https://semver.org); before
1.0.0, any release can change the interface.

## 0.0.1-alpha (in development)

### Added

- The `core` library: vectors, matrices, quaternions, transforms, rectangles,
  bounding boxes, colors, random numbers, properties, results, logging,
  background jobs, a clock, the version, and a test framework.
- CMake packaging: fetch easyforge with `FetchContent`, or install it and use
  `find_package(easyforge COMPONENTS ...)`. When fetched, only the libraries a
  program links are built.
- `EASYFORGE_ONLY` builds one library and the libraries it depends on.
- Build checks: every public header compiles on its own and after `windows.h`,
  public headers avoid names that `windows.h` defines as macros, and each
  library includes only the headers of libraries it depends on.
- The documentation site, built from `docs/` and published on every push.
- The `assets` library: images (PNG, JPEG, BMP, TGA, QOI), sounds (WAV and QOA,
  whole or streamed), TrueType fonts with kerning and an anti-aliased
  rasterizer, OBJ models with MTL materials, file search with mountable folders
  and packs, deflate and zlib decompression, and loading in the background.
  Every decoder is tested against files cut short or with bytes changed.
