#pragma once

// Lets std::format, Log, and test failure messages print easyforge values.
// Number formats apply to every component: std::format("{:.1f}", position).

#include <format>
#include <string_view>

#include <easyforge/core/Color.h>
#include <easyforge/core/Quaternion.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/core/Vector.h>

namespace easyforge::internal
{
    // Formats a list of floats as "(a, b, c)" with the number format given in braces.
    struct ComponentFormatter : std::formatter<float>
    {
        template <typename FormatContext, typename... Components>
        auto FormatComponents(FormatContext& context, Components... components) const
        {
            context.advance_to(std::format_to(context.out(), "("));
            bool first = true;
            (
                [&] {
                    if (!first)
                    {
                        context.advance_to(std::format_to(context.out(), ", "));
                    }
                    first = false;
                    context.advance_to(std::formatter<float>::format(components, context));
                }(),
                ...);
            return std::format_to(context.out(), ")");
        }
    };
}

template <>
struct std::formatter<easyforge::Vector2> : easyforge::internal::ComponentFormatter
{
    template <typename FormatContext>
    auto format(const easyforge::Vector2& vector, FormatContext& context) const
    {
        return FormatComponents(context, vector.X, vector.Y);
    }
};

template <>
struct std::formatter<easyforge::Vector3> : easyforge::internal::ComponentFormatter
{
    template <typename FormatContext>
    auto format(const easyforge::Vector3& vector, FormatContext& context) const
    {
        return FormatComponents(context, vector.X, vector.Y, vector.Z);
    }
};

template <>
struct std::formatter<easyforge::Vector4> : easyforge::internal::ComponentFormatter
{
    template <typename FormatContext>
    auto format(const easyforge::Vector4& vector, FormatContext& context) const
    {
        return FormatComponents(context, vector.X, vector.Y, vector.Z, vector.W);
    }
};

template <>
struct std::formatter<easyforge::Quaternion> : easyforge::internal::ComponentFormatter
{
    template <typename FormatContext>
    auto format(const easyforge::Quaternion& rotation, FormatContext& context) const
    {
        return FormatComponents(context, rotation.X, rotation.Y, rotation.Z, rotation.W);
    }
};

// Prints (X, Y, Width, Height).
template <>
struct std::formatter<easyforge::Rectangle> : easyforge::internal::ComponentFormatter
{
    template <typename FormatContext>
    auto format(const easyforge::Rectangle& rectangle, FormatContext& context) const
    {
        return FormatComponents(context, rectangle.X, rectangle.Y, rectangle.Width, rectangle.Height);
    }
};

// Prints the hex code, such as #15151A.
template <>
struct std::formatter<easyforge::Color> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const easyforge::Color& color, FormatContext& context) const
    {
        return std::formatter<std::string_view>::format(color.ToHex(), context);
    }
};
