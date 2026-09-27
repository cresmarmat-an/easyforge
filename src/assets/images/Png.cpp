#include <array>
#include <cstdlib>
#include <format>
#include <string>
#include <vector>

#include <easyforge/assets/Compression.h>

#include "../ByteReader.h"
#include "ImageDecoders.h"

namespace easyforge::internal
{
    namespace
    {
        enum ColorType : std::uint8_t
        {
            Gray = 0,
            Truecolor = 2,
            Indexed = 3,
            GrayAlpha = 4,
            TruecolorAlpha = 6,
        };

        struct Header
        {
            std::uint32_t Width = 0;
            std::uint32_t Height = 0;
            int BitDepth = 0;
            int Color = 0;
            bool Interlaced = false;

            int Channels() const
            {
                switch (Color)
                {
                case Gray:
                case Indexed:
                    return 1;
                case GrayAlpha:
                    return 2;
                case Truecolor:
                    return 3;
                default:
                    return 4;
                }
            }

            int BitsPerPixel() const { return Channels() * BitDepth; }

            // Filters work on whole bytes: one pixel's worth, but at least one byte.
            int FilterStep() const { return BitsPerPixel() >= 8 ? BitsPerPixel() / 8 : 1; }

            std::size_t RowBytes(std::uint32_t width) const
            {
                return (static_cast<std::size_t>(width) * static_cast<std::size_t>(BitsPerPixel()) + 7) / 8;
            }
        };

        // Adam7 sends an image in seven passes, each a grid of every nth pixel:
        // first column, first row, column step, row step.
        constexpr std::array<std::array<std::uint32_t, 4>, 7> Adam7Passes = { {
            { 0, 0, 8, 8 },
            { 4, 0, 8, 8 },
            { 0, 4, 4, 8 },
            { 2, 0, 4, 4 },
            { 0, 2, 2, 4 },
            { 1, 0, 2, 2 },
            { 0, 1, 1, 2 },
        } };

        struct Transparency
        {
            bool Present = false;
            std::array<std::uint16_t, 3> Key {};
        };

        bool ValidDepth(int color, int depth)
        {
            switch (color)
            {
            case Gray:
                return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
            case Indexed:
                return depth == 1 || depth == 2 || depth == 4 || depth == 8;
            case Truecolor:
            case GrayAlpha:
            case TruecolorAlpha:
                return depth == 8 || depth == 16;
            default:
                return false;
            }
        }

        std::uint8_t Paeth(int left, int above, int aboveLeft)
        {
            int estimate = left + above - aboveLeft;
            int toLeft = std::abs(estimate - left);
            int toAbove = std::abs(estimate - above);
            int toAboveLeft = std::abs(estimate - aboveLeft);
            if (toLeft <= toAbove && toLeft <= toAboveLeft)
            {
                return static_cast<std::uint8_t>(left);
            }
            return static_cast<std::uint8_t>(toAbove <= toAboveLeft ? above : aboveLeft);
        }

        // Undoes a row's filter in place, given the unfiltered row above it.
        bool Unfilter(int filter, std::uint8_t* row, const std::uint8_t* previous, std::size_t length, int step)
        {
            std::size_t offset = static_cast<std::size_t>(step);
            switch (filter)
            {
            case 0:
                return true;
            case 1:
                for (std::size_t index = offset; index < length; ++index)
                {
                    row[index] = static_cast<std::uint8_t>(row[index] + row[index - offset]);
                }
                return true;
            case 2:
                for (std::size_t index = 0; index < length; ++index)
                {
                    row[index] = static_cast<std::uint8_t>(row[index] + previous[index]);
                }
                return true;
            case 3:
                for (std::size_t index = 0; index < length; ++index)
                {
                    int left = index >= offset ? row[index - offset] : 0;
                    row[index] = static_cast<std::uint8_t>(row[index] + ((left + previous[index]) >> 1));
                }
                return true;
            case 4:
                for (std::size_t index = 0; index < length; ++index)
                {
                    int left = index >= offset ? row[index - offset] : 0;
                    int aboveLeft = index >= offset ? previous[index - offset] : 0;
                    row[index] = static_cast<std::uint8_t>(row[index] + Paeth(left, previous[index], aboveLeft));
                }
                return true;
            default:
                return false;
            }
        }

