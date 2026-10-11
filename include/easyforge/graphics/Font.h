#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <easyforge/assets/FontData.h>
#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Export.h>

namespace easyforge
{
    namespace internal
    {
        class FontState;
    }

    // A font ready to draw text with. Glyphs are drawn at each size the first
    // time they are needed and kept on the GPU.
    //
    //     Font font = Font::Load("Inter.ttf");
    //     canvas.Text(font, "Hello", { .Position = { 20, 20 }, .Size = 32 });
    //
    // Font is a handle: copies refer to the same font.
    class EASYFORGE_GRAPHICS_EXPORT Font
    {
    public:
        [[nodiscard]] static Font Load(std::string_view path, int faceIndex = 0);
        static Font FromData(FontData data);

        // No font. Tests as false.
        Font();

        // The font in a file, so settings can name it: `.Font = "fonts/Inter.ttf"`.
        // Everything that names the same file shares one font while any of them
        // is in use, so naming it in many places loads it once. A file that
        // cannot be read logs a warning and gives a font that tests as false.
        // Load always reads the file again.
        Font(const char* path);
        Font(const std::string& path);
        Font(std::string_view path);

        explicit operator bool() const;
        const std::string& Error() const;

        // The width and height of the text at a size in points: the widest line,
        // and the number of lines times the line height.
        Vector2 Measure(std::string_view text, float size) const;

        // The distance from one line's top to the next line's top, at a size in points.
        float LineHeight(float size) const;

        const FontData& Data() const;

        // The shared state; used by the graphics library itself.
        const std::shared_ptr<internal::FontState>& State() const { return Shared; }

    private:
        explicit Font(std::shared_ptr<internal::FontState> state);

        std::shared_ptr<internal::FontState> Shared;
    };
}
