#include "Text.h"

#include <vector>

namespace easyforge::ui::internal
{
    namespace
    {
        bool Continues(char character)
        {
            return (static_cast<unsigned char>(character) & 0xC0) == 0x80;
        }

        bool IsSpace(char character)
        {
            return character == ' ' || character == '\t' || character == '\n';
        }

        // Letters and digits, and anything outside ASCII, which is mostly letters.
        bool IsWordCharacter(char character)
        {
            unsigned char value = static_cast<unsigned char>(character);
            return value >= 0x80 || (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
                   (value >= 'a' && value <= 'z') || value == '_';
        }
    }

    std::size_t NextCharacter(std::string_view text, std::size_t index)
    {
        if (index >= text.size())
        {
            return text.size();
        }
        ++index;
        while (index < text.size() && Continues(text[index]))
        {
            ++index;
        }
        return index;
    }

    std::size_t PreviousCharacter(std::string_view text, std::size_t index)
    {
        if (index == 0)
        {
            return 0;
        }
        index = index > text.size() ? text.size() : index;
        --index;
        while (index > 0 && Continues(text[index]))
        {
            --index;
        }
        return index;
    }

    std::size_t NextWord(std::string_view text, std::size_t index)
    {
        while (index < text.size() && IsWordCharacter(text[index]))
        {
            index = NextCharacter(text, index);
        }
        while (index < text.size() && !IsWordCharacter(text[index]))
        {
            index = NextCharacter(text, index);
        }
        return index;
    }

    std::size_t PreviousWord(std::string_view text, std::size_t index)
    {
        while (index > 0 && !IsWordCharacter(text[PreviousCharacter(text, index)]))
        {
            index = PreviousCharacter(text, index);
        }
        while (index > 0 && IsWordCharacter(text[PreviousCharacter(text, index)]))
        {
            index = PreviousCharacter(text, index);
        }
        return index;
    }

    void WordAround(std::string_view text, std::size_t index, std::size_t& start, std::size_t& end)
    {
        index = index > text.size() ? text.size() : index;
        start = index;
        end = index;
        bool word = index < text.size() ? IsWordCharacter(text[index])
                                        : (index > 0 && IsWordCharacter(text[PreviousCharacter(text, index)]));
        while (start > 0 && IsWordCharacter(text[PreviousCharacter(text, start)]) == word)
        {
            start = PreviousCharacter(text, start);
        }
        while (end < text.size() && IsWordCharacter(text[end]) == word)
        {
            end = NextCharacter(text, end);
        }
    }

    std::size_t CountCharacters(std::string_view text)
    {
        std::size_t count = 0;
        for (char character : text)
        {
            count += Continues(character) ? 0 : 1;
        }
        return count;
    }

    std::string WrapText(const Font& font, std::string_view text, float size, float width)
    {
        if (!font || width <= 0.0f)
        {
            return std::string(text);
        }
        std::string result;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            std::string_view paragraph = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

            std::string line;
            std::size_t position = 0;
            while (position < paragraph.size())
            {
                // The next word, with the spaces before it.
                std::size_t wordStart = position;
                while (wordStart < paragraph.size() && IsSpace(paragraph[wordStart]))
                {
                    ++wordStart;
                }
                std::size_t wordEnd = wordStart;
                while (wordEnd < paragraph.size() && !IsSpace(paragraph[wordEnd]))
                {
                    ++wordEnd;
                }
                std::string_view spaces = paragraph.substr(position, wordStart - position);
                std::string_view word = paragraph.substr(wordStart, wordEnd - wordStart);
                position = wordEnd;

                std::string candidate = line.empty() ? std::string(word) : line + std::string(spaces) + std::string(word);
                if (font.Measure(candidate, size).X <= width || line.empty())
                {
                    line = std::move(candidate);
                }
                else
                {
                    result += line + '\n';
                    line = std::string(word);
                }

                // A word too wide for any line is broken between characters.
                while (font.Measure(line, size).X > width && CountCharacters(line) > 1)
                {
                    std::size_t cut = PreviousCharacter(line, line.size());
                    while (cut > 0 && font.Measure(std::string_view(line).substr(0, cut), size).X > width)
                    {
                        cut = PreviousCharacter(line, cut);
                    }
                    if (cut == 0)
                    {
                        cut = NextCharacter(line, 0);
                    }
                    result += line.substr(0, cut) + '\n';
                    line = line.substr(cut);
                }
            }
            result += line;
            if (end == std::string_view::npos)
            {
                break;
            }
            result += '\n';
            start = end + 1;
        }
        return result;
    }
}