        // Reads sample `index` of a row, for any bit depth.
        std::uint16_t Sample(const std::uint8_t* row, std::size_t index, int depth)
        {
            if (depth == 8)
            {
                return row[index];
            }
            if (depth == 16)
            {
                return static_cast<std::uint16_t>((row[index * 2] << 8) | row[index * 2 + 1]);
            }
            std::size_t bit = index * static_cast<std::size_t>(depth);
            int shift = 8 - depth - static_cast<int>(bit % 8);
            return static_cast<std::uint16_t>((row[bit / 8] >> shift) & ((1 << depth) - 1));
        }

        std::uint8_t ToEightBits(std::uint16_t value, int depth)
        {
            switch (depth)
            {
            case 1:
                return static_cast<std::uint8_t>(value * 255);
            case 2:
                return static_cast<std::uint8_t>(value * 85);
            case 4:
                return static_cast<std::uint8_t>(value * 17);
            case 16:
                return static_cast<std::uint8_t>(value >> 8);
            default:
                return static_cast<std::uint8_t>(value);
            }
        }

        struct Context
        {
            Header Info;
            std::vector<std::array<std::uint8_t, 4>> Palette;
            Transparency Key;
        };

        // Writes one unfiltered row of `width` pixels into the image, placing them
        // `step` apart starting at column `firstColumn`.
        Result<> StoreRow(const Context& context, const std::uint8_t* row, std::uint32_t width, std::uint8_t* output,
            std::size_t firstColumn, std::size_t step)
        {
            const Header& info = context.Info;
            int depth = info.BitDepth;
            for (std::uint32_t column = 0; column < width; ++column)
            {
                std::uint8_t* pixel = output + (firstColumn + column * step) * 4;
                std::size_t base = static_cast<std::size_t>(column) * static_cast<std::size_t>(info.Channels());
                switch (info.Color)
                {
                case Gray:
                {
                    std::uint16_t gray = Sample(row, base, depth);
                    std::uint8_t value = ToEightBits(gray, depth);
                    pixel[0] = pixel[1] = pixel[2] = value;
                    pixel[3] = context.Key.Present && gray == context.Key.Key[0] ? 0 : 255;
                    break;
                }
                case GrayAlpha:
                    pixel[0] = pixel[1] = pixel[2] = ToEightBits(Sample(row, base, depth), depth);
                    pixel[3] = ToEightBits(Sample(row, base + 1, depth), depth);
                    break;
                case Truecolor:
                {
                    std::uint16_t red = Sample(row, base, depth);
                    std::uint16_t green = Sample(row, base + 1, depth);
                    std::uint16_t blue = Sample(row, base + 2, depth);
                    pixel[0] = ToEightBits(red, depth);
                    pixel[1] = ToEightBits(green, depth);
                    pixel[2] = ToEightBits(blue, depth);
                    bool transparent = context.Key.Present && red == context.Key.Key[0] &&
                                       green == context.Key.Key[1] && blue == context.Key.Key[2];
                    pixel[3] = transparent ? 0 : 255;
                    break;
                }
                case TruecolorAlpha:
                    for (int channel = 0; channel < 4; ++channel)
                    {
                        pixel[channel] = ToEightBits(Sample(row, base + static_cast<std::size_t>(channel), depth), depth);
                    }
                    break;
                case Indexed:
                {
                    std::uint16_t index = Sample(row, base, depth);
                    if (index >= context.Palette.size())
                    {
                        return Failure(std::format("a pixel uses color {} of a palette with {} colors", index,
                            context.Palette.size()));
                    }
                    const std::array<std::uint8_t, 4>& color = context.Palette[index];
                    pixel[0] = color[0];
                    pixel[1] = color[1];
                    pixel[2] = color[2];
                    pixel[3] = color[3];
                    break;
                }
                default:
                    break;
                }
            }
            return {};
        }

