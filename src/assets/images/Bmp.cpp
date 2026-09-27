#include <array>
#include <bit>
#include <format>
#include <vector>

#include "../ByteReader.h"
#include "ImageDecoders.h"

namespace easyforge::internal
{
    namespace
    {
        constexpr std::uint32_t Uncompressed = 0;
        constexpr std::uint32_t RunLength8 = 1;
        constexpr std::uint32_t RunLength4 = 2;
        constexpr std::uint32_t BitFields = 3;
        constexpr std::uint32_t AlphaBitFields = 6;

        // Extracts one channel described by a bit mask and scales it to 8 bits.
        struct Channel
        {
            std::uint32_t Mask = 0;
            int Shift = 0;
            int Bits = 0;

            explicit Channel(std::uint32_t mask = 0) : Mask(mask)
            {
                if (mask != 0)
                {
                    Shift = std::countr_zero(mask);
                    Bits = std::popcount(mask);
                }
            }

            std::uint8_t From(std::uint32_t pixel, std::uint8_t missing) const
            {
                if (Mask == 0)
                {
                    return missing;
                }
                std::uint32_t value = (pixel & Mask) >> Shift;
                std::uint32_t largest = Bits >= 32 ? 0xFFFFFFFFu : (1u << Bits) - 1;
                return static_cast<std::uint8_t>((static_cast<std::uint64_t>(value) * 255 + largest / 2) / largest);
            }
        };
    }

