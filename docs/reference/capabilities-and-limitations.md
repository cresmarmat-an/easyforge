# Capabilities and limitations

What easyforge 0.0.1-alpha can and cannot do, in one place. Each library's pages
go into more detail.

## Libraries

| Library | Status |
|---|---|
| core | Available and tested on Windows |
| assets, window, input, graphics, data, ui, script, sound, physics, network | Planned, in that order |

## core

It can:

- Do vector, matrix, quaternion, and transform math with one set of
  conventions: right-handed, Y up, -Z forward, depth from 0 to 1.
- Work with rectangles, bounding boxes, and colors in sRGB and linear light.
- Make random numbers that repeat exactly for the same seed on every platform.
- Give objects properties that are read and assigned like variables.
- Report failures as values with readable messages, without exceptions.
- Log at four levels to standard error, the debugger, or a handler of your own.
- Run work on background threads, wait without freezing the pool, and split
  loops across threads.
- Measure time with a clock that never jumps.
- Run tests with a small framework of its own.

It cannot yet:

- Use `double` or integer vectors and matrices.
- Split a matrix into position, rotation, and scale, or turn a quaternion back
  into angles.
- Convert colors to or from HSV or HSL.
- Prioritize or cancel background jobs.
- Write log messages to a file without a handler of your own.

## Platforms

| Platform | Status |
|---|---|
| Windows | `core` built and tested with Visual Studio 2026 |
| Linux | Planned for stage 2 |
| Web | Planned for stage 3 |
| macOS and iOS | Planned for stage 4 |
| Android | Planned for stage 5 |
