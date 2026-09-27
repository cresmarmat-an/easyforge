#include <easyforge/core/Testing.h>

#include "TestData.h"

using namespace easyforge;

namespace
{
    // Coverage at a pixel of a bitmap, or -1 outside it.
    int CoverageAt(const GlyphBitmap& bitmap, int x, int y)
    {
        if (x < 0 || y < 0 || x >= bitmap.Width || y >= bitmap.Height)
        {
            return -1;
        }
        return bitmap.Coverage[static_cast<std::size_t>(y * bitmap.Width + x)];
    }
}

EASYFORGE_TEST(FontNamesAndMetrics)
{
    FontData font = FontData::Load(testdata::Path("fonts/test_regular.ttf"));
    EASYFORGE_REQUIRE(font);
    EASYFORGE_EXPECT_EQUAL(font.FamilyName(), std::string("Easyforge Test"));
    EASYFORGE_EXPECT_EQUAL(font.StyleName(), std::string("Regular"));
    EASYFORGE_EXPECT_EQUAL(font.GlyphCount(), 7);

    FontMetrics metrics = font.Metrics();
    EASYFORGE_EXPECT_EQUAL(metrics.UnitsPerEm, 1000.0f);
    EASYFORGE_EXPECT_EQUAL(metrics.Ascender, 800.0f);
    EASYFORGE_EXPECT_EQUAL(metrics.Descender, -200.0f);
    EASYFORGE_EXPECT_EQUAL(metrics.LineGap, 90.0f);
    EASYFORGE_EXPECT_EQUAL(metrics.LineHeight(), 1090.0f);

    // This font asks for its OS/2 typographic metrics instead.
    FontData bold = FontData::Load(testdata::Path("fonts/test_bold.ttf"));
    EASYFORGE_REQUIRE(bold);
    EASYFORGE_EXPECT_EQUAL(bold.StyleName(), std::string("Bold"));
    EASYFORGE_EXPECT_EQUAL(bold.Metrics().Ascender, 750.0f);
    EASYFORGE_EXPECT_EQUAL(bold.Metrics().Descender, -250.0f);
    EASYFORGE_EXPECT_EQUAL(bold.Metrics().LineGap, 100.0f);
}

EASYFORGE_TEST(FontCharacterMaps)
{
    FontData regular = FontData::Load(testdata::Path("fonts/test_regular.ttf"));
    FontData bold = FontData::Load(testdata::Path("fonts/test_bold.ttf"));
    EASYFORGE_REQUIRE(regular && bold);

    for (const FontData* font : { &regular, &bold })
    {
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U' '), 1);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'A'), 2);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'O'), 3);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'.'), 4);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'\u00C4'), 5);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'W'), 6);
        // A run whose glyphs are not in order, which format 4 stores in its glyph array.
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'X'), 3);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'Y'), 2);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'Z'), 6);
        EASYFORGE_EXPECT_EQUAL(font->GlyphIndex(U'B'), 0);
    }

    // Characters beyond U+FFFF need the format 12 map.
    EASYFORGE_EXPECT_EQUAL(regular.GlyphIndex(U'\U0001F600'), 0);
    EASYFORGE_EXPECT_EQUAL(bold.GlyphIndex(U'\U0001F600'), 3);
}

EASYFORGE_TEST(FontGlyphMetrics)
{
    FontData font = FontData::Load(testdata::Path("fonts/test_regular.ttf"));
    EASYFORGE_REQUIRE(font);

    GlyphMetrics a = font.GlyphMetricsOf(2);
    EASYFORGE_EXPECT_EQUAL(a.Advance, 700.0f);
    EASYFORGE_EXPECT_EQUAL(a.LeftBearing, 0.0f);
    EASYFORGE_EXPECT_EQUAL(a.Minimum, (Vector2 { 0.0f, 0.0f }));
    EASYFORGE_EXPECT_EQUAL(a.Maximum, (Vector2 { 700.0f, 700.0f }));

    // Only the first five glyphs have their own advance; the rest share the last one.
    EASYFORGE_EXPECT_EQUAL(font.GlyphMetricsOf(5).Advance, 200.0f);
    EASYFORGE_EXPECT_EQUAL(font.GlyphMetricsOf(5).Maximum, (Vector2 { 700.0f, 900.0f }));

    GlyphMetrics space = font.GlyphMetricsOf(1);
    EASYFORGE_EXPECT_EQUAL(space.Advance, 250.0f);
    EASYFORGE_EXPECT_EQUAL(space.Maximum, Vector2 {});
    EASYFORGE_EXPECT(font.GlyphOutline(1).empty());

    EASYFORGE_EXPECT_EQUAL(font.GlyphMetricsOf(99).Advance, 0.0f);
}

