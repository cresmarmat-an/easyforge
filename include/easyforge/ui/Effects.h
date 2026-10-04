#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <easyforge/core/Color.h>
#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Texture.h>

namespace easyforge::ui
{
    // How an image fills an element's box.
    enum class ImageFit
    {
        // Stretched to the box, whatever its shape.
        Stretch,

        // As large as fits inside the box without cutting any off, centered.
        Contain,

        // As small as covers the whole box, centered, with the rest cut off.
        Cover,

        // One point per pixel, centered.
        Center,
    };

    struct ImageBackgroundSettings
    {
        ui::ImageFit Fit = ui::ImageFit::Stretch;

        // Keeps this many pixels at each edge from stretching, so a frame drawn in
        // a small image keeps its corners at any size. Only with Stretch.
        float Slice = 0.0f;

        // Multiplies every pixel; lowering the alpha fades the image.
        Color Tint = Color::White;
    };

    // What fills an element behind its content.
    //
    //     .Background = Color::Hex("#15151A")
    //     .Background = ui::Background::Gradient(Color::Hex("#243B55"), Color::Hex("#141E30"))
    //     .Background = ui::Background::Image("paper.png", { .Slice = 12 })
    class Background
    {
    public:
        enum class Kind
        {
            None,
            Color,
            Gradient,
            Image,
        };

        // Nothing, so what is behind shows through.
        Background() = default;

        Background(easyforge::Color color) : Type(Kind::Color), First(color) {}

        // From `from` to `to` along `angle` degrees: 0 from left to right, 90
        // from top to bottom.
        static Background Gradient(easyforge::Color from, easyforge::Color to, float angle = 90.0f);

        // An image file, found the way assets finds files.
        static Background Image(std::string_view path, const ImageBackgroundSettings& settings = {});
        static Background Image(const easyforge::Texture& texture, const ImageBackgroundSettings& settings = {});

        Kind Type = Kind::None;
        easyforge::Color First = easyforge::Color::Transparent;
        easyforge::Color Last = easyforge::Color::Transparent;
        float Angle = 90.0f;
        easyforge::Texture Picture;
        ImageBackgroundSettings PictureSettings;
    };

    // A soft shadow under the element, in the shape of its box.
    struct Shadow
    {
        Vector2 Offset { 0.0f, 4.0f };

        // How far the edge fades, in points.
        float Blur = 12.0f;

        // Grows the shadow's box on every side before it is blurred.
        float Spread = 0.0f;

        easyforge::Color Color { 0.0f, 0.0f, 0.0f, 0.3f };
    };

    // Light around the element, in the shape of its box. Without a color, the
    // theme's accent color.
    struct Glow
    {
        float Blur = 16.0f;
        float Spread = 0.0f;
        std::optional<easyforge::Color> Color;
    };

    // A line around the element, outside its box. Without a color, the theme's
    // focus color.
    struct Outline
    {
        float Width = 2.0f;

        // Space between the box and the line.
        float Gap = 0.0f;

        std::optional<easyforge::Color> Color;
    };

    // Blurs whatever is behind the element, like frosted glass. Give the element
    // a background that lets some of it through, such as white at 60%.
    struct BackgroundBlur
    {
        float Radius = 20.0f;
    };

    // Colors that change across the element, drawn over its background and
    // under its content.
    struct Gradient
    {
        easyforge::Color From = easyforge::Color::White.WithAlpha(0.2f);
        easyforge::Color To = easyforge::Color::Transparent;

        // 0 from left to right, 90 from top to bottom.
        float Angle = 90.0f;
    };

    // Changes the colors of the element and everything in it.
    struct ColorAdjust
    {
        // Added to every channel: -1 is black, 1 is white.
        float Brightness = 0.0f;

        // 1 leaves colors as they are; 0 is flat gray; above 1 is stronger.
        float Contrast = 1.0f;

        // 1 leaves colors as they are; 0 is gray; above 1 is more vivid.
        float Saturation = 1.0f;

        // Turns every color around the color wheel, in degrees.
        float Hue = 0.0f;
    };

    // Shows the element and everything in it only inside a rounded box, fading
    // at the edge over Feather points. Children that reach past the element's
    // rounded corners are cut to them.
    struct Mask
    {
        float CornerRadius = 0.0f;
        float Feather = 0.0f;
    };

    // Any one of the effects, for lists such as `.Effects = { ui::Shadow {}, ui::Outline {} }`.
    using Effect = std::variant<Shadow, Glow, Outline, BackgroundBlur, Gradient, ColorAdjust, Mask>;
}
