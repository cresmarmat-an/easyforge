#include <array>
#include <format>
#include <vector>

#include "../ByteReader.h"
#include "ImageDecoders.h"

namespace easyforge::internal
{
    namespace
    {
        // Reads one stored color of `depth` bits into RGBA.
        std::array<std::uint8_t, 4> ReadColor(ByteReader& reader, int depth, bool gray)
        {
            if (gray)
            {
                std::uint8_t value = reader.U8();
                return { value, value, value, 255 };
            }
            switch (depth)
            {
            case 15:
            case 16:
            {
                std::uint16_t value = reader.U16Little();
                auto expand = [](int five) { return static_cast<std::uint8_t>((five * 255 + 15) / 31); };
                std::uint8_t alpha = depth == 16 && (value & 0x8000) == 0 ? 0 : 255;
                return { expand((value >> 10) & 31), expand((value >> 5) & 31), expand(value & 31), alpha };
            }
            case 24:
            {
                std::uint8_t blue = reader.U8();
                std::uint8_t green = reader.U8();
                std::uint8_t red = reader.U8();
                return { red, green, blue, 255 };
            }
            case 32:
            {
                std::uint8_t blue = reader.U8();
                std::uint8_t green = reader.U8();
                std::uint8_t red = reader.U8();
                std::uint8_t alpha = reader.U8();
                return { red, green, blue, alpha };
            }
            default:
                return { 0, 0, 0, 255 };
            }
        }
    }

    Result<ImageData> DecodeTga(std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        int identifierLength = reader.U8();
        int colorMapType = reader.U8();
        int imageType = reader.U8();
        int firstEntry = reader.U16Little();
        int entryCount = reader.U16Little();
        int entryDepth = reader.U8();
        reader.Skip(4);
        int width = reader.U16Little();
        int height = reader.U16Little();
        int depth = reader.U8();
        int descriptor = reader.U8();
        if (reader.Failed())
        {
            return Failure("it is too short to be a TGA file");
        }

        bool runLength = imageType >= 9;
        int baseType = runLength ? imageType - 8 : imageType;
        if (baseType != 1 && baseType != 2 && baseType != 3)
        {
            return Failure(std::format("it has image type {}, which is not a TGA image type easyforge reads", imageType));
        }
        bool mapped = baseType == 1;
        bool gray = baseType == 3;
        if (mapped && (colorMapType != 1 || (depth != 8 && depth != 16)))
        {
            return Failure("it is a color-mapped TGA without a usable color map");
        }
        if (gray && depth != 8)
        {
            return Failure(std::format("it is a grayscale TGA with {} bits per pixel; only 8 is supported", depth));
        }
        if (!mapped && !gray && depth != 15 && depth != 16 && depth != 24 && depth != 32)
        {
            return Failure(std::format("{} bits per pixel is not a valid TGA depth", depth));
        }
        Result<> size = CheckImageSize(width, height);
        if (!size)
        {
            return Failure(size.Error());
        }

        reader.Skip(static_cast<std::size_t>(identifierLength));
        std::vector<std::array<std::uint8_t, 4>> colorMap;
        if (colorMapType == 1)
        {
            if (entryDepth != 15 && entryDepth != 16 && entryDepth != 24 && entryDepth != 32)
            {
                return Failure(std::format("its color map uses {} bits per entry, which is not valid", entryDepth));
            }
            colorMap.resize(static_cast<std::size_t>(entryCount));
            for (auto& color : colorMap)
            {
                color = ReadColor(reader, entryDepth, false);
            }
        }

        // A run-length packet covers at most 128 pixels.
        std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (pixelCount > reader.Remaining() * (runLength ? 128 : 1))
        {
            return Failure("its pixel data is cut short");
        }
        std::vector<std::array<std::uint8_t, 4>> pixels(pixelCount);

        auto readPixel = [&]() -> std::array<std::uint8_t, 4> {
            if (!mapped)
            {
                return ReadColor(reader, depth, gray);
            }
            int index = depth == 8 ? reader.U8() : reader.U16Little();
            index -= firstEntry;
            if (index < 0 || index >= static_cast<int>(colorMap.size()))
            {
                return { 0, 0, 0, 0 };
            }
            return colorMap[static_cast<std::size_t>(index)];
        };

        std::size_t filled = 0;
        while (filled < pixelCount && !reader.Failed())
        {
            if (!runLength)
            {
                pixels[filled++] = readPixel();
                continue;
            }
            int packet = reader.U8();
            std::size_t count = static_cast<std::size_t>((packet & 0x7F) + 1);
            if (count > pixelCount - filled)
            {
                return Failure("a run-length packet goes past the end of the image");
            }
            if ((packet & 0x80) != 0)
            {
                std::array<std::uint8_t, 4> color = readPixel();
                for (std::size_t index = 0; index < count; ++index)
                {
                    pixels[filled++] = color;
                }
            }
            else
            {
                for (std::size_t index = 0; index < count; ++index)
                {
                    pixels[filled++] = readPixel();
                }
            }
        }
        if (reader.Failed())
        {
            return Failure("its pixel data is cut short");
        }

        // Bit 4 of the descriptor means right to left, bit 5 means top to bottom.
        bool rightToLeft = (descriptor & 0x10) != 0;
        bool topToBottom = (descriptor & 0x20) != 0;
        ImageData image(width, height);
        for (int row = 0; row < height; ++row)
        {
            int targetRow = topToBottom ? row : height - 1 - row;
            for (int column = 0; column < width; ++column)
            {
                int targetColumn = rightToLeft ? width - 1 - column : column;
                const auto& color = pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                                           static_cast<std::size_t>(column)];
                std::uint8_t* pixel = image.Pixels.data() + static_cast<std::size_t>(targetRow) * image.Stride() +
                                      static_cast<std::size_t>(targetColumn) * 4;
                pixel[0] = color[0];
                pixel[1] = color[1];
                pixel[2] = color[2];
                pixel[3] = color[3];
            }
        }
        return image;
    }
}
