#include <easyforge/core/Testing.h>

#include <filesystem>

#include "TestData.h"

using namespace easyforge;

// Every decoder must survive files cut short or with bytes changed: it may fail
// or produce a wrong result, but never crash, hang, or read outside its data.
// Debug builds check every vector access, so a stray read fails loudly here.

namespace
{
    template <typename Decode>
    void Batter(const std::vector<std::uint8_t>& original, Decode decode, std::uint64_t seed)
    {
        // Every length up to 300 bytes, then larger steps.
        for (std::size_t length = 0; length < original.size(); length += length < 300 ? 1 : 97)
        {
            std::vector<std::uint8_t> cut(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(length));
            decode(cut);
        }

        Random random(seed);
        for (int attempt = 0; attempt < 300; ++attempt)
        {
            std::vector<std::uint8_t> changed = original;
            int changes = random.IntegerBetween(1, 8);
            for (int change = 0; change < changes && !changed.empty(); ++change)
            {
                std::size_t position = static_cast<std::size_t>(
                    random.IntegerBetween(0, static_cast<int>(changed.size()) - 1));
                changed[position] = static_cast<std::uint8_t>(random.Next());
            }
            decode(changed);
        }
    }

    std::vector<std::string> FilesIn(const std::string& folder, const std::string& extension)
    {
        std::vector<std::string> names;
        for (const auto& item : std::filesystem::directory_iterator(testdata::Path(folder)))
        {
            if (item.path().extension() == extension)
            {
                names.push_back(folder + "/" + item.path().filename().string());
            }
        }
        return names;
    }
}

EASYFORGE_TEST(DamagedImagesNeverCrash)
{
    std::uint64_t seed = 1;
    for (const char* extension : { ".png", ".jpg", ".bmp", ".tga", ".qoi" })
    {
        for (const std::string& name : FilesIn("images", extension))
        {
            Batter(
                testdata::Read(name), [&name](const std::vector<std::uint8_t>& bytes) {
                    ImageData image = ImageData::Decode(bytes, name);
                    if (image)
                    {
                        EASYFORGE_EXPECT(image.Pixels.size() == image.Stride() * static_cast<std::size_t>(image.Height));
                    }
                },
                seed++);
        }
    }
}

EASYFORGE_TEST(DamagedSoundsNeverCrash)
{
    std::uint64_t seed = 100;
    for (const char* extension : { ".wav", ".qoa" })
    {
        for (const std::string& name : FilesIn("sounds", extension))
        {
            Batter(
                testdata::Read(name), [&name](const std::vector<std::uint8_t>& bytes) {
                    SoundData sound = SoundData::Decode(bytes, name);
                    if (sound)
                    {
                        EASYFORGE_EXPECT(sound.Samples.size() == sound.FrameCount() * static_cast<std::size_t>(sound.ChannelCount));
                    }
                },
                seed++);
        }
    }
}

EASYFORGE_TEST(DamagedFontsNeverCrash)
{
    std::uint64_t seed = 200;
    for (const std::string& name : FilesIn("fonts", ".ttf"))
    {
        Batter(
            testdata::Read(name), [](const std::vector<std::uint8_t>& bytes) {
                FontData font = FontData::Decode(bytes, "damaged font");
                if (!font)
                {
                    return;
                }
                for (char32_t character : { U'A', U'O', U'Ä', U'X', U'\U0001F600' })
                {
                    int glyph = font.GlyphIndex(character);
                    font.GlyphMetricsOf(glyph);
                    font.Rasterize(glyph, 24.0f);
                    font.Kerning(glyph, 2);
                }
                for (int glyph = 0; glyph < Min(font.GlyphCount(), 16); ++glyph)
                {
                    font.GlyphOutline(glyph);
                }
            },
            seed++);
    }
}

EASYFORGE_TEST(DamagedModelsNeverCrash)
{
    std::uint64_t seed = 300;
    for (const std::string& name : FilesIn("models", ".obj"))
    {
        Batter(
            testdata::Read(name), [](const std::vector<std::uint8_t>& bytes) {
                std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                ModelData::DecodeObj(text, text);
            },
            seed++);
    }
}
