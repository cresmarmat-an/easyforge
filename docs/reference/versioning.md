# Versioning

easyforge's versions follow [Semantic Versioning](https://semver.org): three
numbers, major.minor.patch, such as 1.4.2.

| A new | Means |
|---|---|
| Patch, 1.4.2 to 1.4.3 | Fixes only. Code that built keeps building and does what the docs say |
| Minor, 1.4.3 to 1.5.0 | New features too. Code that built keeps building |
| Major, 1.5.0 to 2.0.0 | Changes that can stop code from building, or change what it does |

## Before 1.0.0

easyforge is at 0.0.1, before 1.0.0. Until 1.0.0, a minor release can also
change the interface, as Semantic Versioning allows, so the libraries can still
take the best shape; a patch release still only fixes. Every such change is in
the [changelog](changelog.md), with what to write instead.

An installation therefore matches only the exact version asked for:
`find_package(easyforge 0.0.1)` accepts 0.0.1 and nothing else. From 1.0.0,
asking for a version accepts any later one with the same major number.

## What counts as a breaking change

Any of these needs a major release from 1.0.0 on:

- Removing or renaming a public header, type, function, property, setting, or
  enum value, or anything else in `include/easyforge`.
- Changing what a function takes or gives back so that calls to it no longer
  build, such as a setting's type that no longer accepts what it used to.
- Changing what a function does, where the docs say what it does.
- Stopping a later version from reading what an earlier one wrote: `.tree`
  files, packs, and input recordings.
- Raising what the platforms need, such as an older Windows version no longer
  supported.

These do not:

- Adding headers, types, functions, properties, settings, and enum values.
  Settings are structs filled by name, so a new field never breaks code that
  leaves it out.
- Anything outside `include/easyforge`, and anything in a namespace named
  `internal`, which is the libraries' own business even where a header shows
  it.
- Fixing a bug, where the old behavior went against the docs.
- Speed, memory use, and the wording of log messages and errors.

## The network's wire format

Programs connected with `network` must speak the same packet format. The
format has its own marker, written in every packet, which changes whenever the
format does; two programs with different markers do not connect. The changelog
says when it changes. From 1.0.0, it changes only in a major release.

## Deprecation

Before something is removed in a major release, it is deprecated in at least
one minor release before it:

- it is marked `[[deprecated("...")]]` in its header, which says what to use
  instead and makes the compiler warn wherever it is used;
- the changelog lists it under Deprecated;
- its docs say what replaces it.

It keeps working until the major release that removes it.

## The changelog

The [changelog](changelog.md) keeps every change, newest first, in the form of
[Keep a Changelog](https://keepachangelog.com). Changes collect under
Unreleased until a release takes them, under these headings:

| Heading | For |
|---|---|
| Added | New features |
| Changed | Changes to what exists, with what to write instead when code is affected |
| Deprecated | What will be removed, and what replaces it |
| Removed | What was removed |
| Fixed | Bugs fixed |
| Security | Anything that affects safety, such as data from the network |

## Which version you have

```cpp
#include <easyforge/version.h>

Log("easyforge {}", easyforge::VersionText);              // "0.0.1"
bool newEnough = easyforge::Version.Minor >= 1;
```

`Version` has `Major`, `Minor`, `Patch`, and `Label`, which is empty for an
official release and names a preview, such as `"alpha"`, otherwise.
