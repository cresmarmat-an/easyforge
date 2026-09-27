#include <easyforge/core/Testing.h>

#include <string>

#include "TestData.h"

using namespace easyforge;

namespace
{
    // Decodes images/<name> and compares it with images/<base>_<extension>.rgba exactly.
    void ExpectExact(const std::string& name)
    {
        std::size_t dot = name.find_last_of('.');
        std::string expectedName = "images/" + name.substr(0, dot) + "_" + name.substr(dot + 1) + ".rgba";
        ImageData image = ImageData::Load(testdata::Path("images/" + name));
        ImageData expected = testdata::Expected(expectedName);
        if (!image)
        {
            testing::ReportFailure(__FILE__, __LINE__, name + " did not decode: " + image.Error());
            return;
        }
        int difference = testdata::LargestDifference(image, expected);
        if (difference != 0)
        {
            testing::ReportFailure(__FILE__, __LINE__,
                name + " differs from its expected pixels by up to " + std::to_string(difference));
        }
    }
}

EASYFORGE_TEST(PngEveryColorTypeAndDepth)
{
    for (const char* name : { "rgba8.png", "rgba16.png", "rgb8.png", "rgb16.png", "rgb8_key.png", "gray1.png",
             "gray2.png", "gray4.png", "gray8.png", "gray16.png", "gray8_key.png", "gray_alpha8.png",
             "gray_alpha16.png", "palette1.png", "palette2.png", "palette4.png", "palette8.png" })
    {
        ExpectExact(name);
    }
}

EASYFORGE_TEST(PngInterlacingAndCompression)
{
    for (const char* name : { "rgba8_interlaced.png", "rgba8_stored.png", "rgba8_fixed.png" })
    {
        ExpectExact(name);
    }
}

EASYFORGE_TEST(BmpVariants)
{
    for (const char* name : { "bgr24.bmp", "bgr24_top_down.bmp", "bgr24_core.bmp", "bgra32_v4.bmp", "bgrx32.bmp",
             "rgb565.bmp", "rgb555.bmp", "palette1.bmp", "palette4.bmp", "palette8.bmp" })
    {
        ExpectExact(name);
    }
}

EASYFORGE_TEST(TgaVariants)
{
    for (const char* name : { "bgr24.tga", "bgra32_rle_top.tga", "gray8_rle.tga", "argb16.tga", "mapped8.tga" })
    {
        ExpectExact(name);
    }
}

EASYFORGE_TEST(QoiVariants)
{
    ExpectExact("smooth.qoi");
    ExpectExact("alpha.qoi");
}

EASYFORGE_TEST(JpegMatchesWindowsDecoder)
{
    // JPEG decoders may differ by rounding and upsampling, so the check allows a
    // small difference from the reference Windows decoded.
    for (const char* name : { "color444", "color420", "color422", "gray", "gdiplus" })
    {
        std::string base = name;
        ImageData image = ImageData::Load(testdata::Path("images/" + base + ".jpg"));
        EASYFORGE_REQUIRE(image);
        ImageData expected = testdata::Expected("images/" + base + "_jpg.rgba");

        int largest = testdata::LargestDifference(image, expected);
        double total = 0.0;
        for (std::size_t index = 0; index < image.Pixels.size(); ++index)
        {
            total += std::abs(static_cast<int>(image.Pixels[index]) - static_cast<int>(expected.Pixels[index]));
        }
        double average = total / static_cast<double>(image.Pixels.size());
        if (largest < 0 || largest > 12 || average > 1.5)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::format("{}.jpg: largest difference {}, average {:.2f}", base,
                largest, average));
        }
    }
}

EASYFORGE_TEST(ImageErrorsSayWhy)
{
    ImageData progressive = ImageData::Load(testdata::Path("images/progressive.jpg"));
    EASYFORGE_EXPECT(!progressive);
    EASYFORGE_EXPECT(progressive.Error().find("progressive") != std::string::npos);

    std::vector<std::uint8_t> text = { 'h', 'e', 'l', 'l', 'o' };
    ImageData unknown = ImageData::Decode(text, "notes.txt");
    EASYFORGE_EXPECT(!unknown);
    EASYFORGE_EXPECT(unknown.Error().starts_with("notes.txt: "));

    // A TGA is only recognized by its extension.
    std::vector<std::uint8_t> tga = testdata::Read("images/bgr24.tga");
    EASYFORGE_EXPECT(!ImageData::Decode(tga, "picture"));
    EASYFORGE_EXPECT(ImageData::Decode(tga, "picture.TGA"));

    ImageData missing = ImageData::Load(testdata::Path("images/missing.png"));
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(missing.Error().find("was not found") != std::string::npos);

    // A damaged chunk is caught by its checksum.
    std::vector<std::uint8_t> png = testdata::Read("images/rgba8.png");
    png[20] ^= 0x01;
    ImageData damaged = ImageData::Decode(png, "damaged.png");
    EASYFORGE_EXPECT(!damaged);
    EASYFORGE_EXPECT(damaged.Error().find("CRC") != std::string::npos);
}

EASYFORGE_TEST(ImageDataPixels)
{
    ImageData image(4, 3);
    EASYFORGE_REQUIRE(image);
    EASYFORGE_EXPECT_EQUAL(image.Pixels.size(), std::size_t { 48 });
    EASYFORGE_EXPECT_EQUAL(image.Stride(), std::size_t { 16 });
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(1, 1), Color::Transparent);

    image.SetColorAt(2, 1, Color::Hex("#FF8000"));
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(2, 1), Color::Hex("#FF8000"));
    EASYFORGE_EXPECT_EQUAL(image.Pixels[1 * 16 + 2 * 4 + 1], std::uint8_t { 0x80 });

    image.SetColorAt(10, 10, Color::White);
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(-1, 0), Color::Transparent);

    ImageData wrong(2, 2, std::vector<std::uint8_t>(3));
    EASYFORGE_EXPECT(!wrong);
    EASYFORGE_EXPECT(!wrong.Error().empty());

    ImageData empty;
    EASYFORGE_EXPECT(!empty);
    EASYFORGE_EXPECT(empty.Error().empty());
}

EASYFORGE_TEST(ImageLoadInBackground)
{
    Pending<ImageData> pending = ImageData::LoadInBackground(testdata::Path("images/rgba8.png"));
    const ImageData& image = pending.Get();
    EASYFORGE_EXPECT(pending.Ready());
    EASYFORGE_REQUIRE(image);
    EASYFORGE_EXPECT_EQUAL(image.Width, 13);

    Pending<ImageData> nothing;
    EASYFORGE_EXPECT(nothing.Ready());
    EASYFORGE_EXPECT(!nothing.Get());
}