EASYFORGE_TEST(FontOutlines)
{
    FontData font = FontData::Load(testdata::Path("fonts/test_regular.ttf"));
    EASYFORGE_REQUIRE(font);

    std::vector<Contour> a = font.GlyphOutline(2);
    EASYFORGE_REQUIRE(a.size() == 2);
    EASYFORGE_REQUIRE(a[0].size() == 4);
    EASYFORGE_EXPECT_EQUAL(a[0][1].Position, (Vector2 { 350.0f, 700.0f }));
    EASYFORGE_EXPECT(a[0][1].OnCurve);
    EASYFORGE_EXPECT_EQUAL(a[0].back().Position, a[0].front().Position);

    // Every point of the O is off the curve, so its contour starts halfway between
    // the last and first points and alternates from there.
    std::vector<Contour> o = font.GlyphOutline(3);
    EASYFORGE_REQUIRE(o.size() == 2);
    EASYFORGE_REQUIRE(o[0].size() == 9);
    EASYFORGE_EXPECT_EQUAL(o[0][0].Position, (Vector2 { 225.0f, 175.0f }));
    EASYFORGE_EXPECT(o[0][0].OnCurve);
    EASYFORGE_EXPECT(!o[0][1].OnCurve);
    EASYFORGE_EXPECT(o[0][2].OnCurve);
    EASYFORGE_EXPECT_EQUAL(o[0].back().Position, o[0].front().Position);

    // The composite: the A, a dot moved above it, and a half-size dot beside that.
    std::vector<Contour> dots = font.GlyphOutline(5);
    EASYFORGE_REQUIRE(dots.size() == 4);
    EASYFORGE_EXPECT_EQUAL(dots[2][0].Position, (Vector2 { 250.0f, 800.0f }));
    EASYFORGE_EXPECT_EQUAL(dots[3][0].Position, (Vector2 { 425.0f, 800.0f }));
    EASYFORGE_EXPECT_EQUAL(dots[3][2].Position, (Vector2 { 475.0f, 850.0f }));

    // Long coordinate steps.
    std::vector<Contour> wide = font.GlyphOutline(6);
    EASYFORGE_REQUIRE(wide.size() == 1);
    EASYFORGE_EXPECT_EQUAL(wide[0][2].Position, (Vector2 { 1100.0f, 600.0f }));
}

EASYFORGE_TEST(FontKerning)
{
    FontData regular = FontData::Load(testdata::Path("fonts/test_regular.ttf"));
    FontData bold = FontData::Load(testdata::Path("fonts/test_bold.ttf"));
    EASYFORGE_REQUIRE(regular && bold);

    // The kern table.
    EASYFORGE_EXPECT_EQUAL(regular.Kerning(2, 3), -50.0f);
    EASYFORGE_EXPECT_EQUAL(regular.Kerning(3, 2), -20.0f);
    EASYFORGE_EXPECT_EQUAL(regular.Kerning(2, 2), 0.0f);

    // GPOS: a glyph pair (with an X placement stored before the advance) and a
    // class pair inside an extension lookup.
    EASYFORGE_EXPECT_EQUAL(bold.Kerning(2, 3), -70.0f);
    EASYFORGE_EXPECT_EQUAL(bold.Kerning(3, 2), -30.0f);
    EASYFORGE_EXPECT_EQUAL(bold.Kerning(2, 2), 0.0f);
}

EASYFORGE_TEST(FontCollections)
{
    std::vector<std::uint8_t> bytes = testdata::Read("fonts/test_collection.ttc");
    FontData first = FontData::Decode(bytes, "collection", 0);
    FontData second = FontData::Decode(bytes, "collection", 1);
    EASYFORGE_REQUIRE(first && second);
    EASYFORGE_EXPECT_EQUAL(first.StyleName(), std::string("Regular"));
    EASYFORGE_EXPECT_EQUAL(second.StyleName(), std::string("Bold"));
    EASYFORGE_EXPECT_EQUAL(second.Kerning(2, 3), -70.0f);

    FontData third = FontData::Decode(bytes, "collection", 2);
    EASYFORGE_EXPECT(!third);
    EASYFORGE_EXPECT(third.Error().find("collection of 2") != std::string::npos);

    FontData wrongFace = FontData::Load(testdata::Path("fonts/test_regular.ttf"), 1);
    EASYFORGE_EXPECT(!wrongFace);
}

