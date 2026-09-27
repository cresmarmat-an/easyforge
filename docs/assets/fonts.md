# Fonts

`FontData` reads TrueType fonts: their measurements, their glyphs' outlines,
their kerning, and it draws glyphs into bitmaps with anti-aliasing. `graphics`
uses it to draw text; you can use it directly for anything else that needs
glyphs, such as a tool that bakes a font into an image.

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

FontData font = FontData::Load("fonts/Inter-Regular.ttf");
if (!font)
{
    Log(LogLevel::Error, font.Error());
    return;
}

int glyph = font.GlyphIndex(U'A');
GlyphBitmap bitmap = font.Rasterize(glyph, 32.0f);    // 32 pixels per em
Log("{} {}: 'A' is {} by {} pixels", font.FamilyName(), font.StyleName(), bitmap.Width, bitmap.Height);
```

## Units

Fonts measure everything in font units, with Y pointing up from the baseline. A
font has `Metrics().UnitsPerEm` units per em, usually 1000 or 2048. To get pixels,
divide by `UnitsPerEm` and multiply by the font size in pixels.

## The whole font

| Written | Result |
|---|---|
| `FamilyName()`, `StyleName()` | Such as "Inter" and "Bold Italic" |
| `Metrics()` | `UnitsPerEm`, `Ascender` (positive), `Descender` (negative), `LineGap`, and `LineHeight()` |
| `GlyphCount()` | How many glyphs the font has |
| `GlyphIndex(character)` | The glyph for a Unicode character, or 0 for the font's "missing character" glyph |

Fonts that set the USE_TYPO_METRICS flag get their line spacing from their OS/2
table, as they ask; others from their `hhea` table.

## One glyph

| Written | Result |
|---|---|
| `GlyphMetricsOf(glyph)` | `Advance`, `LeftBearing`, and the outline's box, `Minimum` and `Maximum` |
| `GlyphOutline(glyph)` | The outline as a list of contours |
| `Kerning(left, right)` | How much to adjust the space between two glyphs, usually negative |
| `Rasterize(glyph, pixelsPerEm)` | A `GlyphBitmap` of the glyph at that size |

### Laying out a line

```cpp
float size = 32.0f;                                   // pixels per em
float scale = size / font.Metrics().UnitsPerEm;
float penX = 0.0f;
int previous = -1;
for (char32_t character : U"AVATAR")
{
    int glyph = font.GlyphIndex(character);
    if (previous >= 0)
    {
        penX += font.Kerning(previous, glyph) * scale;
    }
    GlyphBitmap bitmap = font.Rasterize(glyph, size);
    // draw `bitmap` with its left edge at penX + bitmap.Left and its top edge
    // bitmap.Top pixels above the baseline
    penX += font.GlyphMetricsOf(glyph).Advance * scale;
    previous = glyph;
}
```

### Outlines

Each contour is a closed loop of `OutlinePoint`s, each a `Position` and whether it
is `OnCurve`. Contours always start with an on-curve point, never have two
off-curve points in a row, and end where they started. An off-curve point is the
control point of a quadratic curve between the on-curve points on either side.
Composite glyphs, such as Ä built from A and two dots, come back as one outline.

### Bitmaps

A `GlyphBitmap` has `Width` and `Height` in pixels, and `Coverage`: one value per
pixel, rows from top to bottom, 0 where the glyph is absent and 255 where it
covers the pixel entirely. Anti-aliasing comes from computing exactly how much of
each pixel the outline covers. `Left` is the distance from the pen position to the
bitmap's left edge, and `Top` the distance from the baseline up to its top edge.
A glyph with no outline, such as a space, gives an empty bitmap.

## Kerning

Kerning comes from the font's GPOS table when it has a `kern` feature there, as
modern fonts do; otherwise from its older `kern` table. Pair adjustments by glyph
and by glyph class are both read.

## Collections

A `.ttc` file holds several fonts. Choose one with the face index:
`FontData::Load("fonts/family.ttc", 1)`. Asking for a face the file does not have
gives an error that says how many it has.

## Limitations

- OpenType fonts with CFF outlines (most `.otf` files) give an error.
- Hinting instructions are ignored. Glyphs are drawn from their exact outlines,
  which suits high-resolution screens and scaled text; at very small sizes they can
  look softer than in programs that apply hinting.
- There is no text shaping beyond kerning pairs: no ligatures, no contextual
  forms, so scripts such as Arabic and Devanagari do not display correctly yet.
- Color and emoji glyphs (COLR, CBDT, sbix) and variable fonts' variations are
  not read.
- Only Unicode character maps of format 4 and 12 are read, which covers
  practically every font made since the 1990s.
- A component of a composite glyph placed by matching points, rather than by an
  offset, is placed at the origin.
- A glyph more than four ems wide or tall gives an empty bitmap; only a damaged
  font has one.
