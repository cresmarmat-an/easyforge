#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <easyforge/core/Color.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    enum class MessageValueKind
    {
        Nothing,
        Boolean,
        Integer,
        Number,
        Text,
        Bytes,
        Vector2,
        Vector3,
        Vector4,
        Color,
    };

    class MessageValue;

    // The types a MessageValue reads as.
    template <typename Type>
    concept MessageReadable = std::same_as<Type, bool> || (std::integral<Type> && !std::same_as<Type, char>) ||
                              std::floating_point<Type> || std::same_as<Type, std::string> ||
                              std::same_as<Type, std::vector<std::uint8_t>> || std::same_as<Type, Vector2> ||
                              std::same_as<Type, Vector3> || std::same_as<Type, Vector4> || std::same_as<Type, Color> ||
                              std::same_as<Type, MessageValue>;

    // One value of a message.
    class MessageValue
    {
    public:
        // Nothing.
        MessageValue() = default;

        MessageValue(bool value) : Stored(value) {}

        template <std::integral Integer>
            requires(!std::same_as<Integer, bool> && !std::same_as<Integer, char>)
        MessageValue(Integer value) : Stored(static_cast<std::int64_t>(value))
        {
        }

        template <std::floating_point Floating>
        MessageValue(Floating value) : Stored(static_cast<double>(value))
        {
        }

        MessageValue(const char* value) : Stored(std::string(value ? value : "")) {}
        MessageValue(std::string value) : Stored(std::move(value)) {}
        MessageValue(std::string_view value) : Stored(std::string(value)) {}
        MessageValue(std::vector<std::uint8_t> bytes) : Stored(std::move(bytes)) {}
        MessageValue(easyforge::Vector2 value) : Stored(value) {}
        MessageValue(easyforge::Vector3 value) : Stored(value) {}
        MessageValue(easyforge::Vector4 value) : Stored(value) {}
        MessageValue(easyforge::Color value) : Stored(value) {}

        MessageValueKind Kind() const { return static_cast<MessageValueKind>(Stored.index()); }
        bool IsNothing() const { return Stored.index() == 0; }

        // Each reads its own kind; whole numbers and numbers read as each other,
        // and anything else reads as the empty value.
        bool AsBoolean() const;
        std::int64_t AsInteger() const;
        double AsNumber() const;
        std::string AsText() const;
        std::vector<std::uint8_t> AsBytes() const;
        easyforge::Vector2 AsVector2() const;
        easyforge::Vector3 AsVector3() const;
        easyforge::Vector4 AsVector4() const;
        easyforge::Color AsColor() const;

        template <MessageReadable Type>
        Type As() const
        {
            if constexpr (std::same_as<Type, MessageValue>)
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
            else if constexpr (std::same_as<Type, std::vector<std::uint8_t>>)
            {
                return AsBytes();
            }
            else if constexpr (std::same_as<Type, easyforge::Vector2>)
            {
                return AsVector2();
            }
            else if constexpr (std::same_as<Type, easyforge::Vector3>)
            {
                return AsVector3();
            }
            else if constexpr (std::same_as<Type, easyforge::Vector4>)
            {
                return AsVector4();
            }
            else
            {
                return AsColor();
            }
        }

        // Reads as the type it is given to: `int score = reply.Message["Score"];`.
        template <MessageReadable Type>
        operator Type() const
        {
            return As<Type>();
        }

        bool operator==(const MessageValue& other) const = default;

    private:
        std::variant<std::monostate, bool, std::int64_t, double, std::string, std::vector<std::uint8_t>, easyforge::Vector2,
            easyforge::Vector3, easyforge::Vector4, easyforge::Color>
            Stored;
    };

    // Named values sent between programs, in the order they were given.
    //
    //     client.Send("Chat", { { "Text", "hello" }, { "Color", Color::Hex("#66CCFF") } });
    //
    //     server.OnMessage("Chat", [](Connection from, const Message& message) {
    //         std::string text = message["Text"];
    //     });
    class Message
    {
    public:
        Message() = default;

        // A name given twice keeps the later value.
        Message(std::initializer_list<std::pair<std::string, MessageValue>> fields);

        // Writing: adds the name when it is new. As with std::map, reading this
        // way adds a missing name too; read with Get, or through a const
        // message, to leave the message as it is.
        MessageValue& operator[](std::string_view name);

        // Reading: nothing for a name the message does not have.
        MessageValue operator[](std::string_view name) const;
        MessageValue Get(std::string_view name) const;

        void Set(std::string_view name, MessageValue value);
        bool Has(std::string_view name) const;
        bool Remove(std::string_view name);
        std::vector<std::string> Names() const;
        std::size_t Size() const { return Fields.size(); }
        bool IsEmpty() const { return Fields.empty(); }

        // The message as bytes, and back. Decoding bytes that are not a message
        // gives nothing.
        std::vector<std::uint8_t> Encode() const;
        static std::optional<Message> Decode(std::span<const std::uint8_t> bytes);

        bool operator==(const Message& other) const = default;

    private:
        std::vector<std::pair<std::string, MessageValue>> Fields;
    };
}
