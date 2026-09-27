// easyforge-icon makes the icon files a program is built with, from one image.
// The easyforge_app_icon CMake function runs it; it can also be run by hand:
//
//     easyforge-icon <image> <output.ico>
//
// The .ico file holds the image at every size Windows asks for, from 16 to 256
// pixels, each one scaled from the original rather than from each other.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <easyforge/assets.h>

using namespace easyforge;

namespace
{
    constexpr int Sizes[] = { 16, 20, 24, 32, 40, 48, 64, 96, 128, 256 };

    // The image scaled to fit a square, keeping its shape, centered.
    ImageData FitSquare(const ImageData& image, int size)
    {
        int width = image.Width >= image.Height ? size : Max(1, image.Width * size / image.Height);
        int height = image.Height >= image.Width ? size : Max(1, image.Height * size / image.Width);
        ImageData scaled = image.Resized(width, height);
        ImageData square(size, size);
        int left = (size - width) / 2;
        int top = (size - height) / 2;
        for (int y = 0; y < height; ++y)
        {
            std::memcpy(square.Pixels.data() + static_cast<std::size_t>(top + y) * square.Stride() + left * 4,
                scaled.Pixels.data() + static_cast<std::size_t>(y) * scaled.Stride(), scaled.Stride());
        }
        return square;
    }

    void Write16(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    void Write32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        Write16(bytes, value & 0xFFFF);
        Write16(bytes, value >> 16);
    }

    // One icon image: a bitmap header, the pixels from the bottom row up in
    // blue, green, red, alpha order, and a transparency mask that Windows ignores
    // when the pixels have alpha.
    std::vector<std::uint8_t> EncodeBitmap(const ImageData& image)
    {
        std::uint32_t maskStride = static_cast<std::uint32_t>((image.Width + 31) / 32 * 4);
        std::uint32_t pixelBytes = static_cast<std::uint32_t>(image.Width * image.Height * 4);
        std::uint32_t maskBytes = maskStride * static_cast<std::uint32_t>(image.Height);

        std::vector<std::uint8_t> bytes;
        Write32(bytes, 40);
        Write32(bytes, static_cast<std::uint32_t>(image.Width));
        Write32(bytes, static_cast<std::uint32_t>(image.Height * 2));
        Write16(bytes, 1);
        Write16(bytes, 32);
        Write32(bytes, 0);
        Write32(bytes, pixelBytes + maskBytes);
        Write32(bytes, 0);
        Write32(bytes, 0);
        Write32(bytes, 0);
        Write32(bytes, 0);

        for (int y = image.Height - 1; y >= 0; --y)
        {
            const std::uint8_t* row = image.Pixels.data() + static_cast<std::size_t>(y) * image.Stride();
            for (int x = 0; x < image.Width; ++x)
            {
                const std::uint8_t* pixel = row + x * 4;
                bytes.push_back(pixel[2]);
                bytes.push_back(pixel[1]);
                bytes.push_back(pixel[0]);
                bytes.push_back(pixel[3]);
            }
        }
        bytes.insert(bytes.end(), maskBytes, 0);
        return bytes;
    }
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 3)
    {
        std::fputs("usage: easyforge-icon <image> <output.ico>\n", stderr);
        return 2;
    }

    ImageData image = ImageData::Load(arguments[1]);
    if (!image)
    {
        std::fprintf(stderr, "easyforge-icon: %s\n", image.Error().c_str());
        return 1;
    }

    std::vector<std::vector<std::uint8_t>> bitmaps;
    for (int size : Sizes)
    {
        bitmaps.push_back(EncodeBitmap(FitSquare(image, size)));
    }

    std::size_t count = std::size(Sizes);
    std::vector<std::uint8_t> file;
    Write16(file, 0);
    Write16(file, 1);
    Write16(file, static_cast<std::uint32_t>(count));
    std::uint32_t offset = static_cast<std::uint32_t>(6 + 16 * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        int size = Sizes[index];
        // A size of 256 is written as 0, since it does not fit in a byte.
        file.push_back(static_cast<std::uint8_t>(size == 256 ? 0 : size));
        file.push_back(static_cast<std::uint8_t>(size == 256 ? 0 : size));
        file.push_back(0);
        file.push_back(0);
        Write16(file, 1);
        Write16(file, 32);
        Write32(file, static_cast<std::uint32_t>(bitmaps[index].size()));
        Write32(file, offset);
        offset += static_cast<std::uint32_t>(bitmaps[index].size());
    }
    for (const std::vector<std::uint8_t>& bitmap : bitmaps)
    {
        file.insert(file.end(), bitmap.begin(), bitmap.end());
    }

    std::ofstream output(arguments[2], std::ios::binary);
    output.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!output)
    {
        std::fprintf(stderr, "easyforge-icon: could not write %s\n", arguments[2]);
        return 1;
    }
    return 0;
}
