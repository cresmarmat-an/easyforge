#pragma once

#include <easyforge/core/Rectangle.h>
#include <easyforge/graphics/Canvas.h>
#include <easyforge/graphics/Texture.h>
#include <easyforge/ui/Effects.h>

#include "Behavior.h"

namespace easyforge::ui::internal
{
    // Where a picture of `pictureSize` pixels goes in `box` for a fit, and which
    // part of it shows.
    struct FittedPicture
    {
        Rectangle Destination;
        Rectangle Source;
    };
    FittedPicture FitPicture(Vector2 pictureSize, Rectangle box, ImageFit fit);

    // Draws a picture into a box as an element or a background does.
    void DrawPicture(const Canvas& canvas, const Texture& texture, Rectangle box, ImageFit fit, float slice, Color tint,
        float cornerRadius);

    // Draws a box: its background and its border.
    void DrawBox(const Canvas& canvas, const Box& box, Rectangle area);

    // A ring around a box, for outlines and focus.
    void DrawRing(const Canvas& canvas, Rectangle area, float cornerRadius, float gap, float width, Color color);

    // Text lines placed in a box.
    void DrawTextIn(const Canvas& canvas, const Font& font, std::string_view text, float size, Color color, Rectangle box,
        TextAlignment alignment, bool centerVertically);
}
