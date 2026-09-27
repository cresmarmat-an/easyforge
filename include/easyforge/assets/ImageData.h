#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/assets/Pending.h>
#include <easyforge/core/Color.h>

namespace easyforge
{
    // An image in memory: 8-bit red, green, blue, and alpha for each pixel, rows
    // from top to bottom, colors in sRGB.
    //
    //     ImageData logo = ImageData::Load("logo.png");
    //     if (!logo)
    //     {
    //         Log(logo.Error());
    //     }
    //
    // Reads PNG, JPEG (baseline and extended sequential), BMP, TGA, and QOI. The
    // format is recognized from the file's contents; only TGA, which has no
    // signature, is recognized by its ".tga" extension.
    class ImageData
    {
    public:
        // Decoders refuse images wider or taller than this, or with more than
        // MaximumPixels pixels, so a damaged file cannot ask for huge amounts of memory.
        static constexpr int MaximumSide = 32768;
        static constexpr std::int64_t MaximumPixels = std::int64_t { 1 } << 28;

        int Width = 0;
        int Height = 0;

        // Width * Height * 4 bytes: red, green, blue, alpha.
        std::vector<std::uint8_t> Pixels;

        // An empty image. It tests as false, with no error.
        ImageData() = default;

        // A transparent black image of the given size.
        ImageData(int width, int height);

        // An image from pixels you already have; `pixels` must hold width * height * 4 bytes.
        ImageData(int width, int height, std::vector<std::uint8_t> pixels);

        static ImageData Load(std::string_view path);

        // Decodes an image from bytes in memory. `name` is used in error messages
        // and to recognize TGA files.
        static ImageData Decode(std::span<const std::uint8_t> bytes, std::string_view name = "image");

        static Pending<ImageData> LoadInBackground(std::string_view path);

        // True when the image has pixels.
        explicit operator bool() const { return Width > 0 && Height > 0 && ErrorText.empty(); }

        // Why loading failed, or empty.
        const std::string& Error() const { return ErrorText; }

        // Bytes from the start of one row to the next.
        std::size_t Stride() const { return static_cast<std::size_t>(Width) * 4; }

        // Positions outside the image read as transparent black and are ignored when written.
        Color ColorAt(int x, int y) const;
        void SetColorAt(int x, int y, Color color);

        // A copy scaled to the given size. Shrinking averages every pixel each new
        // pixel covers; growing blends the nearest four. Colors are blended in
        // linear light, weighted by alpha, so a transparent pixel's color never
        // bleeds into its neighbors.
        ImageData Resized(int width, int height) const;

    private:
        std::string ErrorText;
    };
}