EASYFORGE_TEST(FontRasterizing)
{
    FontData font = FontData::Load(testdata::Path("fonts/test_regular.ttf"));
    EASYFORGE_REQUIRE(font);

    // A rectangle 1100 by 600 units at 10 pixels per em is exactly 11 by 6 pixels.
    GlyphBitmap wide = font.Rasterize(6, 10.0f);
    EASYFORGE_EXPECT_EQUAL(wide.Width, 11);
    EASYFORGE_EXPECT_EQUAL(wide.Height, 6);
    EASYFORGE_EXPECT_EQUAL(wide.Left, 0);
    EASYFORGE_EXPECT_EQUAL(wide.Top, 6);
    bool full = true;
    for (std::uint8_t value : wide.Coverage)
    {
        full = full && value == 255;
    }
    EASYFORGE_EXPECT(full);

    // The dot at 15 pixels per em spans 0.75 to 2.25 across and 0 to 1.5 up, so its
    // edge pixels are only partly covered.
    GlyphBitmap dot = font.Rasterize(4, 15.0f);
    EASYFORGE_REQUIRE(dot.Width == 3 && dot.Height == 2);
    EASYFORGE_EXPECT_EQUAL(dot.Top, 2);
    EASYFORGE_EXPECT_NEAR(static_cast<float>(CoverageAt(dot, 0, 0)), 32.0f, 1.0f);
    EASYFORGE_EXPECT_NEAR(static_cast<float>(CoverageAt(dot, 1, 0)), 128.0f, 1.0f);
    EASYFORGE_EXPECT_NEAR(static_cast<float>(CoverageAt(dot, 0, 1)), 64.0f, 1.0f);
    EASYFORGE_EXPECT_EQUAL(CoverageAt(dot, 1, 1), 255);

    // The A's hole stays empty while the rest of the letter is filled.
    GlyphBitmap a = font.Rasterize(2, 100.0f);
    EASYFORGE_REQUIRE(a.Width == 70 && a.Height == 70);
    EASYFORGE_EXPECT(CoverageAt(a, 35, 47) < 10);
    EASYFORGE_EXPECT_EQUAL(CoverageAt(a, 35, 10), 255);
    EASYFORGE_EXPECT_EQUAL(CoverageAt(a, 2, 5), 0);

    // Curves: the O's ring is filled and its middle is empty.
    GlyphBitmap o = font.Rasterize(3, 100.0f);
    EASYFORGE_EXPECT(CoverageAt(o, o.Width / 2, o.Height / 2) < 10);
    EASYFORGE_EXPECT_EQUAL(CoverageAt(o, 15, o.Height / 2), 255);

    EASYFORGE_EXPECT(font.Rasterize(1, 32.0f).Coverage.empty());
}

EASYFORGE_TEST(FontSystemFontsWhenPresent)
{
    // Real fonts exercise far more of the format than the test fonts. These run
    // only where the fonts exist, such as on Windows.
    for (const char* path : { "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf" })
    {
        if (!Files::Exists(path))
        {
            continue;
        }
        FontData font = FontData::Load(path);
        EASYFORGE_REQUIRE(font);
        EASYFORGE_EXPECT(!font.FamilyName().empty());
        int glyph = font.GlyphIndex(U'A');
        EASYFORGE_EXPECT(glyph != 0);
        GlyphBitmap bitmap = font.Rasterize(glyph, 48.0f);
        EASYFORGE_EXPECT(bitmap.Width > 20 && bitmap.Width < 48);
        EASYFORGE_EXPECT(bitmap.Height > 20 && bitmap.Height < 48);
        int brightest = 0;
        for (std::uint8_t value : bitmap.Coverage)
        {
            brightest = Max(brightest, static_cast<int>(value));
        }
        EASYFORGE_EXPECT_EQUAL(brightest, 255);
        EASYFORGE_EXPECT(font.Kerning(font.GlyphIndex(U'A'), font.GlyphIndex(U'V')) < 0.0f);
    }
}
