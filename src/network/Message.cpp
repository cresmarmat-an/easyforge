#include <easyforge/network/Message.h>

#include <algorithm>
#include <cmath>

#include "Encoding.h"

namespace easyforge
{
    using internal::networking::ByteReader;
    using internal::networking::ByteWriter;

    bool MessageValue::AsBoolean() const
    {
        const bool* value = std::get_if<bool>(&Stored);
        return value && *value;
    }

    std::int64_t MessageValue::AsInteger() const
    {
        if (const std::int64_t* value = std::get_if<std::int64_t>(&Stored))
        {
            return *value;
        }
        if (const double* value = std::get_if<double>(&Stored))
        {
            // Clamped first, because converting a double outside the range is undefined.
            if (std::isnan(*value))
            {
                return 0;
            }
            double clamped = std::clamp(*value, -9.2e18, 9.2e18);
            return static_cast<std::int64_t>(clamped);
        }
        return 0;
    }

    double MessageValue::AsNumber() const
    {
        if (const double* value = std::get_if<double>(&Stored))
        {
            return *value;
        }
        if (const std::int64_t* value = std::get_if<std::int64_t>(&Stored))
        {
            return static_cast<double>(*value);
        }
        return 0.0;
    }

    std::string MessageValue::AsText() const
    {
        const std::string* value = std::get_if<std::string>(&Stored);
        return value ? *value : std::string();
    }

    std::vector<std::uint8_t> MessageValue::AsBytes() const
    {
        const std::vector<std::uint8_t>* value = std::get_if<std::vector<std::uint8_t>>(&Stored);
        return value ? *value : std::vector<std::uint8_t>();
    }

    Vector2 MessageValue::AsVector2() const
    {
        const Vector2* value = std::get_if<Vector2>(&Stored);
        return value ? *value : Vector2 {};
    }

    Vector3 MessageValue::AsVector3() const
    {
        const Vector3* value = std::get_if<Vector3>(&Stored);
        return value ? *value : Vector3 {};
    }

    Vector4 MessageValue::AsVector4() const
    {
        const Vector4* value = std::get_if<Vector4>(&Stored);
        return value ? *value : Vector4 {};
    }

    Color MessageValue::AsColor() const
    {
        const Color* value = std::get_if<Color>(&Stored);
        return value ? *value : Color {};
    }

    Message::Message(std::initializer_list<std::pair<std::string, MessageValue>> fields)
    {
        for (const auto& [name, value] : fields)
        {
            Set(name, value);
        }
    }

    MessageValue& Message::operator[](std::string_view name)
    {
        for (auto& [fieldName, value] : Fields)
        {
            if (fieldName == name)
            {
                return value;
            }
        }
        return Fields.emplace_back(std::string(name), MessageValue()).second;
    }

    MessageValue Message::operator[](std::string_view name) const
    {
        return Get(name);
    }

    MessageValue Message::Get(std::string_view name) const
    {
        for (const auto& [fieldName, value] : Fields)
        {
            if (fieldName == name)
            {
                return value;
            }
        }
        return {};
    }

    void Message::Set(std::string_view name, MessageValue value)
    {
        (*this)[name] = std::move(value);
    }

    bool Message::Has(std::string_view name) const
    {
        return std::ranges::any_of(Fields, [&](const auto& field) { return field.first == name; });
    }

    bool Message::Remove(std::string_view name)
    {
        auto found = std::ranges::find_if(Fields, [&](const auto& field) { return field.first == name; });
        if (found == Fields.end())
        {
            return false;
        }
        Fields.erase(found);
        return true;
    }

    std::vector<std::string> Message::Names() const
    {
        std::vector<std::string> names;
        names.reserve(Fields.size());
        for (const auto& field : Fields)
        {
            names.push_back(field.first);
        }
        return names;
    }