        // Unfilters and stores one image, or one pass of an interlaced image.
        Result<> DecodePass(const Context& context, std::span<const std::uint8_t> data, std::size_t& position,
            std::uint32_t width, std::uint32_t height, std::uint8_t* output, std::size_t outputStride,
            std::size_t firstColumn, std::size_t firstRow, std::size_t columnStep, std::size_t rowStep)
        {
            if (width == 0 || height == 0)
            {
                return {};
            }
            std::size_t rowBytes = context.Info.RowBytes(width);
            std::vector<std::uint8_t> previous(rowBytes, 0);
            std::vector<std::uint8_t> current(rowBytes);
            for (std::uint32_t row = 0; row < height; ++row)
            {
                if (data.size() - position < rowBytes + 1)
                {
                    return Failure("its image data is cut short");
                }
                int filter = data[position];
                std::copy(data.begin() + static_cast<std::ptrdiff_t>(position + 1),
                    data.begin() + static_cast<std::ptrdiff_t>(position + 1 + rowBytes), current.begin());
                position += rowBytes + 1;

                if (!Unfilter(filter, current.data(), previous.data(), rowBytes, context.Info.FilterStep()))
                {
                    return Failure(std::format("a row uses the unknown filter type {}", filter));
                }
                std::uint8_t* rowStart = output + (firstRow + row * rowStep) * outputStride;
                Result<> stored = StoreRow(context, current.data(), width, rowStart, firstColumn, columnStep);
                if (!stored)
                {
                    return stored;
                }
                std::swap(previous, current);
            }
            return {};
        }
    }

