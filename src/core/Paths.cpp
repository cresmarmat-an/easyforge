#include <easyforge/core/Paths.h>

#include <cstdint>

namespace easyforge
{
    namespace
    {
        constexpr char32_t Replacement = 0xFFFD;

        // The next character of UTF-8 text, moving `at` past it. A byte that does
        // not start a valid, shortest-form sequence gives the replacement
        // character and moves on by one.
        char32_t NextCharacter(std::string_view text, std::size_t& at)
        {
            auto byte = [&](std::size_t index) { return static_cast<std::uint8_t>(text[index]); };
            std::uint8_t first = byte(at);
            if (first < 0x80)
            {
                ++at;
                return first;
            }
            int length = first >= 0xF0 && first <= 0xF4 ? 4 : first >= 0xE0 ? 3 : first >= 0xC2 && first <= 0xDF ? 2 : 0;
            if (length == 0 || at + static_cast<std::size_t>(length) > text.size())
            {
                ++at;
                return Replacement;
            }
            char32_t value = first & (length == 2 ? 0x1F : length == 3 ? 0x0F : 0x07);
            for (int index = 1; index < length; ++index)
            {
                std::uint8_t next = byte(at + static_cast<std::size_t>(index));
                if ((next & 0xC0) != 0x80)
                {
                    ++at;
                    return Replacement;
                }
                value = (value << 6) | (next & 0x3F);
            }
            bool overlong = (length == 3 && value < 0x800) || (length == 4 && value < 0x10000);
            bool surrogate = value >= 0xD800 && value <= 0xDFFF;
            if (overlong || surrogate || value > 0x10FFFF)
            {
                ++at;
                return Replacement;
            }
            at += static_cast<std::size_t>(length);
            return value;
        }

        void AppendUtf8(std::string& text, char32_t value)
        {
            if (value < 0x80)
            {
                text += static_cast<char>(value);
            }
            else if (value < 0x800)
            {
                text += static_cast<char>(0xC0 | (value >> 6));
                text += static_cast<char>(0x80 | (value & 0x3F));
            }
            else if (value < 0x10000)
            {
                text += static_cast<char>(0xE0 | (value >> 12));
                text += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
                text += static_cast<char>(0x80 | (value & 0x3F));
            }
            else
            {
                text += static_cast<char>(0xF0 | (value >> 18));
                text += static_cast<char>(0x80 | ((value >> 12) & 0x3F));
                text += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
                text += static_cast<char>(0x80 | (value & 0x3F));
            }
        }
    }

#ifdef _WIN32
    // Windows names are UTF-16, so the text is turned into that by hand; the
    // conversions std::filesystem would make throw on text that is not valid.
    std::filesystem::path PathFromText(std::string_view text)
    {
        std::wstring wide;
        wide.reserve(text.size());
        for (std::size_t at = 0; at < text.size();)
        {
            char32_t value = NextCharacter(text, at);
            if (value >= 0x10000)
            {
                value -= 0x10000;
                wide += static_cast<wchar_t>(0xD800 + (value >> 10));
                wide += static_cast<wchar_t>(0xDC00 + (value & 0x3FF));
            }
            else
            {
                wide += static_cast<wchar_t>(value);
            }
        }
        return std::filesystem::path(std::move(wide));
    }

    std::string TextFromPath(const std::filesystem::path& path)
    {
        std::wstring wide = path.generic_wstring();
        std::string text;
        text.reserve(wide.size());
        for (std::size_t at = 0; at < wide.size(); ++at)
        {
            char32_t value = static_cast<char32_t>(wide[at]);
            if (value >= 0xD800 && value <= 0xDBFF && at + 1 < wide.size() && wide[at + 1] >= 0xDC00 && wide[at + 1] <= 0xDFFF)
            {
                value = 0x10000 + ((value - 0xD800) << 10) + (static_cast<char32_t>(wide[at + 1]) - 0xDC00);
                ++at;
            }
            else if (value >= 0xD800 && value <= 0xDFFF)
            {
                value = Replacement;
            }
            AppendUtf8(text, value);
        }
        return text;
    }
#else
    // Elsewhere names are bytes, which the text already is.
    std::filesystem::path PathFromText(std::string_view text)
    {
        return std::filesystem::path(std::string(text));
    }

    std::string TextFromPath(const std::filesystem::path& path)
    {
        return path.generic_string();
    }
#endif
}
