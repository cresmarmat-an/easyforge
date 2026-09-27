#include <easyforge/assets/FontData.h>

#include <algorithm>
#include <bit>
#include <format>
#include <set>

#include <easyforge/assets/Files.h>

#include "../ByteReader.h"
#include "FontFile.h"

namespace easyforge
{
    namespace internal
    {
        namespace
        {
            constexpr std::uint32_t Tag(const char (&text)[5])
            {
                return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[0])) << 24) |
                       (static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[1])) << 16) |
                       (static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[2])) << 8) |
                       static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[3]));
            }

            // Reads numbers at absolute offsets in the font, as zero past the end.
            struct Bytes
            {
                const std::vector<std::uint8_t>& Data;

                std::uint8_t U8(std::size_t offset) const { return offset < Data.size() ? Data[offset] : 0; }
                std::uint16_t U16(std::size_t offset) const
                {
                    return static_cast<std::uint16_t>((U8(offset) << 8) | U8(offset + 1));
                }
                std::int16_t I16(std::size_t offset) const { return static_cast<std::int16_t>(U16(offset)); }
                std::uint32_t U32(std::size_t offset) const
                {
                    return (static_cast<std::uint32_t>(U16(offset)) << 16) | U16(offset + 2);
                }
            };

            std::string DecodeUtf16BigEndian(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t length)
            {
                std::string text;
                Bytes bytes { data };
                for (std::size_t index = 0; index + 1 < length; index += 2)
                {
                    char32_t unit = bytes.U16(offset + index);
                    if (unit >= 0xD800 && unit < 0xDC00 && index + 3 < length)
                    {
                        char32_t low = bytes.U16(offset + index + 2);
                        unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                        index += 2;
                    }
                    if (unit < 0x80)
                    {
                        text += static_cast<char>(unit);
                    }
                    else if (unit < 0x800)
                    {
                        text += static_cast<char>(0xC0 | (unit >> 6));
                        text += static_cast<char>(0x80 | (unit & 0x3F));
                    }
                    else if (unit < 0x10000)
                    {
                        text += static_cast<char>(0xE0 | (unit >> 12));
                        text += static_cast<char>(0x80 | ((unit >> 6) & 0x3F));
                        text += static_cast<char>(0x80 | (unit & 0x3F));
                    }
                    else
                    {
                        text += static_cast<char>(0xF0 | (unit >> 18));
                        text += static_cast<char>(0x80 | ((unit >> 12) & 0x3F));
                        text += static_cast<char>(0x80 | ((unit >> 6) & 0x3F));
                        text += static_cast<char>(0x80 | (unit & 0x3F));
                    }
                }
                return text;
            }

            void ReadNames(FontFile& font)
            {
                if (!font.Names.Present())
                {
                    return;
                }
                Bytes bytes { font.Bytes };
                std::uint32_t base = font.Names.Offset;
                int count = bytes.U16(base + 2);
                std::uint32_t strings = base + bytes.U16(base + 4);

                // Typographic names (16, 17) are preferred over the basic ones (1, 2),
                // Windows English over any other.
                int familyScore = -1;
                int styleScore = -1;
                for (int index = 0; index < count; ++index)
                {
                    std::uint32_t record = base + 6 + static_cast<std::uint32_t>(index) * 12;
                    int platform = bytes.U16(record);
                    int encoding = bytes.U16(record + 2);
                    int language = bytes.U16(record + 4);
                    int nameIdentifier = bytes.U16(record + 6);
                    std::size_t length = bytes.U16(record + 8);
                    std::size_t offset = strings + bytes.U16(record + 10);
                    if (offset + length > font.Bytes.size())
                    {
                        continue;
                    }

                    std::string text;
                    int platformScore = 0;
                    if (platform == 3 && (encoding == 1 || encoding == 10))
                    {
                        text = DecodeUtf16BigEndian(font.Bytes, offset, length);
                        platformScore = language == 0x409 ? 4 : 2;
                    }
                    else if (platform == 0)
                    {
                        text = DecodeUtf16BigEndian(font.Bytes, offset, length);
                        platformScore = 3;
                    }
                    else if (platform == 1 && encoding == 0)
                    {
                        text.assign(reinterpret_cast<const char*>(font.Bytes.data() + offset), length);
                        platformScore = 1;
                    }
                    else
                    {
                        continue;
                    }

                    if (nameIdentifier == 1 || nameIdentifier == 16)
                    {
                        int score = platformScore + (nameIdentifier == 16 ? 10 : 0);
                        if (score > familyScore)
                        {
                            familyScore = score;
                            font.Family = text;
                        }
                    }
                    else if (nameIdentifier == 2 || nameIdentifier == 17)
                    {
                        int score = platformScore + (nameIdentifier == 17 ? 10 : 0);
                        if (score > styleScore)
                        {
                            styleScore = score;
                            font.Style = text;
                        }
                    }
                }
            }

            // Picks the most complete Unicode character map the font has.
            Result<> ChooseCharacterMap(FontFile& font)
            {
                Bytes bytes { font.Bytes };
                std::uint32_t base = font.CharacterMap.Offset;
                int count = bytes.U16(base + 2);
                int bestScore = 0;
                for (int index = 0; index < count; ++index)
                {
                    std::uint32_t record = base + 4 + static_cast<std::uint32_t>(index) * 8;
                    int platform = bytes.U16(record);
                    int encoding = bytes.U16(record + 2);
                    std::uint32_t offset = base + bytes.U32(record + 4);
                    if (offset + 4 > font.Bytes.size())
                    {
                        continue;
                    }
                    int format = bytes.U16(offset);
                    bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
                    int score = 0;
                    if (unicode && format == 12)
                    {
                        score = 3;
                    }
                    else if (unicode && format == 4)
                    {
                        score = 2;
                    }
                    else if (platform == 3 && encoding == 0 && format == 4)
                    {
                        // Symbol fonts map their characters into U+F000 and up.
                        score = 1;
                    }
                    if (score > bestScore)
                    {
                        bestScore = score;
                        font.CharacterMapOffset = offset;
                        font.CharacterMapFormat = format;
                    }
                }
                if (bestScore == 0)
                {
                    return Failure("it has no Unicode character map this reader understands (formats 4 and 12)");
                }
                return {};
            }

            // Finds the lookups of the GPOS "kern" feature and their pair-adjustment subtables.
            void ReadPositioning(FontFile& font)
            {
                if (!font.Positioning.Present())
                {
                    return;
                }
                Bytes bytes { font.Bytes };
                std::uint32_t base = font.Positioning.Offset;
                std::uint32_t features = base + bytes.U16(base + 6);
                std::uint32_t lookups = base + bytes.U16(base + 8);

                std::set<int> wanted;
                int featureCount = bytes.U16(features);
                for (int index = 0; index < featureCount; ++index)
                {
                    std::uint32_t record = features + 2 + static_cast<std::uint32_t>(index) * 6;
                    if (bytes.U32(record) != Tag("kern"))
                    {
                        continue;
                    }
                    std::uint32_t feature = features + bytes.U16(record + 4);
                    int lookupCount = bytes.U16(feature + 2);
                    for (int lookup = 0; lookup < lookupCount; ++lookup)
                    {
                        wanted.insert(bytes.U16(feature + 4 + static_cast<std::uint32_t>(lookup) * 2));
                    }
                }

                int lookupCount = bytes.U16(lookups);
                for (int index : wanted)
                {
                    if (index >= lookupCount)
                    {
                        continue;
                    }
                    std::uint32_t lookup = lookups + bytes.U16(lookups + 2 + static_cast<std::uint32_t>(index) * 2);
                    int type = bytes.U16(lookup);
                    int subtableCount = bytes.U16(lookup + 4);
                    std::vector<std::uint32_t> subtables;
                    for (int subtable = 0; subtable < subtableCount; ++subtable)
                    {
                        std::uint32_t offset = lookup + bytes.U16(lookup + 6 + static_cast<std::uint32_t>(subtable) * 2);
                        if (type == 9)
                        {
                            // An extension subtable points further to the real one.
                            if (bytes.U16(offset + 2) != 2)
                            {
                                continue;
                            }
                            offset += bytes.U32(offset + 4);
                        }
                        else if (type != 2)
                        {
                            continue;
                        }
                        subtables.push_back(offset);
                    }
                    if (!subtables.empty())
                    {
                        font.KerningLookups.push_back(std::move(subtables));
                    }
                }
            }

            Result<std::shared_ptr<FontFile>> Parse(std::vector<std::uint8_t> data, int faceIndex)
            {
                auto font = std::make_shared<FontFile>();
                font->Bytes = std::move(data);
                Bytes bytes { font->Bytes };
                if (font->Bytes.size() < 12)
                {
                    return Failure("it is too short to be a font");
                }

                std::uint32_t start = 0;
                if (bytes.U32(0) == Tag("ttcf"))
                {
                    std::uint32_t count = bytes.U32(8);
                    if (faceIndex < 0 || static_cast<std::uint32_t>(faceIndex) >= count)
                    {
                        return Failure(std::format("it is a collection of {} fonts, so face {} does not exist", count,
                            faceIndex));
                    }
                    start = bytes.U32(12 + static_cast<std::uint32_t>(faceIndex) * 4);
                }
                else if (faceIndex != 0)
                {
                    return Failure(std::format("it holds one font, so face {} does not exist", faceIndex));
                }

                std::uint32_t version = bytes.U32(start);
                if (version == Tag("OTTO"))
                {
                    return Failure("it is an OpenType font with CFF outlines, which easyforge does not read yet");
                }
                if (version != 0x00010000 && version != Tag("true"))
                {
                    return Failure("it is not a TrueType font");
                }

                int tableCount = bytes.U16(start + 4);
                for (int index = 0; index < tableCount; ++index)
                {
                    std::uint32_t record = start + 12 + static_cast<std::uint32_t>(index) * 16;
                    std::uint32_t tag = bytes.U32(record);
                    FontTable table { bytes.U32(record + 8), bytes.U32(record + 12) };
                    if (static_cast<std::uint64_t>(table.Offset) + table.Length > font->Bytes.size())
                    {
                        continue;
                    }
                    if (tag == Tag("head"))
                        font->Head = table;
                    else if (tag == Tag("hhea"))
                        font->HorizontalHeader = table;
                    else if (tag == Tag("maxp"))
                        font->MaximumProfile = table;
                    else if (tag == Tag("hmtx"))
                        font->HorizontalMetrics = table;
                    else if (tag == Tag("loca"))
                        font->Locations = table;
                    else if (tag == Tag("glyf"))
                        font->Glyphs = table;
                    else if (tag == Tag("cmap"))
                        font->CharacterMap = table;
                    else if (tag == Tag("kern"))
                        font->Kerning = table;
                    else if (tag == Tag("GPOS"))
                        font->Positioning = table;
                    else if (tag == Tag("name"))
                        font->Names = table;
                    else if (tag == Tag("OS/2"))
                        font->WindowsMetrics = table;
                }

                if (!font->Head.Present() || !font->HorizontalHeader.Present() || !font->MaximumProfile.Present() ||
                    !font->HorizontalMetrics.Present() || !font->Locations.Present() || !font->Glyphs.Present() ||
                    !font->CharacterMap.Present())
                {
                    return Failure("it is missing a table every TrueType font needs");
                }

                font->Metrics.UnitsPerEm = bytes.U16(font->Head.Offset + 18);
                font->LongLocations = bytes.I16(font->Head.Offset + 50) != 0;
                font->GlyphCount = bytes.U16(font->MaximumProfile.Offset + 4);
                font->HorizontalMetricCount = bytes.U16(font->HorizontalHeader.Offset + 34);
                if (font->Metrics.UnitsPerEm < 16 || font->GlyphCount == 0 || font->HorizontalMetricCount == 0)
                {
                    return Failure("its header values are not valid");
                }

                std::uint32_t header = font->HorizontalHeader.Offset;
                font->Metrics.Ascender = bytes.I16(header + 4);
                font->Metrics.Descender = bytes.I16(header + 6);
                font->Metrics.LineGap = bytes.I16(header + 8);

                // Fonts that set USE_TYPO_METRICS want their OS/2 line spacing used instead.
                if (font->WindowsMetrics.Length >= 78 && (bytes.U16(font->WindowsMetrics.Offset + 62) & 0x80) != 0)
                {
                    std::uint32_t metrics = font->WindowsMetrics.Offset;
                    font->Metrics.Ascender = bytes.I16(metrics + 68);
                    font->Metrics.Descender = bytes.I16(metrics + 70);
                    font->Metrics.LineGap = bytes.I16(metrics + 72);
                }

                Result<> characterMap = ChooseCharacterMap(*font);
                if (!characterMap)
                {
                    return Failure(characterMap.Error());
                }
                ReadNames(*font);
                ReadPositioning(*font);
                return font;
            }

            // Where glyph `glyph`'s outline data is inside the glyf table; length 0 for none.
            FontTable GlyphLocation(const FontFile& font, int glyph)
            {
                if (glyph < 0 || glyph >= font.GlyphCount)
                {
                    return {};
                }
                Bytes bytes { font.Bytes };
                std::uint32_t start = 0;
                std::uint32_t end = 0;
                std::uint32_t base = font.Locations.Offset;
                std::uint32_t index = static_cast<std::uint32_t>(glyph);
                if (font.LongLocations)
                {
                    start = bytes.U32(base + index * 4);
                    end = bytes.U32(base + index * 4 + 4);
                }
                else
                {
                    start = static_cast<std::uint32_t>(bytes.U16(base + index * 2)) * 2;
                    end = static_cast<std::uint32_t>(bytes.U16(base + index * 2 + 2)) * 2;
                }
                if (end <= start || end > font.Glyphs.Length)
                {
                    return {};
                }
                return { font.Glyphs.Offset + start, end - start };
            }

            // Turns raw points into a contour in the documented shape: starts on the
            // curve, and has an on-curve point between any two off-curve points.
            Contour Tidy(const std::vector<OutlinePoint>& raw)
            {
                Contour contour;
                if (raw.empty())
                {
                    return contour;
                }

                std::size_t count = raw.size();
                std::size_t first = count;
                for (std::size_t index = 0; index < count; ++index)
                {
                    if (raw[index].OnCurve)
                    {
                        first = index;
                        break;
                    }
                }

                OutlinePoint start;
                if (first == count)
                {
                    // Every point is off the curve: start halfway between the last and the first.
                    start = { (raw[count - 1].Position + raw[0].Position) * 0.5f, true };
                    first = 0;
                    contour.push_back(start);
                    for (std::size_t step = 0; step < count; ++step)
                    {
                        const OutlinePoint& point = raw[step];
                        const OutlinePoint& next = raw[(step + 1) % count];
                        contour.push_back(point);
                        contour.push_back({ (point.Position + next.Position) * 0.5f, true });
                    }
                    return contour;
                }

                start = raw[first];
                contour.push_back(start);
                for (std::size_t step = 1; step <= count; ++step)
                {
                    const OutlinePoint& point = raw[(first + step) % count];
                    if (!point.OnCurve && !contour.back().OnCurve)
                    {
                        contour.push_back({ (contour.back().Position + point.Position) * 0.5f, true });
                    }
                    contour.push_back(point);
                }
                return contour;
            }

            // Limits on one outline. Real glyphs, even complex CJK ones, have a few
            // thousand points; a damaged font can claim tens of thousands, or a composite
            // that includes itself, which would take far too long to draw.
            struct OutlineBudget
            {
                int Glyphs = 256;
                int Points = 65536;
            };

            constexpr int MaximumGlyphPoints = 16384;

            void AppendOutline(
                const FontFile& font, int glyph, int depth, OutlineBudget& budget, std::vector<Contour>& contours)
            {
                FontTable location = GlyphLocation(font, glyph);
                if (location.Length < 10 || depth > 8 || budget.Glyphs <= 0)
                {
                    return;
                }
                --budget.Glyphs;
                Bytes bytes { font.Bytes };
                std::uint32_t offset = location.Offset;
                std::uint32_t end = location.Offset + location.Length;
                int contourCount = bytes.I16(offset);
                offset += 10;

                if (contourCount >= 0)
                {
                    std::vector<int> endPoints(static_cast<std::size_t>(contourCount));
                    for (int& point : endPoints)
                    {
                        point = bytes.U16(offset);
                        offset += 2;
                    }
                    if (contourCount == 0)
                    {
                        return;
                    }
                    std::size_t pointCount = static_cast<std::size_t>(endPoints.back()) + 1;
                    if (pointCount > static_cast<std::size_t>(MaximumGlyphPoints) ||
                        pointCount > static_cast<std::size_t>(budget.Points))
                    {
                        return;
                    }
                    budget.Points -= static_cast<int>(pointCount);
                    offset += 2 + bytes.U16(offset);

                    std::vector<std::uint8_t> flags;
                    flags.reserve(pointCount);
                    while (flags.size() < pointCount && offset < end)
                    {
                        std::uint8_t flag = bytes.U8(offset++);
                        flags.push_back(flag);
                        if ((flag & 8) != 0)
                        {
                            int repeat = bytes.U8(offset++);
                            for (int count = 0; count < repeat && flags.size() < pointCount; ++count)
                            {
                                flags.push_back(flag);
                            }
                        }
                    }
                    if (flags.size() < pointCount)
                    {
                        return;
                    }

                    std::vector<OutlinePoint> points(pointCount);
                    int value = 0;
                    for (std::size_t index = 0; index < pointCount; ++index)
                    {
                        std::uint8_t flag = flags[index];
                        if ((flag & 2) != 0)
                        {
                            int delta = bytes.U8(offset++);
                            value += (flag & 16) != 0 ? delta : -delta;
                        }
                        else if ((flag & 16) == 0)
                        {
                            value += bytes.I16(offset);
                            offset += 2;
                        }
                        points[index].Position.X = static_cast<float>(value);
                        points[index].OnCurve = (flag & 1) != 0;
                    }
                    value = 0;
                    for (std::size_t index = 0; index < pointCount; ++index)
                    {
                        std::uint8_t flag = flags[index];
                        if ((flag & 4) != 0)
                        {
                            int delta = bytes.U8(offset++);
                            value += (flag & 32) != 0 ? delta : -delta;
                        }
                        else if ((flag & 32) == 0)
                        {
                            value += bytes.I16(offset);
                            offset += 2;
                        }
                        points[index].Position.Y = static_cast<float>(value);
                    }

                    std::size_t begin = 0;
                    for (int endPoint : endPoints)
                    {
                        std::size_t last = static_cast<std::size_t>(endPoint);
                        if (last < begin || last >= pointCount)
                        {
                            return;
                        }
                        std::vector<OutlinePoint> raw(points.begin() + static_cast<std::ptrdiff_t>(begin),
                            points.begin() + static_cast<std::ptrdiff_t>(last + 1));
                        if (raw.size() >= 2)
                        {
                            contours.push_back(Tidy(raw));
                        }
                        begin = last + 1;
                    }
                    return;
                }

                // A composite glyph: other glyphs, each moved and possibly scaled.
                constexpr int ArgumentsAreWords = 0x1;
                constexpr int ArgumentsAreOffsets = 0x2;
                constexpr int HasScale = 0x8;
                constexpr int MoreComponents = 0x20;
                constexpr int HasScaleXY = 0x40;
                constexpr int HasTwoByTwo = 0x80;
                constexpr int ScaledOffset = 0x800;

                int flags = MoreComponents;
                while ((flags & MoreComponents) != 0 && offset + 4 <= end)
                {
                    flags = bytes.U16(offset);
                    int component = bytes.U16(offset + 2);
                    offset += 4;

                    float first = 0.0f;
                    float second = 0.0f;
                    if ((flags & ArgumentsAreWords) != 0)
                    {
                        first = bytes.I16(offset);
                        second = bytes.I16(offset + 2);
                        offset += 4;
                    }
                    else
                    {
                        first = static_cast<float>(static_cast<std::int8_t>(bytes.U8(offset)));
                        second = static_cast<float>(static_cast<std::int8_t>(bytes.U8(offset + 1)));
                        offset += 2;
                    }

                    auto fixed = [&bytes](std::uint32_t at) { return static_cast<float>(bytes.I16(at)) / 16384.0f; };
                    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f;
                    if ((flags & HasScale) != 0)
                    {
                        a = d = fixed(offset);
                        offset += 2;
                    }
                    else if ((flags & HasScaleXY) != 0)
                    {
                        a = fixed(offset);
                        d = fixed(offset + 2);
                        offset += 4;
                    }
                    else if ((flags & HasTwoByTwo) != 0)
                    {
                        a = fixed(offset);
                        b = fixed(offset + 2);
                        c = fixed(offset + 4);
                        d = fixed(offset + 6);
                        offset += 8;
                    }

                    // Placing a component by matching points is rare and not supported; it is placed at the origin.
                    Vector2 shift;
                    if ((flags & ArgumentsAreOffsets) != 0)
                    {
                        shift = { first, second };
                        if ((flags & ScaledOffset) != 0)
                        {
                            shift = { a * first + c * second, b * first + d * second };
                        }
                    }

                    std::vector<Contour> parts;
                    AppendOutline(font, component, depth + 1, budget, parts);
                    for (Contour& contour : parts)
                    {
                        for (OutlinePoint& point : contour)
                        {
                            Vector2 position = point.Position;
                            point.Position = { a * position.X + c * position.Y + shift.X,
                                b * position.X + d * position.Y + shift.Y };
                        }
                        contours.push_back(std::move(contour));
                    }
                }
            }

            int CoverageIndex(const Bytes& bytes, std::uint32_t coverage, int glyph)
            {
                int format = bytes.U16(coverage);
                if (format == 1)
                {
                    int count = bytes.U16(coverage + 2);
                    int low = 0;
                    int high = count - 1;
                    while (low <= high)
                    {
                        int middle = low + (high - low) / 2;
                        int value = bytes.U16(coverage + 4 + static_cast<std::uint32_t>(middle) * 2);
                        if (value == glyph)
                        {
                            return middle;
                        }
                        if (value < glyph)
                            low = middle + 1;
                        else
                            high = middle - 1;
                    }
                }
                else if (format == 2)
                {
                    int count = bytes.U16(coverage + 2);
                    for (int index = 0; index < count; ++index)
                    {
                        std::uint32_t range = coverage + 4 + static_cast<std::uint32_t>(index) * 6;
                        int first = bytes.U16(range);
                        int last = bytes.U16(range + 2);
                        if (glyph >= first && glyph <= last)
                        {
                            return bytes.U16(range + 4) + glyph - first;
                        }
                    }
                }
                return -1;
            }

            int GlyphClass(const Bytes& bytes, std::uint32_t classes, int glyph)
            {
                int format = bytes.U16(classes);
                if (format == 1)
                {
                    int first = bytes.U16(classes + 2);
                    int count = bytes.U16(classes + 4);
                    if (glyph >= first && glyph < first + count)
                    {
                        return bytes.U16(classes + 6 + static_cast<std::uint32_t>(glyph - first) * 2);
                    }
                }
                else if (format == 2)
                {
                    int count = bytes.U16(classes + 2);
                    for (int index = 0; index < count; ++index)
                    {
                        std::uint32_t range = classes + 4 + static_cast<std::uint32_t>(index) * 6;
                        if (glyph >= bytes.U16(range) && glyph <= bytes.U16(range + 2))
                        {
                            return bytes.U16(range + 4);
                        }
                    }
                }
                return 0;
            }

            // The first value record's horizontal advance adjustment, or false when this
            // subtable does not cover the pair.
            bool PairAdjustment(const Bytes& bytes, std::uint32_t subtable, int left, int right, float& adjustment)
            {
                int format = bytes.U16(subtable);
                int coverage = CoverageIndex(bytes, subtable + bytes.U16(subtable + 2), left);
                if (coverage < 0)
                {
                    return false;
                }
                int firstFormat = bytes.U16(subtable + 4);
                int secondFormat = bytes.U16(subtable + 6);
                std::uint32_t firstSize = static_cast<std::uint32_t>(std::popcount(static_cast<unsigned>(firstFormat))) * 2;
                std::uint32_t secondSize = static_cast<std::uint32_t>(std::popcount(static_cast<unsigned>(secondFormat))) * 2;
                if ((firstFormat & 0x4) == 0)
                {
                    return false;
                }
                std::uint32_t advanceOffset =
                    static_cast<std::uint32_t>(std::popcount(static_cast<unsigned>(firstFormat & 0x3))) * 2;

                if (format == 1)
                {
                    int setCount = bytes.U16(subtable + 8);
                    if (coverage >= setCount)
                    {
                        return false;
                    }
                    std::uint32_t pairSet = subtable + bytes.U16(subtable + 10 + static_cast<std::uint32_t>(coverage) * 2);
                    int pairCount = bytes.U16(pairSet);
                    std::uint32_t recordSize = 2 + firstSize + secondSize;
                    for (int index = 0; index < pairCount; ++index)
                    {
                        std::uint32_t record = pairSet + 2 + static_cast<std::uint32_t>(index) * recordSize;
                        if (bytes.U16(record) == right)
                        {
                            adjustment = bytes.I16(record + 2 + advanceOffset);
                            return true;
                        }
                    }
                    return false;
                }
                if (format == 2)
                {
                    int firstClass = GlyphClass(bytes, subtable + bytes.U16(subtable + 8), left);
                    int secondClass = GlyphClass(bytes, subtable + bytes.U16(subtable + 10), right);
                    int firstClassCount = bytes.U16(subtable + 12);
                    int secondClassCount = bytes.U16(subtable + 14);
                    if (firstClass >= firstClassCount || secondClass >= secondClassCount)
                    {
                        return false;
                    }
                    std::uint32_t recordSize = firstSize + secondSize;
                    std::uint32_t record = subtable + 16 +
                                           (static_cast<std::uint32_t>(firstClass) * static_cast<std::uint32_t>(secondClassCount) +
                                               static_cast<std::uint32_t>(secondClass)) * recordSize;
                    adjustment = bytes.I16(record + advanceOffset);
                    return true;
                }
                return false;
            }
        }
    }

    FontData FontData::Load(std::string_view path, int faceIndex)
    {
        Result<std::vector<std::uint8_t>> bytes = Files::Read(path);
        if (!bytes)
        {
            FontData failed;
            failed.ErrorText = bytes.Error();
            return failed;
        }
        return Decode(std::move(bytes).Get(), path, faceIndex);
    }

    FontData FontData::Decode(std::vector<std::uint8_t> bytes, std::string_view name, int faceIndex)
    {
        FontData font;
        Result<std::shared_ptr<internal::FontFile>> parsed = internal::Parse(std::move(bytes), faceIndex);
        if (!parsed)
        {
            font.ErrorText = std::format("{}: {}", name, parsed.Error());
            return font;
        }
        font.File = std::move(parsed).Get();
        return font;
    }

    Pending<FontData> FontData::LoadInBackground(std::string_view path, int faceIndex)
    {
        return Pending<FontData>(
            Jobs::Shared().Run([path = std::string(path), faceIndex] { return Load(path, faceIndex); }));
    }

    const std::string& FontData::FamilyName() const
    {
        static const std::string empty;
        return File ? File->Family : empty;
    }

    const std::string& FontData::StyleName() const
    {
        static const std::string empty;
        return File ? File->Style : empty;
    }

    FontMetrics FontData::Metrics() const
    {
        return File ? File->Metrics : FontMetrics {};
    }

    int FontData::GlyphCount() const
    {
        return File ? File->GlyphCount : 0;
    }

    int FontData::GlyphIndex(char32_t character) const
    {
        if (!File)
        {
            return 0;
        }
        internal::Bytes bytes { File->Bytes };
        std::uint32_t map = File->CharacterMapOffset;
        std::uint32_t code = static_cast<std::uint32_t>(character);
        int glyph = 0;

        if (File->CharacterMapFormat == 12)
        {
            std::uint32_t groups = bytes.U32(map + 12);
            std::uint32_t low = 0;
            std::uint32_t high = groups;
            while (low < high)
            {
                std::uint32_t middle = low + (high - low) / 2;
                std::uint32_t group = map + 16 + middle * 12;
                std::uint32_t first = bytes.U32(group);
                std::uint32_t last = bytes.U32(group + 4);
                if (code < first)
                {
                    high = middle;
                }
                else if (code > last)
                {
                    low = middle + 1;
                }
                else
                {
                    glyph = static_cast<int>(bytes.U32(group + 8) + (code - first));
                    break;
                }
            }
        }
        else if (File->CharacterMapFormat == 4 && code <= 0xFFFF)
        {
            std::uint32_t segments = bytes.U16(map + 6) / 2;
            std::uint32_t ends = map + 14;
            std::uint32_t starts = ends + segments * 2 + 2;
            std::uint32_t deltas = starts + segments * 2;
            std::uint32_t ranges = deltas + segments * 2;

            std::uint32_t low = 0;
            std::uint32_t high = segments;
            while (low < high)
            {
                std::uint32_t middle = low + (high - low) / 2;
                if (bytes.U16(ends + middle * 2) < code)
                    low = middle + 1;
                else
                    high = middle;
            }
            if (low < segments)
            {
                std::uint32_t segment = low;
                std::uint16_t start = bytes.U16(starts + segment * 2);
                if (code >= start)
                {
                    std::uint16_t delta = bytes.U16(deltas + segment * 2);
                    std::uint32_t rangeAt = ranges + segment * 2;
                    std::uint16_t range = bytes.U16(rangeAt);
                    if (range == 0)
                    {
                        glyph = static_cast<std::uint16_t>(code + delta);
                    }
                    else
                    {
                        std::uint16_t value = bytes.U16(rangeAt + range + (code - start) * 2);
                        glyph = value == 0 ? 0 : static_cast<std::uint16_t>(value + delta);
                    }
                }
            }
        }
        return glyph < File->GlyphCount ? glyph : 0;
    }

    GlyphMetrics FontData::GlyphMetricsOf(int glyph) const
    {
        GlyphMetrics metrics;
        if (!File || glyph < 0 || glyph >= File->GlyphCount)
        {
            return metrics;
        }
        internal::Bytes bytes { File->Bytes };
        std::uint32_t base = File->HorizontalMetrics.Offset;
        std::uint32_t count = static_cast<std::uint32_t>(File->HorizontalMetricCount);
        std::uint32_t index = static_cast<std::uint32_t>(glyph);
        if (index < count)
        {
            metrics.Advance = bytes.U16(base + index * 4);
            metrics.LeftBearing = bytes.I16(base + index * 4 + 2);
        }
        else
        {
            metrics.Advance = bytes.U16(base + (count - 1) * 4);
            metrics.LeftBearing = bytes.I16(base + count * 4 + (index - count) * 2);
        }

        internal::FontTable location = internal::GlyphLocation(*File, glyph);
        if (location.Length >= 10)
        {
            metrics.Minimum = { static_cast<float>(bytes.I16(location.Offset + 2)),
                static_cast<float>(bytes.I16(location.Offset + 4)) };
            metrics.Maximum = { static_cast<float>(bytes.I16(location.Offset + 6)),
                static_cast<float>(bytes.I16(location.Offset + 8)) };
        }
        return metrics;
    }

    std::vector<Contour> FontData::GlyphOutline(int glyph) const
    {
        std::vector<Contour> contours;
        if (File)
        {
            internal::OutlineBudget budget;
            internal::AppendOutline(*File, glyph, 0, budget, contours);
        }
        return contours;
    }

    float FontData::Kerning(int leftGlyph, int rightGlyph) const
    {
        if (!File)
        {
            return 0.0f;
        }
        internal::Bytes bytes { File->Bytes };

        if (!File->KerningLookups.empty())
        {
            float total = 0.0f;
            for (const auto& lookup : File->KerningLookups)
            {
                for (std::uint32_t subtable : lookup)
                {
                    float adjustment = 0.0f;
                    if (internal::PairAdjustment(bytes, subtable, leftGlyph, rightGlyph, adjustment))
                    {
                        total += adjustment;
                        break;
                    }
                }
            }
            return total;
        }

        if (File->Kerning.Present() && bytes.U16(File->Kerning.Offset) == 0)
        {
            std::uint32_t base = File->Kerning.Offset;
            int tables = bytes.U16(base + 2);
            std::uint32_t subtable = base + 4;
            float total = 0.0f;
            std::uint32_t pair = (static_cast<std::uint32_t>(leftGlyph) << 16) | static_cast<std::uint32_t>(rightGlyph);
            for (int table = 0; table < tables; ++table)
            {
                int length = bytes.U16(subtable + 2);
                int coverage = bytes.U16(subtable + 4);
                // Format 0, horizontal, not a minimum or cross-stream table.
                if ((coverage >> 8) == 0 && (coverage & 0x7) == 0x1)
                {
                    int pairCount = bytes.U16(subtable + 6);
                    int low = 0;
                    int high = pairCount - 1;
                    while (low <= high)
                    {
                        int middle = low + (high - low) / 2;
                        std::uint32_t record = subtable + 14 + static_cast<std::uint32_t>(middle) * 6;
                        std::uint32_t value = bytes.U32(record);
                        if (value == pair)
                        {
                            total += bytes.I16(record + 4);
                            break;
                        }
                        if (value < pair)
                            low = middle + 1;
                        else
                            high = middle - 1;
                    }
                }
                subtable += static_cast<std::uint32_t>(length);
            }
            return total;
        }
        return 0.0f;
    }

    GlyphBitmap FontData::Rasterize(int glyph, float pixelsPerEm) const
    {
        if (!File || pixelsPerEm <= 0.0f)
        {
            return {};
        }
        // Real glyphs stay within a few ems; a damaged font's huge coordinates would
        // otherwise make a huge bitmap that takes a long time to fill.
        float largestSide = Max(64.0f, pixelsPerEm * 4.0f);
        return internal::RasterizeOutline(GlyphOutline(glyph), pixelsPerEm / File->Metrics.UnitsPerEm, largestSide);
    }
}
