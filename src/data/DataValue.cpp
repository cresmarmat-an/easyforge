#include <easyforge/data/DataValue.h>

#include <charconv>
#include <cmath>
#include <format>
#include <vector>

namespace easyforge
{
    std::string_view DataTypeName(DataType type)
    {
        switch (type)
        {
        case DataType::Nothing: return "nothing";
        case DataType::Boolean: return "boolean";
        case DataType::Integer: return "integer";
        case DataType::Number: return "number";
        case DataType::Text: return "text";
        case DataType::Vector2: return "vector2";
        case DataType::Vector3: return "vector3";
        case DataType::Vector4: return "vector4";
        case DataType::Color: return "color";
        }
        return "nothing";
    }

    bool DataValue::AsBoolean() const
    {
        const bool* value = std::get_if<bool>(&Stored);
        return value && *value;
    }

    std::int64_t DataValue::AsInteger() const
    {
        if (const std::int64_t* value = std::get_if<std::int64_t>(&Stored))
        {
            return *value;
        }
        if (const double* value = std::get_if<double>(&Stored))
        {
            return static_cast<std::int64_t>(std::llround(*value));
        }
        return 0;
    }

    double DataValue::AsNumber() const
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

    std::string DataValue::AsText() const
    {
        const std::string* value = std::get_if<std::string>(&Stored);
        return value ? *value : std::string();
    }

    Vector2 DataValue::AsVector2() const
    {
        const Vector2* value = std::get_if<Vector2>(&Stored);
        return value ? *value : Vector2 {};
    }

    Vector3 DataValue::AsVector3() const
    {
        const Vector3* value = std::get_if<Vector3>(&Stored);
        return value ? *value : Vector3 {};
    }

    Vector4 DataValue::AsVector4() const
    {
        const Vector4* value = std::get_if<Vector4>(&Stored);
        return value ? *value : Vector4 {};
    }

    Color DataValue::AsColor() const
    {
        const Color* value = std::get_if<Color>(&Stored);
        return value ? *value : Color {};
    }

    namespace
    {
        // The shortest text that reads back as exactly the same number, always
        // with a decimal point, so it is never mistaken for a whole number.
        std::string NumberText(double value)
        {
            char buffer[64];
            std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            std::string text(buffer, result.ptr);
            if (text.find_first_of(".eEn") == std::string::npos)
            {
                text += ".0";
            }
            return text;
        }

        std::string ComponentText(float value)
        {
            char buffer[64];
            std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            return std::string(buffer, result.ptr);
        }

        std::string_view Trim(std::string_view text)
        {
            std::size_t first = text.find_first_not_of(" \t\r");
            if (first == std::string_view::npos)
            {
                return {};
            }
            return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
        }

        bool ReadDouble(std::string_view text, double& value)
        {
            text = Trim(text);
            if (!text.empty() && text[0] == '+')
            {
                text.remove_prefix(1);
            }
            std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc() && result.ptr == text.data() + text.size() && !text.empty();
        }
    }

    std::string DataValue::ToText() const
    {
        switch (Type())
        {
        case DataType::Nothing: return "nothing";
        case DataType::Boolean: return AsBoolean() ? "true" : "false";
        case DataType::Integer: return std::to_string(AsInteger());
        case DataType::Number: return NumberText(AsNumber());
        case DataType::Text:
        {
            std::string text = "\"";
            for (char character : AsText())
            {
                switch (character)
                {
                case '"': text += "\\\""; break;
                case '\\': text += "\\\\"; break;
                case '\n': text += "\\n"; break;
                case '\t': text += "\\t"; break;
                case '\r': text += "\\r"; break;
                default: text += character; break;
                }
            }
            return text + "\"";
        }
        case DataType::Vector2:
        {
            Vector2 value = AsVector2();
            return ComponentText(value.X) + ", " + ComponentText(value.Y);
        }
        case DataType::Vector3:
        {
            Vector3 value = AsVector3();
            return ComponentText(value.X) + ", " + ComponentText(value.Y) + ", " + ComponentText(value.Z);
        }
        case DataType::Vector4:
        {
            Vector4 value = AsVector4();
            return ComponentText(value.X) + ", " + ComponentText(value.Y) + ", " + ComponentText(value.Z) + ", " +
                   ComponentText(value.W);
        }
        case DataType::Color: return AsColor().ToHex();
        }
        return "nothing";
    }

    DataValue DataValue::FromText(std::string_view text, bool* valid)
    {
        auto succeed = [&](DataValue value) {
            if (valid)
            {
                *valid = true;
            }
            return value;
        };
        auto fail = [&] {
            if (valid)
            {
                *valid = false;
            }
            return DataValue();
        };

        text = Trim(text);
        if (text == "nothing")
        {
            return succeed(DataValue());
        }
        if (text == "true" || text == "false")
        {
            return succeed(DataValue(text == "true"));
        }
        if (!text.empty() && text[0] == '"')
        {
            if (text.size() < 2 || text.back() != '"')
            {
                return fail();
            }
            std::string result;
            for (std::size_t index = 1; index + 1 < text.size(); ++index)
            {
                char character = text[index];
                if (character == '\\' && index + 2 < text.size())
                {
                    char escaped = text[++index];
                    result += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped == 'r' ? '\r' : escaped;
                    continue;
                }
                result += character;
            }
            return succeed(DataValue(std::move(result)));
        }
        if (!text.empty() && text[0] == '#')
        {
            if (!Color::IsHex(text))
            {
                return fail();
            }
            return succeed(DataValue(Color::Hex(text)));
        }
        if (text.find(',') != std::string_view::npos)
        {
            std::vector<float> parts;
            std::size_t start = 0;
            while (start <= text.size())
            {
                std::size_t comma = text.find(',', start);
                std::string_view part = text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
                double number = 0.0;
                if (!ReadDouble(part, number))
                {
                    return fail();
                }
                parts.push_back(static_cast<float>(number));
                if (comma == std::string_view::npos)
                {
                    break;
                }
                start = comma + 1;
            }
            switch (parts.size())
            {
            case 2: return succeed(DataValue(Vector2 { parts[0], parts[1] }));
            case 3: return succeed(DataValue(Vector3 { parts[0], parts[1], parts[2] }));
            case 4: return succeed(DataValue(Vector4 { parts[0], parts[1], parts[2], parts[3] }));
            default: return fail();
            }
        }
        // A whole number has only digits after an optional sign.
        std::string_view digits = !text.empty() && (text[0] == '-' || text[0] == '+') ? text.substr(1) : text;
        if (!digits.empty() && digits.find_first_not_of("0123456789") == std::string_view::npos)
        {
            std::int64_t integer = 0;
            std::string_view signless = text[0] == '+' ? text.substr(1) : text;
            std::from_chars_result result = std::from_chars(signless.data(), signless.data() + signless.size(), integer);
            if (result.ec == std::errc())
            {
                return succeed(DataValue(integer));
            }
        }
        double number = 0.0;
        if (ReadDouble(text, number))
        {
            return succeed(DataValue(number));
        }
        return fail();
    }
}
