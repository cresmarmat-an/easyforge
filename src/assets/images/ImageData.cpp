#include <easyforge/assets/ImageData.h>

#include <cmath>
#include <cstring>
#include <format>

#include <easyforge/assets/Files.h>

#include "ImageDecoders.h"

namespace easyforge
{
    namespace internal
    {
        Result<> CheckImageSize(std::int64_t width, std::int64_t height)
        {
            if (width <= 0 || height <= 0)
            {
                return Failure(std::format("its size, {} by {}, has no pixels", width, height));
            }
            if (width > ImageData::MaximumSide || height > ImageData::MaximumSide ||
                width * height > ImageData::MaximumPixels)
            {
                return Failure(std::format("it is {} by {} pixels, larger than easyforge accepts", width, height));
            }
            return {};
        }
    }

    namespace
    {
        bool StartsWith(std::span<const std::uint8_t> bytes, std::string_view signature)
        {
            return bytes.size() >= signature.size() && std::memcmp(bytes.data(), signature.data(), signature.size()) == 0;
        }

        std::uint8_t ToByte(float component)
        {
            return static_cast<std::uint8_t>(std::lround(Clamp(component, 0.0f, 1.0f) * 255.0f));
        }
    }

    ImageData::ImageData(int width, int height)
        : Width(width > 0 && height > 0 ? width : 0), Height(width > 0 && height > 0 ? height : 0),
          Pixels(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4, 0)
    {
    }

    ImageData::ImageData(int width, int height, std::vector<std::uint8_t> pixels)
        : Width(width), Height(height), Pixels(std::move(pixels))
    {
        std::size_t expected = width > 0 && height > 0 ? static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4 : 0;
        if (expected == 0 || Pixels.size() != expected)
        {
            ErrorText = std::format("an image of {} by {} needs {} bytes of pixels, but {} were given", width, height,
                expected, Pixels.size());
            Width = 0;
            Height = 0;
            Pixels.clear();
        }
    }

    ImageData ImageData::Load(std::string_view path)
    {
        Result<std::vector<std::uint8_t>> bytes = Files::Read(path);
        if (!bytes)
        {
            ImageData failed;
            failed.ErrorText = bytes.Error();
            return failed;
        }
        return Decode(*bytes, path);
    }

    ImageData ImageData::Decode(std::span<const std::uint8_t> bytes, std::string_view name)
    {
        Result<ImageData> decoded = Failure("it is not an image format easyforge can read (PNG, JPEG, BMP, TGA, or QOI)");
        if (StartsWith(bytes, "\x89PNG\r\n\x1a\n"))
        {
            decoded = internal::DecodePng(bytes);
        }
        else if (StartsWith(bytes, "\xFF\xD8\xFF"))
        {
            decoded = internal::DecodeJpeg(bytes);
        }
        else if (StartsWith(bytes, "BM"))
        {
            decoded = internal::DecodeBmp(bytes);
        }
        else if (StartsWith(bytes, "qoif"))
        {
            decoded = internal::DecodeQoi(bytes);
        }
        else if (Files::ExtensionOf(name) == ".tga")
        {
            decoded = internal::DecodeTga(bytes);
        }

        if (!decoded)
        {
            ImageData failed;
            failed.ErrorText = std::format("{}: {}", name, decoded.Error());
            return failed;
        }
        return std::move(decoded).Get();
    }

    Pending<ImageData> ImageData::LoadInBackground(std::string_view path)
    {
        return Pending<ImageData>(Jobs::Shared().Run([path = std::string(path)] { return Load(path); }));
    }

    Color ImageData::ColorAt(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= Width || y >= Height)
        {
            return Color::Transparent;
        }
        const std::uint8_t* pixel = Pixels.data() + (static_cast<std::size_t>(y) * Stride() + static_cast<std::size_t>(x) * 4);
        return Color::FromBytes(pixel[0], pixel[1], pixel[2], pixel[3]);
    }

    void ImageData::SetColorAt(int x, int y, Color color)
    {
        if (x < 0 || y < 0 || x >= Width || y >= Height)
        {
            return;
        }
        std::uint8_t* pixel = Pixels.data() + (static_cast<std::size_t>(y) * Stride() + static_cast<std::size_t>(x) * 4);
        pixel[0] = ToByte(color.Red);
        pixel[1] = ToByte(color.Green);
        pixel[2] = ToByte(color.Blue);
        pixel[3] = ToByte(color.Alpha);
    }
}
