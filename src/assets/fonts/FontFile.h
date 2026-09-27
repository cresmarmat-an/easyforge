#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <easyforge/assets/FontData.h>

namespace easyforge::internal
{
    struct FontTable
    {
        std::uint32_t Offset = 0;
        std::uint32_t Length = 0;

        bool Present() const { return Length > 0; }
    };

    // A parsed TrueType font: the file's bytes and where its tables are.
    struct FontFile
    {
        std::vector<std::uint8_t> Bytes;

        FontTable Head;
        FontTable HorizontalHeader;
        FontTable MaximumProfile;
        FontTable HorizontalMetrics;
        FontTable Locations;
        FontTable Glyphs;
        FontTable CharacterMap;
        FontTable Kerning;
        FontTable Positioning;
        FontTable Names;
        FontTable WindowsMetrics;

        FontMetrics Metrics;
        int GlyphCount = 0;
        int HorizontalMetricCount = 0;
        bool LongLocations = false;

        // Absolute offset and format of the character map subtable in use.
        std::uint32_t CharacterMapOffset = 0;
        int CharacterMapFormat = 0;

        // Offsets of the pair-adjustment subtables of the GPOS "kern" feature,
        // grouped by lookup.
        std::vector<std::vector<std::uint32_t>> KerningLookups;

        std::string Family;
        std::string Style;
    };

    // The rasterizer, in FontRasterizer.cpp. Outlines wider or taller than
    // `largestSide` pixels give an empty bitmap.
    GlyphBitmap RasterizeOutline(const std::vector<Contour>& contours, float scale, float largestSide);
}
