# Compression

The deflate decoder and the checksums that PNG uses are available on their own,
for reading ZIP entries, zlib streams, and other formats built on them.

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

Result<std::vector<std::uint8_t>> text = Decompress(compressedBytes, CompressedFormat::Zlib);
if (!text)
{
    Log(LogLevel::Error, text.Error());   // for example "the compressed data is damaged: ..."
}
```

## Decompress

`Decompress(data, format, expectedSize, maximumSize)`

| Argument | Meaning |
|---|---|
| `data` | The compressed bytes |
| `format` | `CompressedFormat::Deflate` for raw deflate (RFC 1951), as stored in ZIP files; `CompressedFormat::Zlib` for deflate with the zlib header and checksum (RFC 1950), as stored in PNG |
| `expectedSize` | A hint for how much memory to reserve. Optional |
| `maximumSize` | Decompressing fails once the output would grow past this. Optional |

Every kind of deflate block is read: stored, fixed Huffman, and dynamic Huffman.
For zlib data the header check and the Adler-32 checksum are verified, so damage
is reported rather than returned as wrong bytes.

Set `maximumSize` whenever the data comes from outside the program. Deflate can
expand a thousand times, so a small file could otherwise produce gigabytes.

## Checksums

| Function | Used by |
|---|---|
| `Crc32(bytes)` | PNG, ZIP, gzip |
| `Adler32(bytes)` | zlib |

Both take the previous result as a second argument to continue a checksum across
several pieces: `Crc32(second, Crc32(first))` equals the checksum of both together.

## Limitations

- Only decompression. There is no compressor yet.
- zlib data that needs a preset dictionary gives an error; no common format uses
  one.
- gzip files need their header skipped first; there is no gzip reader.
