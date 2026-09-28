#pragma once

#include <string_view>

#include <easyforge/assets/FontData.h>
#include <easyforge/core/Scalar.h>
#include <easyforge/core/Vector.h>

namespace easyforge::internal
{
    // The next character of UTF-8 text, moving `position` past it. Bytes that are
    // not valid UTF-8 read as U+FFFD, the replacement character.
    char32_t NextCharacter(std::string_view text, std::size_t& position);

    // Places each glyph of the text: calls `visit(glyph, pen)` with the pen
    // position in pixels from the top left of the text, on the glyph's baseline.
    // Lines break at "\n"; a tab is four spaces wide. Returns the size of the
    // text: the widest line, and the height of all the lines.
    template <typename Visit>
    Vector2 LayOutText(const FontData& font, std::string_view text, float pixelsPerEm, Visit&& visit)
    {
        FontMetrics metrics = font.Metrics();
        if (metrics.UnitsPerEm <= 0.0f)
        {
            return {};
        }
        float scale = pixelsPerEm / metrics.UnitsPerEm;
        float lineHeight = metrics.LineHeight() * scale;
        float baseline = metrics.Ascender * scale;
        float x = 0.0f;
        float widest = 0.0f;
        int lines = 1;
        int previous = -1;
        int space = font.GlyphIndex(U' ');

        std::size_t position = 0;
        while (position < text.size())
        {
            char32_t character = NextCharacter(text, position);
            if (character == U'\n')
            {
                widest = Max(widest, x);
                x = 0.0f;
                baseline += lineHeight;
                ++lines;
                previous = -1;
                continue;
            }
            if (character == U'\r')
            {
                continue;
            }
            if (character == U'\t')
            {
                x += font.GlyphMetricsOf(space).Advance * scale * 4.0f;
                previous = -1;
                continue;
            }
            int glyph = font.GlyphIndex(character);
            if (previous >= 0)
            {
                x += font.Kerning(previous, glyph) * scale;
            }
            visit(glyph, Vector2 { x, baseline });
            x += font.GlyphMetricsOf(glyph).Advance * scale;
            previous = glyph;
        }
        widest = Max(widest, x);
        return { widest, lineHeight * static_cast<float>(lines) };
    }
}
