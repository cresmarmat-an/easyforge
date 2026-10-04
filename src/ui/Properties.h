#pragma once

// How element handles' properties reach their element: settings that are table
// values go through the element's node, and the rest through the values kept
// beside it.

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <easyforge/core/Host.h>
#include <easyforge/ui/Displays.h>
#include <easyforge/ui/Elements.h>
#include <easyforge/ui/Layout.h>

#include "ElementState.h"

namespace easyforge::ui::internal
{
    // A property's name, usable as a template argument: Stored<float, "Gap">.
    template <std::size_t Length>
    struct PropertyName
    {
        char Text[Length] {};

        constexpr PropertyName(const char (&text)[Length])
        {
            for (std::size_t index = 0; index < Length; ++index)
            {
                Text[index] = text[index];
            }
        }

        constexpr std::string_view View() const { return { Text, Length - 1 }; }
    };

    // ---- Enumerations as text, so the table reads well --------------------

    template <typename Enumeration>
    struct EnumerationNames;

    template <>
    struct EnumerationNames<Alignment>
    {
        static constexpr std::array<std::string_view, 4> Names = { "start", "center", "end", "stretch" };
    };

    template <>
    struct EnumerationNames<Distribution>
    {
        static constexpr std::array<std::string_view, 6> Names = { "start", "center", "end", "space between",
            "space around", "space evenly" };
    };

    template <>
    struct EnumerationNames<TextAlignment>
    {
        static constexpr std::array<std::string_view, 3> Names = { "start", "center", "end" };
    };

    template <>
    struct EnumerationNames<ImageFit>
    {
        static constexpr std::array<std::string_view, 4> Names = { "stretch", "contain", "cover", "center" };
    };

    template <>
    struct EnumerationNames<ButtonStyle>
    {
        static constexpr std::array<std::string_view, 3> Names = { "normal", "accent", "subtle" };
    };

    template <>
    struct EnumerationNames<ScrollDirection>
    {
        static constexpr std::array<std::string_view, 2> Names = { "vertical", "horizontal" };
    };

    template <>
    struct EnumerationNames<easyforge::Cursor>
    {
        static constexpr std::array<std::string_view, 13> Names = { "arrow", "text", "hand", "crosshair", "move",
            "resize horizontal", "resize vertical", "resize diagonal", "resize anti-diagonal", "not allowed", "wait",
            "progress", "hidden" };
    };

    template <typename Type>
    concept NamedEnumeration = requires { EnumerationNames<Type>::Names; };

    // ---- Conversions --------------------------------------------------------

    DataValue SizeToData(Size size);
    Size SizeFromData(const DataValue& value);

    template <typename Value>
    DataValue ToData(const Value& value)
    {
        if constexpr (NamedEnumeration<Value>)
        {
            std::size_t index = static_cast<std::size_t>(value);
            const auto& names = EnumerationNames<Value>::Names;
            return index < names.size() ? DataValue(names[index]) : DataValue();
        }
        else if constexpr (std::is_same_v<Value, Size>)
        {
            return SizeToData(value);
        }
        else if constexpr (std::is_same_v<Value, Insets>)
        {
            return DataValue(Vector4 { value.Left, value.Top, value.Right, value.Bottom });
        }
        else if constexpr (requires { typename Value::value_type; value.has_value(); })
        {
            return value ? ToData(*value) : DataValue();
        }
        else
        {
            return DataValue(value);
        }
    }

