#include <array>
#include <format>

#include "../ByteReader.h"
#include "ImageDecoders.h"

namespace easyforge::internal
{
    Result<ImageData> DecodeQoi(std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        if (!reader.Matches("qoif"))
        {
            return Failure("it is not a QOI file");
        }
        std::uint32_t width = reader.U32Big();
        std::uint32_t height = reader.U32Big();
        int channels = reader.U8();
        reader.Skip(1);
        if (reader.Failed())
        {
            return Failure("its header is cut short");
        }
        if (channels != 3 && channels != 4)
        {
            return Failure(std::format("it declares {} channels; QOI allows 3 or 4", channels));
        }
        Result<> size = CheckImageSize(width, height);
        if (!size)
        {
            return Failure(size.Error());
        }

        // One byte can repeat a pixel 62 times, so the data bounds the pixel count.
        if (static_cast<std::uint64_t>(width) * height > static_cast<std::uint64_t>(reader.Remaining()) * 62)
        {
            return Failure("its pixel data is cut short");
        }

        ImageData image(static_cast<int>(width), static_cast<int>(height));
        std::array<std::array<std::uint8_t, 4>, 64> seen {};
        std::array<std::uint8_t, 4> pixel = { 0, 0, 0, 255 };
        std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::size_t index = 0;

        while (index < pixelCount)
        {
            std::uint8_t tag = reader.U8();
            if (reader.Failed())
            {
                return Failure("its pixel data is cut short");
            }

            std::size_t run = 1;
            if (tag == 0xFE)
            {
                pixel[0] = reader.U8();
                pixel[1] = reader.U8();
                pixel[2] = reader.U8();
            }
            else if (tag == 0xFF)
            {
                pixel[0] = reader.U8();
                pixel[1] = reader.U8();
                pixel[2] = reader.U8();
                pixel[3] = reader.U8();
            }
            else
            {
                switch (tag >> 6)
                {
                case 0:
                    pixel = seen[tag & 0x3F];
                    break;
                case 1:
                    pixel[0] = static_cast<std::uint8_t>(pixel[0] + ((tag >> 4) & 3) - 2);
                    pixel[1] = static_cast<std::uint8_t>(pixel[1] + ((tag >> 2) & 3) - 2);
                    pixel[2] = static_cast<std::uint8_t>(pixel[2] + (tag & 3) - 2);
                    break;
                case 2:
                {
                    int greenChange = (tag & 0x3F) - 32;
                    std::uint8_t next = reader.U8();
                    pixel[0] = static_cast<std::uint8_t>(pixel[0] + greenChange - 8 + (next >> 4));
                    pixel[1] = static_cast<std::uint8_t>(pixel[1] + greenChange);
                    pixel[2] = static_cast<std::uint8_t>(pixel[2] + greenChange - 8 + (next & 15));
                    break;
                }
                default:
                    run = static_cast<std::size_t>((tag & 0x3F) + 1);
                    break;
                }
            }

            seen[static_cast<std::size_t>((pixel[0] * 3 + pixel[1] * 5 + pixel[2] * 7 + pixel[3] * 11) % 64)] = pixel;
            for (std::size_t repeat = 0; repeat < run && index < pixelCount; ++repeat, ++index)
            {
                std::uint8_t* target = image.Pixels.data() + index * 4;
                target[0] = pixel[0];
                target[1] = pixel[1];
                target[2] = pixel[2];
                target[3] = pixel[3];
            }
        }
        if (reader.Failed())
        {
            return Failure("its pixel data is cut short");
        }
        return image;
    }
}