    // Count u16, then for each field: name (u8 length and bytes), kind u8, and the
    // value. Text and bytes carry a u32 length.
    std::vector<std::uint8_t> Message::Encode() const
    {
        std::vector<std::uint8_t> bytes;
        ByteWriter writer(bytes);
        std::size_t count = std::min<std::size_t>(Fields.size(), 65535);
        writer.Write16(static_cast<std::uint16_t>(count));
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& [name, value] = Fields[index];
            writer.WriteShortText(name);
            writer.Write8(static_cast<std::uint8_t>(value.Kind()));
            switch (value.Kind())
            {
            case MessageValueKind::Nothing:
                break;
            case MessageValueKind::Boolean:
                writer.Write8(value.AsBoolean() ? 1 : 0);
                break;
            case MessageValueKind::Integer:
                writer.Write64(static_cast<std::uint64_t>(value.AsInteger()));
                break;
            case MessageValueKind::Number:
                writer.WriteDouble(value.AsNumber());
                break;
            case MessageValueKind::Text:
            {
                std::string text = value.AsText();
                writer.Write32(static_cast<std::uint32_t>(text.size()));
                writer.WriteBytes(text);
                break;
            }
            case MessageValueKind::Bytes:
            {
                std::vector<std::uint8_t> data = value.AsBytes();
                writer.Write32(static_cast<std::uint32_t>(data.size()));
                writer.WriteBytes(data);
                break;
            }
            case MessageValueKind::Vector2:
            {
                Vector2 vector = value.AsVector2();
                writer.WriteFloat(vector.X);
                writer.WriteFloat(vector.Y);
                break;
            }
            case MessageValueKind::Vector3:
            {
                Vector3 vector = value.AsVector3();
                writer.WriteFloat(vector.X);
                writer.WriteFloat(vector.Y);
                writer.WriteFloat(vector.Z);
                break;
            }
            case MessageValueKind::Vector4:
            {
                Vector4 vector = value.AsVector4();
                writer.WriteFloat(vector.X);
                writer.WriteFloat(vector.Y);
                writer.WriteFloat(vector.Z);
                writer.WriteFloat(vector.W);
                break;
            }
            case MessageValueKind::Color:
            {
                Color color = value.AsColor();
                writer.WriteFloat(color.Red);
                writer.WriteFloat(color.Green);
                writer.WriteFloat(color.Blue);
                writer.WriteFloat(color.Alpha);
                break;
            }
            }
        }
        return bytes;
    }

    std::optional<Message> Message::Decode(std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        Message message;
        std::uint16_t count = reader.Read16();
        for (std::uint16_t index = 0; index < count && !reader.Failed(); ++index)
        {
            std::string name = reader.ReadShortText();
            std::uint8_t kind = reader.Read8();
            MessageValue value;
            switch (static_cast<MessageValueKind>(kind))
            {
            case MessageValueKind::Nothing:
                break;
            case MessageValueKind::Boolean:
                value = reader.Read8() != 0;
                break;
            case MessageValueKind::Integer:
                value = static_cast<std::int64_t>(reader.Read64());
                break;
            case MessageValueKind::Number:
                value = reader.ReadDouble();
                break;
            case MessageValueKind::Text:
                value = reader.ReadText(reader.Read32());
                break;
            case MessageValueKind::Bytes:
            {
                std::span<const std::uint8_t> data = reader.ReadBytes(reader.Read32());
                value = std::vector<std::uint8_t>(data.begin(), data.end());
                break;
            }
            case MessageValueKind::Vector2:
            {
                float x = reader.ReadFloat();
                float y = reader.ReadFloat();
                value = Vector2 { x, y };
                break;
            }
            case MessageValueKind::Vector3:
            {
                float x = reader.ReadFloat();
                float y = reader.ReadFloat();
                float z = reader.ReadFloat();
                value = Vector3 { x, y, z };
                break;
            }
            case MessageValueKind::Vector4:
            {
                float x = reader.ReadFloat();
                float y = reader.ReadFloat();
                float z = reader.ReadFloat();
                float w = reader.ReadFloat();
                value = Vector4 { x, y, z, w };
                break;
            }
            case MessageValueKind::Color:
            {
                float red = reader.ReadFloat();
                float green = reader.ReadFloat();
                float blue = reader.ReadFloat();
                float alpha = reader.ReadFloat();
                value = Color { red, green, blue, alpha };
                break;
            }
            default:
                return std::nullopt;
            }
            // Encode never repeats a name, so the fields are kept as they come.
            message.Fields.emplace_back(std::move(name), std::move(value));
        }
        if (reader.Failed() || !reader.IsAtEnd())
        {
            return std::nullopt;
        }
        return message;
    }
}
