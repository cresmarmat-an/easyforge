#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace easyforge::language
{
    // Where something is in a source file, counting from 1.
    struct Location
    {
        int Line = 1;
        int Column = 1;

        bool operator==(const Location&) const = default;
    };

    // Something wrong with a source file, and where.
    struct Problem
    {
        Location Where;
        std::string Message;
    };

    enum class TokenKind
    {
        // A name such as `count` or `SomeName`.
        Name,

        // A word the language reserves, such as `function` or `then`.
        Keyword,

        // A number such as `12`, `0.5`, `1e3`, or `0xFF`.
        Number,

        // Text in double quotes. The token keeps what was between the quotes, with
        // escapes still in it, so `{name}` parts can be found later.
        Text,

        // A color such as `#FF8000` or `#FF800080`.
        ColorCode,

        // Punctuation or an operator, such as `(`, `==`, or `+=`.
        Symbol,

        EndOfFile,
    };

    struct Token
    {
        TokenKind Kind = TokenKind::EndOfFile;
        std::string Spelling;
        Location Where;

        // True when the token is the first on its line.
        bool StartsLine = false;

        bool Is(TokenKind kind, std::string_view spelling) const { return Kind == kind && Spelling == spelling; }
        bool IsKeyword(std::string_view word) const { return Is(TokenKind::Keyword, word); }
        bool IsSymbol(std::string_view symbol) const { return Is(TokenKind::Symbol, symbol); }
    };

    // True for the words the easyforge languages reserve.
    bool IsKeyword(std::string_view word);

    // Splits source text into tokens, leaving out comments (`--` to the end of
    // the line, and `--[[` to the next `]]`) and a UTF-8 byte order mark at the
    // start. The last token is always EndOfFile.
    // Anything that cannot be read is added to `problems`.
    std::vector<Token> Tokenize(std::string_view source, std::vector<Problem>& problems);
}
