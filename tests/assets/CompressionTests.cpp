#include <easyforge/core/Testing.h>

#include <string_view>

#include "TestData.h"

using namespace easyforge;

namespace
{
    std::span<const std::uint8_t> Text(std::string_view text)
    {
        return { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() };
    }
}

EASYFORGE_TEST(ChecksumsMatchKnownValues)
{
    EASYFORGE_EXPECT_EQUAL(Crc32(Text("123456789")), 0xCBF43926u);
    EASYFORGE_EXPECT_EQUAL(Crc32(Text("")), 0u);
    EASYFORGE_EXPECT_EQUAL(Adler32(Text("Wikipedia")), 0x11E60398u);
    EASYFORGE_EXPECT_EQUAL(Adler32(Text("")), 1u);

    // A checksum can be continued across pieces.
    EASYFORGE_EXPECT_EQUAL(Crc32(Text("56789"), Crc32(Text("1234"))), 0xCBF43926u);
    EASYFORGE_EXPECT_EQUAL(Adler32(Text("pedia"), Adler32(Text("Wiki"))), 0x11E60398u);
}

EASYFORGE_TEST(DecompressEveryBlockType)
{
    std::vector<std::uint8_t> original = testdata::Read("compression/original.bin");
    EASYFORGE_REQUIRE(!original.empty());

    for (const char* name : { "compression/dynamic.zlib", "compression/stored.zlib", "compression/fixed.zlib",
             "compression/blocks.zlib" })
    {
        Result<std::vector<std::uint8_t>> result = Decompress(testdata::Read(name), CompressedFormat::Zlib, original.size());
        EASYFORGE_EXPECT(result);
        EASYFORGE_EXPECT(result.Succeeded() && result.Get() == original);
    }

    Result<std::vector<std::uint8_t>> raw = Decompress(testdata::Read("compression/raw.deflate"), CompressedFormat::Deflate);
    EASYFORGE_REQUIRE(raw);
    EASYFORGE_EXPECT(raw.Get() == original);
}

EASYFORGE_TEST(DecompressReportsDamage)
{
    std::vector<std::uint8_t> data = testdata::Read("compression/dynamic.zlib");
    EASYFORGE_REQUIRE(data.size() > 10);

    std::vector<std::uint8_t> checksum = data;
    checksum.back() ^= 0xFF;
    Result<std::vector<std::uint8_t>> result = Decompress(checksum, CompressedFormat::Zlib);
    EASYFORGE_EXPECT(!result);
    EASYFORGE_EXPECT(result.Error().find("Adler-32") != std::string::npos);

    std::vector<std::uint8_t> header = data;
    header[0] = 0x79;
    EASYFORGE_EXPECT(!Decompress(header, CompressedFormat::Zlib));

    std::vector<std::uint8_t> cut(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(data.size() / 2));
    EASYFORGE_EXPECT(!Decompress(cut, CompressedFormat::Zlib));
}

EASYFORGE_TEST(DecompressStopsAtTheLimit)
{
    std::vector<std::uint8_t> original = testdata::Read("compression/original.bin");
    Result<std::vector<std::uint8_t>> limited =
        Decompress(testdata::Read("compression/dynamic.zlib"), CompressedFormat::Zlib, 0, original.size() - 1);
    EASYFORGE_EXPECT(!limited);
    EASYFORGE_EXPECT(limited.Error().find("larger than expected") != std::string::npos);

    Result<std::vector<std::uint8_t>> exact =
        Decompress(testdata::Read("compression/stored.zlib"), CompressedFormat::Zlib, 0, original.size());
    EASYFORGE_EXPECT(exact);
}
