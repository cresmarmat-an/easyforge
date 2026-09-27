# Capabilities and limitations

What easyforge 0.0.1-alpha can and cannot do, in one place. Each library's pages
go into more detail.

## Libraries

| Library | Status |
|---|---|
| core | Available and tested on Windows |
| assets | Available and tested on Windows |
| window, input, graphics, data, ui, script, sound, physics, network | Planned, in that order |

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

## assets

It can:

- Read PNG at every color type and bit depth, interlaced or not, with
  transparency and checksum checking.
- Read baseline and extended sequential JPEG, grayscale or color, with any
  whole-number chroma subsampling and restart markers.
- Read BMP (palettes, bit masks, every header version), TGA (with run-length
  compression), and QOI.
- Read WAV with integer or floating point samples, and QOA, whole or streamed a
  piece at a time with seeking.
- Read TrueType fonts and collections: metrics, outlines including composites,
  kerning from GPOS or the kern table, and anti-aliased glyph bitmaps.
- Read OBJ models with MTL materials, with smooth normals made when missing.
- Find files in mounted folders and packs, then the working directory, then next
  to the program; write packs.
- Decompress deflate and zlib data, and compute CRC-32 and Adler-32.
- Load any of these on background threads.
- Survive damaged files, which give an error instead of a crash.

It cannot yet:

- Read progressive, CMYK, or 12-bit JPEG, run-length compressed BMP, or animated
  PNG beyond its first frame.
- Read Ogg Vorbis, MP3, or compressed WAV.
- Read OpenType fonts with CFF outlines, apply hinting, or shape text beyond
  kerning pairs.
- Read glTF or FBX models, skeletons, or animation.
- Compress data, or watch files for changes.

## Platforms

| Platform | Status |
|---|---|
| Windows | `core` and `assets` built and tested with Visual Studio 2026 |
| Linux | Planned for stage 2 |
| Web | Planned for stage 3 |
| macOS and iOS | Planned for stage 4 |
| Android | Planned for stage 5 |
