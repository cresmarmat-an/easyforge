#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Color.h>
#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Export.h>

namespace easyforge
{
    namespace internal
    {
        class ShaderState;
    }

    // A value to give a shader's `value` of the same name:
    //
    //     { "Speed", 2.0f }, { "Tint", Color::Hex("#66CCFF") }
    struct EASYFORGE_GRAPHICS_EXPORT ShaderValue
    {
        // A number, or true or false for a boolean value.
        ShaderValue(std::string name, float value);
        ShaderValue(std::string name, Vector2 value);
        ShaderValue(std::string name, Vector3 value);
        ShaderValue(std::string name, Vector4 value);
        ShaderValue(std::string name, Color value);

        std::string Name {};

        // The numbers, in order; a number uses the first, a color all four.
        float Numbers[4] {};
        int Count = 0;
    };

    // A pixel shader written in the easyforge shader language:
    //
    //     -- ripple.shader
    //     value Speed: number = 1
    //
    //     function Pixel(input: PixelInput) returns color then
    //         constant wave = Sine(input.Position.X * 20 + input.Time * Speed)
    //         return Sample(input.Content, input.Coordinates + vector2(0, wave * 0.01))
    //     end
    //
    // Loading checks the whole shader and reports every problem with its line and
    // column. The shader is turned into the GPU's own language the first time it
    // is drawn.
    //
    // Shader is a handle: copies refer to the same shader.
    class EASYFORGE_GRAPHICS_EXPORT Shader
    {
    public:
        [[nodiscard]] static Shader Load(std::string_view path);
        [[nodiscard]] static Shader FromText(std::string_view source, std::string_view name = "shader");

        // No shader. Tests as false.
        Shader();

        // The shader in a file, so settings can name it:
        // `.Shader = "effects/wave.shader"`. Everything that names the same file
        // shares one shader while any of them is in use. A file that cannot be
        // read or checked logs a warning and gives a shader that tests as false.
        // Load always reads the file again.
        Shader(const char* path);
        Shader(const std::string& path);
        Shader(std::string_view path);

        explicit operator bool() const;

        // Every problem found, one a line, such as
        // "ripple.shader:5:12: 'wave' is not declared", or empty.
        const std::string& Error() const;

        // The names of the shader's values, in the order they are declared.
        std::vector<std::string> ValueNames() const;

        // The code the shader became for the GPU; useful when a shader does not
        // do what was expected.
        const std::string& GeneratedCode() const;

        // The shared state; used by the graphics library itself.
        const std::shared_ptr<internal::ShaderState>& State() const { return Shared; }

    private:
        explicit Shader(std::shared_ptr<internal::ShaderState> state);

        std::shared_ptr<internal::ShaderState> Shared;
    };
}
