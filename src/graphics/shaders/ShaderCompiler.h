#pragma once

// Turns the easyforge shader language into the backend's shading language:
// HLSL for Direct3D 12. It reads the source with the parser the script language
// shares, checks every type, and writes a pixel shader around the `Pixel`
// function the source defines.

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Result.h>

namespace easyforge::internal
{
    enum class ShaderType
    {
        Nothing,
        Number,
        Boolean,
        Vector2,
        Vector3,
        Vector4,
        Color,
        PixelInput,
        Texture,
    };

    std::string_view Spelling(ShaderType type);

    // Numbers a value takes: 1 for number, 2 for vector2, and so on.
    int ComponentCount(ShaderType type);

    // A `value` the program can set, in the order they sit in the constants.
    struct ShaderValueSlot
    {
        std::string Name;
        ShaderType Type = ShaderType::Number;
        std::array<float, 4> Default {};
    };

    struct CompiledShader
    {
        std::string Code;
        std::vector<ShaderValueSlot> Values;
    };

    // The constants every custom shader starts with, before its values, in the
    // order the generated code declares them. Values follow, 16 bytes each.
    struct ShaderFrameConstants
    {
        float TargetSize[2] {};
        float AreaOrigin[2] {};
        float AreaSize[2] {};
        float PointSize[2] {};
        float Clip[4] {};
        float Time = 0.0f;
        float Scale = 1.0f;
        float HasContent = 0.0f;
        float Unused = 0.0f;

        // The content fills this share of its picture, from the top left: the
        // picture can be larger, so its size changes less often.
        float ContentScale[2] { 1.0f, 1.0f };
        float Reserved[2] {};
    };

    // Compiles shader source. On failure the error lists every problem, one a
    // line, as "name:line:column: message".
    Result<CompiledShader> CompileShader(std::string_view source, std::string_view name);
}
