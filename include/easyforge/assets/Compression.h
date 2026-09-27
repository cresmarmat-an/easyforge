#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <easyforge/core/Result.h>

namespace easyforge
{
    enum class CompressedFormat
    {
        // Raw deflate data (RFC 1951), as stored inside ZIP files.
        Deflate,

        // Deflate data with the two-byte zlib header and Adler-32 checksum
        // (RFC 1950), as stored in PNG and FBX files.
        Zlib,
    };

    // Decompresses deflate or zlib data. `expectedSize` is only a hint for how
    // much memory to reserve. Decompressing stops with a failure once the output
    // would pass `maximumSize`, which protects against small files that expand to
    // huge amounts of data. Damaged data gives a failure that says what was wrong;
    // a zlib checksum that does not match is reported as damage too.
    Result<std::vector<std::uint8_t>> Decompress(std::span<const std::uint8_t> data, CompressedFormat format,
        std::size_t expectedSize = 0, std::size_t maximumSize = SIZE_MAX);

    // The CRC-32 checksum used by PNG, ZIP, and gzip. Pass the previous result to
    // continue a checksum across several blocks.
    std::uint32_t Crc32(std::span<const std::uint8_t> data, std::uint32_t previous = 0);

    // The Adler-32 checksum used by zlib.
    std::uint32_t Adler32(std::span<const std::uint8_t> data, std::uint32_t previous = 1);
}
