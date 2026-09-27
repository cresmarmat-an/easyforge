#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <easyforge/core/Scalar.h>

namespace easyforge
{
    // A color with components from 0 to 1. The values are sRGB, the same as hex
    // codes and design tools use; graphics converts them to linear light where
    // blending needs it. Alpha 1 is opaque and 0 is invisible.
    struct Color
    {
        float Red = 0.0f;
        float Green = 0.0f;
        float Blue = 0.0f;
        float Alpha = 1.0f;

        static const Color White;
        static const Color Black;
        static const Color Transparent;

        // Reads "#RGB", "#RGBA", "#RRGGBB", or "#RRGGBBAA", with or without the "#".
        // Text that is not a hex color gives magenta (#FF00FF), which is hard to
        // miss on screen. Use IsHex to check text from outside the program first.
        static constexpr Color Hex(std::string_view text);

        static constexpr bool IsHex(std::string_view text);

        static constexpr Color FromBytes(
            std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
        {
            return { red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f };
        }

        constexpr Color WithAlpha(float alpha) const { return { Red, Green, Blue, alpha }; }

        // "#RRGGBB" when opaque, otherwise "#RRGGBBAA". Components outside 0 to 1 are clamped.
        std::string ToHex() const;

        // The same color in linear light, where adding and blending are physically correct.
        Color ToLinear() const;
        static Color FromLinear(Color linear);

        constexpr bool operator==(const Color&) const = default;
    };

    inline constexpr Color Color::White = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline constexpr Color Color::Black = { 0.0f, 0.0f, 0.0f, 1.0f };
    inline constexpr Color Color::Transparent = { 0.0f, 0.0f, 0.0f, 0.0f };

    namespace internal
    {
        constexpr int HexDigitValue(char digit)
        {
            if (digit >= '0' && digit <= '9')
            {
                return digit - '0';
            }
            if (digit >= 'a' && digit <= 'f')
            {
                return digit - 'a' + 10;
            }
            if (digit >= 'A' && digit <= 'F')
            {
                return digit - 'A' + 10;
            }
            return -1;
        }

        constexpr std::string_view WithoutHash(std::string_view text)
        {
            if (!text.empty() && text.front() == '#')
            {
                text.remove_prefix(1);
            }
            return text;
        }
    }

    constexpr bool Color::IsHex(std::string_view text)
    {
        text = internal::WithoutHash(text);
        if (text.size() != 3 && text.size() != 4 && text.size() != 6 && text.size() != 8)
        {
            return false;
        }
        for (char digit : text)
        {
            if (internal::HexDigitValue(digit) < 0)
            {
                return false;
            }
        }
        return true;
    }

    constexpr Color Color::Hex(std::string_view text)
    {
        if (!IsHex(text))
        {
            return { 1.0f, 0.0f, 1.0f, 1.0f };
        }
        text = internal::WithoutHash(text);

        int components[4] = { 0, 0, 0, 255 };
        if (text.size() <= 4)
        {
            // One digit per component: "F" means "FF".
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                components[index] = internal::HexDigitValue(text[index]) * 17;
            }
        }
        else
        {
            for (std::size_t index = 0; index < text.size() / 2; ++index)
            {
                components[index] =
                    internal::HexDigitValue(text[index * 2]) * 16 + internal::HexDigitValue(text[index * 2 + 1]);
            }
        }
        return FromBytes(static_cast<std::uint8_t>(components[0]), static_cast<std::uint8_t>(components[1]),
            static_cast<std::uint8_t>(components[2]), static_cast<std::uint8_t>(components[3]));
    }

    constexpr Color Lerp(Color from, Color to, float amount)
    {
        return {
            Lerp(from.Red, to.Red, amount),
            Lerp(from.Green, to.Green, amount),
            Lerp(from.Blue, to.Blue, amount),
            Lerp(from.Alpha, to.Alpha, amount),
        };
    }

    inline bool NearlyEqual(Color first, Color second, float tolerance = 0.00001f)
    {
        return NearlyEqual(first.Red, second.Red, tolerance) && NearlyEqual(first.Green, second.Green, tolerance) &&
               NearlyEqual(first.Blue, second.Blue, tolerance) && NearlyEqual(first.Alpha, second.Alpha, tolerance);
    }
}