    template <typename Value>
    Value FromData(const DataValue& data)
    {
        if constexpr (NamedEnumeration<Value>)
        {
            std::string text = data.AsText();
            const auto& names = EnumerationNames<Value>::Names;
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                if (names[index] == text)
                {
                    return static_cast<Value>(index);
                }
            }
            return Value {};
        }
        else if constexpr (std::is_same_v<Value, Size>)
        {
            return SizeFromData(data);
        }
        else if constexpr (std::is_same_v<Value, Insets>)
        {
            if (data.Type() == DataType::Integer || data.Type() == DataType::Number)
            {
                return Insets(data.As<float>());
            }
            Vector4 sides = data.AsVector4();
            return Insets(sides.X, sides.Y, sides.Z, sides.W);
        }
        else if constexpr (requires { typename Value::value_type; std::declval<Value>().has_value(); })
        {
            if (data.IsNothing())
            {
                return std::nullopt;
            }
            return FromData<typename Value::value_type>(data);
        }
        else
        {
            return data.As<Value>();
        }
    }

    // ---- Properties ---------------------------------------------------------

    // A setting kept as a table value.
    template <typename Value, PropertyName Name>
    Property<Value> Stored(std::shared_ptr<ElementState>& state)
    {
        return Property<Value>(
            &state,
            [](const void* owner) -> Value {
                ElementState* element = StateOf(owner);
                return FromData<Value>(element ? element->Get(Name.View()) : DataValue());
            },
            [](void* owner, const Value& value) {
                if (ElementState* element = StateOf(owner))
                {
                    if (!element->Animating)
                    {
                        StopAnimation(*element, Name.View());
                    }
                    element->Set(Name.View(), ToData(value));
                }
            });
    }

    // The same, moving to new values with AnimateTo.
    template <typename Value, PropertyName Name>
    Animated<Value> StoredAnimated(std::shared_ptr<ElementState>& state)
    {
        return Animated<Value>(
            &state,
            [](const void* owner) -> Value {
                ElementState* element = StateOf(owner);
                return FromData<Value>(element ? element->Get(Name.View()) : DataValue());
            },
            [](void* owner, const Value& value) {
                if (ElementState* element = StateOf(owner))
                {
                    if (!element->Animating)
                    {
                        StopAnimation(*element, Name.View());
                    }
                    element->Set(Name.View(), ToData(value));
                }
            },
            Name.View(), &StartElementAnimation);
    }

    // A setting kept beside the table, such as a callback or a texture.
    template <typename Value, PropertyName Name, bool AffectsLayout = false>
    Property<Value> Kept(std::shared_ptr<ElementState>& state)
    {
        return Property<Value>(
            &state,
            [](const void* owner) -> Value {
                ElementState* element = StateOf(owner);
                return element ? element->Object<Value>(Name.View()) : Value {};
            },
            [](void* owner, const Value& value) {
                if (ElementState* element = StateOf(owner))
                {
                    element->SetObject(Name.View(), value);
                    element->Changed(AffectsLayout);
                }
            });
    }

    // Writes a setting from a settings struct when it was changed from the
    // struct's own default, so the table holds only what was chosen and the
    // kind's defaults apply to the rest.
    template <typename Value>
    void Choose(ElementState& element, std::string_view name, const Value& value, const Value& unchosen)
    {
        DataValue data = ToData(value);
        if (!(data == ToData(unchosen)))
        {
            element.Set(name, data);
        }
    }

    // Writes the settings every element has, from any settings struct that has
    // them.
    template <typename Settings>
    void ApplyCommonSettings(ElementState& element, const Settings& settings)
    {
        const Settings defaults {};
        if (!settings.Name.empty())
        {
            element.Row.Rename(settings.Name);
        }
        Choose(element, "Width", settings.Width, defaults.Width);
        Choose(element, "Height", settings.Height, defaults.Height);
        Choose(element, "MinimumWidth", settings.MinimumWidth, defaults.MinimumWidth);
        Choose(element, "MinimumHeight", settings.MinimumHeight, defaults.MinimumHeight);
        Choose(element, "MaximumWidth", settings.MaximumWidth, defaults.MaximumWidth);
        Choose(element, "MaximumHeight", settings.MaximumHeight, defaults.MaximumHeight);
        Choose(element, "Margin", settings.Margin, defaults.Margin);
        if constexpr (requires { settings.Padding; })
        {
            Choose(element, "Padding", settings.Padding, defaults.Padding);
        }
        Choose(element, "CornerRadius", settings.CornerRadius, defaults.CornerRadius);
        if constexpr (requires { settings.Background; })
        {
            if (settings.Background)
            {
                element.SetObject("Background", settings.Background);
            }
        }
        if constexpr (requires { settings.BorderWidth; })
        {
            Choose(element, "BorderWidth", settings.BorderWidth, defaults.BorderWidth);
        }
        Choose(element, "BorderColor", settings.BorderColor, defaults.BorderColor);
        Choose(element, "Opacity", settings.Opacity, defaults.Opacity);
        Choose(element, "Offset", settings.Offset, defaults.Offset);
        Choose(element, "Scale", settings.Scale, defaults.Scale);
        if (!settings.Effects.empty())
        {
            element.SetObject("Effects", settings.Effects);
        }
        if (settings.Shader)
        {
            element.SetObject("Shader", settings.Shader);
        }
        if (!settings.ShaderValues.empty())
        {
            element.SetObject("ShaderValues", settings.ShaderValues);
        }
        Choose(element, "Visible", settings.Visible, defaults.Visible);
        if constexpr (requires { settings.Enabled; })
        {
            Choose(element, "Enabled", settings.Enabled, defaults.Enabled);
        }
        if constexpr (requires { settings.Tooltip; })
        {
            Choose(element, "Tooltip", settings.Tooltip, defaults.Tooltip);
        }
        if constexpr (requires { settings.Cursor; })
        {
            Choose(element, "Cursor", settings.Cursor, defaults.Cursor);
        }
    }

    // Adds each child of a settings struct.
    void AddChildren(ElementState& parent, const std::vector<Element>& children);

    // The state behind a handle, for code inside the library.
    const std::shared_ptr<ElementState>& StateOfHandle(const Element& element);
}
