#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <easyforge/core/Color.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    enum class DataType
    {
        Nothing,
        Boolean,
        Integer,
        Number,
        Text,
        Vector2,
        Vector3,
        Vector4,
        Color,
    };

    // The name of a type as people read it: "nothing", "integer", "vector2".
    std::string_view DataTypeName(DataType type);

    class DataValue;

    // The types a DataValue can be read as.
    template <typename Type>
    concept DataReadable = std::same_as<Type, bool> || (std::integral<Type> && !std::same_as<Type, char>) ||
                           std::floating_point<Type> || std::same_as<Type, std::string> || std::same_as<Type, Vector2> ||
                           std::same_as<Type, Vector3> || std::same_as<Type, Vector4> || std::same_as<Type, Color> ||
                           std::same_as<Type, DataValue>;

    // One value in a table: nothing, true or false, a whole number, a number,
    // text, a vector, or a color.
    //
    //     DataValue health = 100;
    //     int remaining = health.As<int>();
    //
    // Whole numbers and numbers read as each other; anything else read as the
    // wrong type gives that type's empty value, such as 0 or "".
    class DataValue
    {
    public:
        DataValue() = default;
        DataValue(bool value) : Stored(value) {}

        template <std::integral Integer>
            requires(!std::same_as<Integer, bool> && !std::same_as<Integer, char>)
        DataValue(Integer value) : Stored(static_cast<std::int64_t>(value))
        {
        }

        template <std::floating_point Floating>
        DataValue(Floating value) : Stored(static_cast<double>(value))
        {
        }

        DataValue(const char* text) : Stored(std::string(text)) {}
        DataValue(std::string text) : Stored(std::move(text)) {}
        DataValue(std::string_view text) : Stored(std::string(text)) {}
        DataValue(Vector2 value) : Stored(value) {}
        DataValue(Vector3 value) : Stored(value) {}
        DataValue(Vector4 value) : Stored(value) {}
        DataValue(Color value) : Stored(value) {}

        DataType Type() const { return static_cast<DataType>(Stored.index()); }
        bool IsNothing() const { return Stored.index() == 0; }

        bool AsBoolean() const;
        std::int64_t AsInteger() const;
        double AsNumber() const;
        std::string AsText() const;
        Vector2 AsVector2() const;
        Vector3 AsVector3() const;
        Vector4 AsVector4() const;
        Color AsColor() const;

        // The value as any readable type: `value.As<float>()`, `value.As<std::string>()`.
        template <DataReadable Type>
        Type As() const
        {
            if constexpr (std::same_as<Type, DataValue>)
            {
                return *this;
            }
            else if constexpr (std::same_as<Type, bool>)
            {
                return AsBoolean();
            }
            else if constexpr (std::integral<Type>)
            {
                return static_cast<Type>(AsInteger());
            }
            else if constexpr (std::floating_point<Type>)
            {
                return static_cast<Type>(AsNumber());
            }
            else if constexpr (std::same_as<Type, std::string>)
            {
                return AsText();
            }
            else if constexpr (std::same_as<Type, Vector2>)
            {
                return AsVector2();
            }
            else if constexpr (std::same_as<Type, Vector3>)
            {
                return AsVector3();
            }
            else if constexpr (std::same_as<Type, Vector4>)
            {
                return AsVector4();
            }
            else
            {
                return AsColor();
            }
        }

        // The value as it is written in a .tree file: 100, 4.5, "Ari", true,
        // 10, 20, #FF8000, or nothing.
        std::string ToText() const;

        // Reads what ToText writes. Anything else is read as nothing, with `valid`
        // set to false when it is given.
        static DataValue FromText(std::string_view text, bool* valid = nullptr);

        bool operator==(const DataValue&) const = default;

    private:
        std::variant<std::monostate, bool, std::int64_t, double, std::string, Vector2, Vector3, Vector4, Color> Stored;
    };
}

// Prints the value as a .tree file writes it, except that text is printed
// without quotes: std::format("Health: {}", player["Health"].Get()).
template <>
struct std::formatter<easyforge::DataValue> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const easyforge::DataValue& value, FormatContext& context) const
    {
        std::string text = value.Type() == easyforge::DataType::Text ? value.AsText() : value.ToText();
        return std::formatter<std::string_view>::format(text, context);
    }
};
