#pragma once

// Working with UTF-8 text for labels and fields: stepping by characters and
// words, and breaking lines to a width.

#include <cstddef>
#include <string>
#include <string_view>

#include <easyforge/graphics/Font.h>

namespace easyforge::ui::internal
{
    // The byte where the character after, or before, `index` starts.
    std::size_t NextCharacter(std::string_view text, std::size_t index);
    std::size_t PreviousCharacter(std::string_view text, std::size_t index);

    // The byte where the next or previous word starts, for moving with Control.
    std::size_t NextWord(std::string_view text, std::size_t index);
    std::size_t PreviousWord(std::string_view text, std::size_t index);

    // The whole word around `index`, for a double click.
    void WordAround(std::string_view text, std::size_t index, std::size_t& start, std::size_t& end);

    std::size_t CountCharacters(std::string_view text);

    // The text with line breaks added between words so no line is wider than
    // `width` points. A word wider than a whole line is broken between characters.
    std::string WrapText(const Font& font, std::string_view text, float size, float width);
}
