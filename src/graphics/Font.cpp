#include <easyforge/graphics/Font.h>

#include "Resources.h"
#include "Text.h"

namespace easyforge
{
    namespace internal
    {
        char32_t NextCharacter(std::string_view text, std::size_t& position)
        {
            constexpr char32_t Replacement = 0xFFFD;
            auto byte = [&](std::size_t index) { return static_cast<unsigned char>(text[index]); };
            unsigned char first = byte(position);
            if (first < 0x80)
            {
                ++position;
                return first;
            }
            int length = first >= 0xF0 ? 4 : first >= 0xE0 ? 3 : first >= 0xC0 ? 2 : 0;
            if (length == 0 || position + static_cast<std::size_t>(length) > text.size())
            {
                ++position;
                return Replacement;
            }
            char32_t character = first & (0xFF >> (length + 1));
            for (int index = 1; index < length; ++index)
            {
                unsigned char next = byte(position + static_cast<std::size_t>(index));
                if ((next & 0xC0) != 0x80)
                {
                    ++position;
                    return Replacement;
                }
                character = (character << 6) | (next & 0x3F);
            }
            position += static_cast<std::size_t>(length);
            return character > 0x10FFFF ? Replacement : character;
        }

        namespace
        {
            std::uint64_t NextFontNumber()
            {
                static std::uint64_t next = 1;
                return next++;
            }
        }
    }

    Font Font::Load(std::string_view path, int faceIndex)
    {
        return FromData(FontData::Load(path, faceIndex));
    }

    Font Font::FromData(FontData data)
    {
        auto state = std::make_shared<internal::FontState>();
        state->ErrorText = data.Error();
        state->Data = std::move(data);
        state->Identifier = internal::NextFontNumber();
        return Font(state);
    }

    Font::Font() : Shared(std::make_shared<internal::FontState>())
    {
    }

    Font::Font(std::shared_ptr<internal::FontState> state) : Shared(std::move(state))
    {
    }

    Font::operator bool() const
    {
        return static_cast<bool>(Shared->Data);
    }

    const std::string& Font::Error() const
    {
        return Shared->ErrorText;
    }

    Vector2 Font::Measure(std::string_view text, float size) const
    {
        if (!Shared->Data || size <= 0.0f)
        {
            return {};
        }
        return internal::LayOutText(Shared->Data, text, size, [](int, Vector2) {});
    }

    float Font::LineHeight(float size) const
    {
        FontMetrics metrics = Shared->Data ? Shared->Data.Metrics() : FontMetrics {};
        return metrics.UnitsPerEm > 0.0f ? metrics.LineHeight() * size / metrics.UnitsPerEm : 0.0f;
    }

    const FontData& Font::Data() const
    {
        return Shared->Data;
    }
}
