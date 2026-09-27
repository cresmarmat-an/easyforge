#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/assets/Pending.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    namespace internal
    {
        struct FontFile;
    }

    // Measurements of the whole font, in font units. Divide by UnitsPerEm and
    // multiply by the size in pixels to get pixels.
    struct FontMetrics
    {
        float UnitsPerEm = 0.0f;

        // Distance from the baseline up to the top of tall letters, positive.
        float Ascender = 0.0f;

        // Distance from the baseline down to the bottom of low letters, negative.
        float Descender = 0.0f;

        // Extra space the font asks for between lines.
        float LineGap = 0.0f;

        // Ascender - Descender + LineGap: the distance from one baseline to the next.
        float LineHeight() const { return Ascender - Descender + LineGap; }
    };

    // Measurements of one glyph, in font units, with Y pointing up from the baseline.
    struct GlyphMetrics
    {
        // How far to move the pen after drawing the glyph.
        float Advance = 0.0f;

        // Distance from the pen position to the left edge of the glyph's outline.
        float LeftBearing = 0.0f;

        // The corners of the box around the outline. Both are zero for an empty glyph, such as a space.
        Vector2 Minimum;
        Vector2 Maximum;
    };

    // One point of a glyph's outline. An on-curve point lies on the outline; an
    // off-curve point is the control point of a quadratic curve between the
    // on-curve points on either side.
    struct OutlinePoint
    {
        Vector2 Position;
        bool OnCurve = true;
    };

    // A closed loop of an outline. It starts with an on-curve point, never has two
    // off-curve points in a row, and ends where it started.
    using Contour = std::vector<OutlinePoint>;

    // A glyph drawn at a size: how much of each pixel the glyph covers, from 0 to 255.
    struct GlyphBitmap
    {
        int Width = 0;
        int Height = 0;

        // Pixels from the pen position to the bitmap's left edge.
        int Left = 0;

        // Pixels from the baseline up to the bitmap's top edge.
        int Top = 0;

        // Width * Height values, rows from top to bottom.
        std::vector<std::uint8_t> Coverage;
    };

    // A TrueType font. Reads .ttf files, and .ttc collections, from which
    // `faceIndex` picks the font. OpenType fonts with CFF outlines (.otf files
    // that start with "OTTO") are not supported yet.
    //
    //     FontData font = FontData::Load("Inter.ttf");
    //     int glyph = font.GlyphIndex(U'A');
    //     GlyphBitmap bitmap = font.Rasterize(glyph, 32.0f);
    //
    // Copies share the font's data.
    class FontData
    {
    public:
        FontData() = default;

        static FontData Load(std::string_view path, int faceIndex = 0);
        static FontData Decode(std::vector<std::uint8_t> bytes, std::string_view name = "font", int faceIndex = 0);
        static Pending<FontData> LoadInBackground(std::string_view path, int faceIndex = 0);

        explicit operator bool() const { return File != nullptr; }
        const std::string& Error() const { return ErrorText; }

        // The family name, such as "Inter", and the style, such as "Bold Italic".
        const std::string& FamilyName() const;
        const std::string& StyleName() const;

        FontMetrics Metrics() const;
        int GlyphCount() const;

        // The glyph for a character, or 0, the font's "missing character" glyph.
        int GlyphIndex(char32_t character) const;

        GlyphMetrics GlyphMetricsOf(int glyph) const;
        std::vector<Contour> GlyphOutline(int glyph) const;

        // How much to add to the advance between two glyphs, usually negative, in
        // font units. Uses the font's GPOS kerning when it has it, otherwise its kern table.
        float Kerning(int leftGlyph, int rightGlyph) const;

        // Draws a glyph with anti-aliasing. `pixelsPerEm` is the font size in pixels.
        // A glyph more than four ems wide or tall gives an empty bitmap; only a
        // damaged font has one.
        GlyphBitmap Rasterize(int glyph, float pixelsPerEm) const;

    private:
        std::shared_ptr<const internal::FontFile> File;
        std::string ErrorText;
    };
}
