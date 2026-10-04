#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <easyforge/core/Color.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Shader.h>

namespace easyforge
{
    class Font;
    class Scene;
    class Texture;

    namespace internal
    {
        class FrameRecorder;
    }

    // Colors that change evenly across a shape.
    struct LinearGradient
    {
        easyforge::Color From = easyforge::Color::White;
        easyforge::Color To = easyforge::Color::Black;

        // Which way the colors change, in degrees: 0 from left to right, 90 from
        // top to bottom, 180 from right to left. The first and last colors sit on
        // the shape's farthest corners.
        float Angle = 90.0f;
    };

    struct RectangleStyle
    {
        // The top left corner and the size, in points.
        Vector2 Position;
        Vector2 Size;

        easyforge::Color Color = easyforge::Color::White;

        // Rounds every corner by this many points.
        float CornerRadius = 0.0f;

        // A border drawn inside the edge.
        float BorderWidth = 0.0f;
        easyforge::Color BorderColor = easyforge::Color::Transparent;

        // Fills the shape with a gradient instead of Color.
        std::optional<LinearGradient> Gradient;

        // Softens the edge over this many points, half inside and half outside,
        // as a shadow or a glow does.
        float Blur = 0.0f;

        // An area left undrawn, in points, with its own rounded corners: the box
        // a shadow belongs to, so a see-through box does not show its shadow
        // through itself. Not used with a gradient.
        std::optional<easyforge::Rectangle> Hole;
        float HoleCornerRadius = 0.0f;
    };

    struct CircleStyle
    {
        easyforge::Color Color = easyforge::Color::White;
        float BorderWidth = 0.0f;
        easyforge::Color BorderColor = easyforge::Color::Transparent;
        std::optional<LinearGradient> Gradient;
        float Blur = 0.0f;
    };

    struct LineStyle
    {
        easyforge::Color Color = easyforge::Color::White;

        // In points.
        float Width = 1.0f;
    };

    struct ImageStyle
    {
        // The top left corner, in points.
        Vector2 Position;

        // The size to draw at, in points. Zero draws one point for each pixel of
        // the part of the texture shown.
        Vector2 Size;

        // The part of the texture to draw, in its pixels. Empty draws all of it.
        easyforge::Rectangle Source;

        // Multiplies every pixel. White leaves the image as it is; lowering the
        // alpha fades it.
        easyforge::Color Tint = easyforge::Color::White;

        float CornerRadius = 0.0f;

        // Keeps this many pixels at each edge of the image from stretching: the
        // corners keep their size, the edges stretch only along their length,
        // and the middle stretches both ways. A frame or a button made from a
        // small image stays sharp at any size this way. CornerRadius is not used
        // with a slice.
        float Slice = 0.0f;
    };

    struct TextStyle
    {
        // The top left corner of the first line, in points.
        Vector2 Position;

        // The font size in points: the height of an em.
        float Size = 16.0f;

        easyforge::Color Color = easyforge::Color::White;
    };

    // How a layer's content is put back onto the canvas.
    struct LayerStyle
    {
        // Runs every pixel of the layer through the shader, which reads the
        // content with Sample(input.Content, ...). No shader draws the content as
        // it is.
        easyforge::Shader Shader;
        std::vector<ShaderValue> Values;

        // Fades the whole layer.
        float Opacity = 1.0f;
    };

    // Draws 2D shapes, images, text, and 3D scenes into a frame. A canvas comes
    // from Renderer::BeginFrame and works until EndFrame; later libraries hand
    // one out too, such as ui for its drawing areas.
    //
    // Positions and sizes are in points from the top left, with Y growing
    // downward, as on screen. Later drawings cover earlier ones.
    class Canvas
    {
    public:
        // A canvas that draws nothing. Tests as false.
        Canvas() = default;

        explicit Canvas(internal::FrameRecorder* recorder) : Recorder(recorder) {}

        explicit operator bool() const { return Recorder != nullptr; }

        void Rectangle(const RectangleStyle& style) const;
        void Circle(Vector2 center, float radius, const CircleStyle& style = {}) const;
        void Line(Vector2 from, Vector2 to, const LineStyle& style = {}) const;
        void Image(const Texture& texture, const ImageStyle& style = {}) const;

        // Draws text in the font. A new line starts at each "\n".
        void Text(const Font& font, std::string_view text, const TextStyle& style = {}) const;

        // Draws the scene through its camera, filling the whole canvas or an area.
        void Draw(const Scene& scene) const;
        void Draw(const Scene& scene, easyforge::Rectangle area) const;

        // Blurs what is already drawn under the area, like frosted glass, over
        // `radius` points. What is drawn afterwards covers it as usual. With an
        // opacity below 1, the blur is mixed with what was there: 0.5 is half
        // blurred.
        void BlurBehind(easyforge::Rectangle area, float radius, float cornerRadius = 0.0f, float opacity = 1.0f) const;

        // Runs a shader over an area. Its input.Content is empty; to shade what
        // was drawn, use a layer.
        void Shaded(const easyforge::Shader& shader, easyforge::Rectangle area, std::vector<ShaderValue> values = {}) const;

        // Draws everything until the matching EndLayer into a picture of the area,
        // then puts the picture on the canvas: through a shader, or faded, or as
        // it is. Layers nest. An area too large for the GPU to hold in one
        // picture is cut down to the part inside the clip.
        void BeginLayer(easyforge::Rectangle area) const;
        void EndLayer(const LayerStyle& style = {}) const;

        // Only draws inside the area until the matching PopClip. Areas nest: each
        // one is cut down to the one before it.
        void PushClip(easyforge::Rectangle area) const;
        void PopClip() const;

        // Moves and scales everything drawn until the matching PopTransform: a
        // point is multiplied by `scale`, then moved by `offset`. Transforms nest.
        void PushTransform(Vector2 offset, float scale = 1.0f) const;
        void PopTransform() const;

        // The size of the canvas in points, and pixels per point.
        Vector2 Size() const;
        float Scale() const;

        // Where drawing can still show, in points after the transforms: the
        // canvas, cut down by every PushClip and layer.
        easyforge::Rectangle ClipArea() const;

    private:
        internal::FrameRecorder* Recorder = nullptr;
    };
}