    Result<ImageData> DecodeBmp(std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        if (!reader.Matches("BM"))
        {
            return Failure("it is not a BMP file");
        }
        reader.Skip(8);
        std::uint32_t pixelOffset = reader.U32Little();
        std::uint32_t headerSize = reader.U32Little();

        std::int64_t width = 0;
        std::int64_t height = 0;
        int bitsPerPixel = 0;
        std::uint32_t compression = Uncompressed;
        std::uint32_t paletteCount = 0;
        int paletteEntrySize = 4;
        std::array<std::uint32_t, 4> masks {};
        bool haveMasks = false;

        if (headerSize == 12)
        {
            width = reader.U16Little();
            height = reader.I16Little();
            reader.Skip(2);
            bitsPerPixel = reader.U16Little();
            paletteEntrySize = 3;
        }
        else if (headerSize >= 40)
        {
            width = reader.I32Little();
            height = reader.I32Little();
            reader.Skip(2);
            bitsPerPixel = reader.U16Little();
            compression = reader.U32Little();
            reader.Skip(12);
            paletteCount = reader.U32Little();
            reader.Skip(4);

            if (compression == BitFields || compression == AlphaBitFields || headerSize >= 52)
            {
                // Version 2 and later headers hold the masks; the 40-byte header is followed by them.
                bool alpha = compression == AlphaBitFields || headerSize >= 56;
                masks = { reader.U32Little(), reader.U32Little(), reader.U32Little(), alpha ? reader.U32Little() : 0 };
                haveMasks = compression == BitFields || compression == AlphaBitFields;
            }
            reader.Seek(14 + headerSize + (headerSize == 40 && haveMasks ? (compression == AlphaBitFields ? 16 : 12) : 0));
        }
        else
        {
            return Failure(std::format("it has a {}-byte header, which is not a known BMP version", headerSize));
        }
        if (reader.Failed())
        {
            return Failure("its header is cut short");
        }

        if (compression == RunLength8 || compression == RunLength4)
        {
            return Failure("it is a run-length compressed BMP, which is not supported");
        }
        if (compression != Uncompressed && compression != BitFields && compression != AlphaBitFields)
        {
            return Failure(std::format("it uses compression method {}, which is not supported", compression));
        }

        bool topDown = height < 0;
        height = height < 0 ? -height : height;
        Result<> size = CheckImageSize(width, height);
        if (!size)
        {
            return Failure(size.Error());
        }

        std::vector<std::array<std::uint8_t, 4>> palette;
        if (bitsPerPixel <= 8)
        {
            if (bitsPerPixel != 1 && bitsPerPixel != 2 && bitsPerPixel != 4 && bitsPerPixel != 8)
            {
                return Failure(std::format("{} bits per pixel is not a valid BMP depth", bitsPerPixel));
            }
            std::uint32_t count = paletteCount != 0 ? paletteCount : (1u << bitsPerPixel);
            if (count > 256)
            {
                return Failure("its palette has more than 256 colors");
            }
            palette.resize(count);
            for (auto& color : palette)
            {
                std::uint8_t blue = reader.U8();
                std::uint8_t green = reader.U8();
                std::uint8_t red = reader.U8();
                if (paletteEntrySize == 4)
                {
                    reader.Skip(1);
                }
                color = { red, green, blue, 255 };
            }
            if (reader.Failed())
            {
                return Failure("its palette is cut short");
            }
        }
        else if (bitsPerPixel != 16 && bitsPerPixel != 24 && bitsPerPixel != 32)
        {
            return Failure(std::format("{} bits per pixel is not a valid BMP depth", bitsPerPixel));
        }

        if (!haveMasks)
        {
            if (bitsPerPixel == 16)
            {
                masks = { 0x7C00, 0x03E0, 0x001F, 0 };
            }
            else if (bitsPerPixel == 32)
            {
                masks = { 0x00FF0000, 0x0000FF00, 0x000000FF, 0 };
            }
        }
        Channel red(masks[0]);
        Channel green(masks[1]);
        Channel blue(masks[2]);
        Channel alpha(masks[3]);

        std::size_t rowBytes = ((static_cast<std::size_t>(width) * static_cast<std::size_t>(bitsPerPixel) + 31) / 32) * 4;
        if (pixelOffset > bytes.size() || rowBytes * static_cast<std::size_t>(height) > bytes.size() - pixelOffset)
        {
            return Failure("its pixel data is cut short");
        }

        ImageData image(static_cast<int>(width), static_cast<int>(height));
        bool anyAlpha = false;
        for (std::int64_t row = 0; row < height; ++row)
        {
            const std::uint8_t* source = bytes.data() + pixelOffset + static_cast<std::size_t>(row) * rowBytes;
            std::int64_t targetRow = topDown ? row : height - 1 - row;
            std::uint8_t* pixel = image.Pixels.data() + static_cast<std::size_t>(targetRow) * image.Stride();
            for (std::int64_t column = 0; column < width; ++column, pixel += 4)
            {
                if (bitsPerPixel <= 8)
                {
                    std::size_t bit = static_cast<std::size_t>(column) * static_cast<std::size_t>(bitsPerPixel);
                    int shift = 8 - bitsPerPixel - static_cast<int>(bit % 8);
                    std::size_t index = (source[bit / 8] >> shift) & ((1u << bitsPerPixel) - 1);
                    if (index >= palette.size())
                    {
                        return Failure("a pixel uses a color outside the palette");
                    }
                    const auto& color = palette[index];
                    pixel[0] = color[0];
                    pixel[1] = color[1];
                    pixel[2] = color[2];
                    pixel[3] = 255;
                }
                else if (bitsPerPixel == 24)
                {
                    const std::uint8_t* bgr = source + column * 3;
                    pixel[0] = bgr[2];
                    pixel[1] = bgr[1];
                    pixel[2] = bgr[0];
                    pixel[3] = 255;
                }
                else
                {
                    std::uint32_t value = 0;
                    int byteCount = bitsPerPixel / 8;
                    for (int byte = 0; byte < byteCount; ++byte)
                    {
                        value |= static_cast<std::uint32_t>(source[column * byteCount + byte]) << (8 * byte);
                    }
                    pixel[0] = red.From(value, 0);
                    pixel[1] = green.From(value, 0);
                    pixel[2] = blue.From(value, 0);
                    pixel[3] = alpha.From(value, 255);
                    anyAlpha = anyAlpha || (alpha.Mask != 0 && pixel[3] != 0);
                }
            }
        }

        // Many programs write an alpha mask but leave every alpha at zero; such an
        // image is meant to be opaque, not invisible.
        if (alpha.Mask != 0 && !anyAlpha)
        {
            for (std::size_t index = 3; index < image.Pixels.size(); index += 4)
            {
                image.Pixels[index] = 255;
            }
        }
        return image;
    }
}