    Result<ImageData> DecodePng(std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        if (!reader.Matches("\x89PNG\r\n\x1a\n"))
        {
            return Failure("it is not a PNG file");
        }

        Context context;
        bool haveHeader = false;
        bool ended = false;
        std::vector<std::uint8_t> compressed;
        std::vector<std::uint8_t> paletteAlpha;

        while (!reader.AtEnd() && !ended)
        {
            std::uint32_t length = reader.U32Big();
            std::size_t typeStart = reader.Position();
            std::span<const std::uint8_t> typeBytes = reader.Take(4);
            std::span<const std::uint8_t> data = reader.Take(length);
            std::uint32_t storedCrc = reader.U32Big();
            if (reader.Failed())
            {
                return Failure("it is cut short in the middle of a chunk");
            }

            std::string type(typeBytes.begin(), typeBytes.end());
            if (Crc32(bytes.subspan(typeStart, 4 + static_cast<std::size_t>(length))) != storedCrc)
            {
                return Failure(std::format("its {} chunk is damaged (the CRC does not match)", type));
            }
            if (!haveHeader && type != "IHDR")
            {
                return Failure("it does not start with an IHDR chunk");
            }

            ByteReader chunk(data);
            if (type == "IHDR")
            {
                Header& info = context.Info;
                info.Width = chunk.U32Big();
                info.Height = chunk.U32Big();
                info.BitDepth = chunk.U8();
                info.Color = chunk.U8();
                int compression = chunk.U8();
                int filterMethod = chunk.U8();
                int interlace = chunk.U8();
                if (chunk.Failed())
                {
                    return Failure("its IHDR chunk is too short");
                }
                Result<> size = CheckImageSize(info.Width, info.Height);
                if (!size)
                {
                    return Failure(size.Error());
                }
                if (!ValidDepth(info.Color, info.BitDepth))
                {
                    return Failure(std::format("color type {} with bit depth {} is not valid", info.Color, info.BitDepth));
                }
                if (compression != 0 || filterMethod != 0 || interlace > 1)
                {
                    return Failure("it uses a compression, filter, or interlace method PNG does not define");
                }
                info.Interlaced = interlace == 1;
                haveHeader = true;
            }
            else if (type == "PLTE")
            {
                if (length % 3 != 0 || length / 3 > 256 || length == 0)
                {
                    return Failure("its palette has an invalid size");
                }
                context.Palette.resize(length / 3);
                for (auto& color : context.Palette)
                {
                    color = { chunk.U8(), chunk.U8(), chunk.U8(), 255 };
                }
            }
            else if (type == "tRNS")
            {
                if (context.Info.Color == Indexed)
                {
                    paletteAlpha.assign(data.begin(), data.end());
                }
                else if (context.Info.Color == Gray)
                {
                    context.Key.Present = true;
                    context.Key.Key[0] = chunk.U16Big();
                }
                else if (context.Info.Color == Truecolor)
                {
                    context.Key.Present = true;
                    context.Key.Key = { chunk.U16Big(), chunk.U16Big(), chunk.U16Big() };
                }
                if (chunk.Failed())
                {
                    return Failure("its tRNS chunk is too short");
                }
            }
            else if (type == "IDAT")
            {
                compressed.insert(compressed.end(), data.begin(), data.end());
            }
            else if (type == "IEND")
            {
                ended = true;
            }
            else if ((type[0] & 0x20) == 0)
            {
                // An uppercase first letter marks a chunk needed to show the image correctly.
                return Failure(std::format("it uses the {} chunk, which this decoder does not know", type));
            }
        }

        if (!haveHeader)
        {
            return Failure("it has no IHDR chunk");
        }
        if (compressed.empty())
        {
            return Failure("it has no image data");
        }
        if (context.Info.Color == Indexed)
        {
            if (context.Palette.empty())
            {
                return Failure("it uses a palette but has no PLTE chunk");
            }
            for (std::size_t index = 0; index < paletteAlpha.size() && index < context.Palette.size(); ++index)
            {
                context.Palette[index][3] = paletteAlpha[index];
            }
        }

        // The exact amount of filtered data is known, so no more than that is
        // decompressed, and all of it must be there before the image is made.
        const Header& info = context.Info;
        std::size_t expected = 0;
        if (!info.Interlaced)
        {
            expected = (info.RowBytes(info.Width) + 1) * info.Height;
        }
        else
        {
            for (const auto& pass : Adam7Passes)
            {
                std::uint32_t passWidth = info.Width > pass[0] ? (info.Width - pass[0] + pass[2] - 1) / pass[2] : 0;
                std::uint32_t passHeight = info.Height > pass[1] ? (info.Height - pass[1] + pass[3] - 1) / pass[3] : 0;
                if (passWidth > 0 && passHeight > 0)
                {
                    expected += (info.RowBytes(passWidth) + 1) * passHeight;
                }
            }
        }
        Result<std::vector<std::uint8_t>> inflated = Decompress(compressed, CompressedFormat::Zlib, expected, expected);
        if (!inflated)
        {
            return Failure(inflated.Error());
        }
        if (inflated->size() < expected)
        {
            return Failure("its image data is cut short");
        }

        ImageData image(static_cast<int>(info.Width), static_cast<int>(info.Height));
        std::size_t stride = image.Stride();
        std::size_t position = 0;
        if (!info.Interlaced)
        {
            Result<> decoded =
                DecodePass(context, *inflated, position, info.Width, info.Height, image.Pixels.data(), stride, 0, 0, 1, 1);
            if (!decoded)
            {
                return Failure(decoded.Error());
            }
        }
        else
        {
            for (const auto& pass : Adam7Passes)
            {
                std::uint32_t passWidth = info.Width > pass[0] ? (info.Width - pass[0] + pass[2] - 1) / pass[2] : 0;
                std::uint32_t passHeight = info.Height > pass[1] ? (info.Height - pass[1] + pass[3] - 1) / pass[3] : 0;
                Result<> decoded = DecodePass(context, *inflated, position, passWidth, passHeight, image.Pixels.data(),
                    stride, pass[0], pass[1], pass[2], pass[3]);
                if (!decoded)
                {
                    return Failure(decoded.Error());
                }
            }
        }
        return image;
    }
}
