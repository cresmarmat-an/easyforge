#include <easyforge/core/language/Tokens.h>

#include <array>
#include <format>

namespace easyforge::language
{
    namespace
    {
        constexpr std::array<std::string_view, 29> Keywords = {
            "function", "returns", "then", "end", "variable", "constant", "value", "if", "else", "while", "for",
            "in", "to", "return", "break", "continue", "and", "or", "not", "true", "false", "nothing", "type",
            "import", "try", "catch", "spawn", "wait", "yield",
        };

        // Longest first, so `==` is read before `=`.
        constexpr std::array<std::string_view, 23> Symbols = {
            "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "(", ")", "[", "]", "{", "}", ",", ":", ".", "=", "<",
            ">", "+", "-", "*",
        };

        constexpr std::array<std::string_view, 3> MoreSymbols = { "/", "%", "^" };

        bool IsLetter(char character)
        {
            return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_' ||
                   static_cast<unsigned char>(character) >= 0x80;
        }

        bool IsDigit(char character)
        {
            return character >= '0' && character <= '9';
        }

        bool IsHexDigit(char character)
        {
            return IsDigit(character) || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
        }

        class Reader
        {
        public:
            Reader(std::string_view source, std::vector<Problem>& problems) : Source(source), Problems(problems) {}

            std::vector<Token> Read()
            {
                std::vector<Token> tokens;
                while (true)
                {
                    SkipSpaceAndComments();
                    Token token;
                    token.Where = Here();
                    token.StartsLine = LineStart;
                    if (Position >= Source.size())
                    {
                        token.Kind = TokenKind::EndOfFile;
                        tokens.push_back(token);
                        return tokens;
                    }
                    LineStart = false;
                    if (!ReadToken(token))
                    {
                        continue;
                    }
                    tokens.push_back(std::move(token));
                }
            }

        private:
            Location Here() const { return { Line, Column }; }

            char Peek(std::size_t ahead = 0) const
            {
                return Position + ahead < Source.size() ? Source[Position + ahead] : '\0';
            }

            void Advance(std::size_t count = 1)
            {
                for (std::size_t index = 0; index < count && Position < Source.size(); ++index)
                {
                    if (Source[Position] == '\n')
                    {
                        ++Line;
                        Column = 1;
                        LineStart = true;
                    }
                    else
                    {
                        ++Column;
                    }
                    ++Position;
                }
            }

            void Report(Location where, std::string message) { Problems.push_back({ where, std::move(message) }); }

            void SkipSpaceAndComments()
            {
                while (Position < Source.size())
                {
                    char character = Peek();
                    if (character == ' ' || character == '\t' || character == '\r' || character == '\n')
                    {
                        Advance();
                        continue;
                    }
                    if (character == '-' && Peek(1) == '-')
                    {
                        if (Peek(2) == '[' && Peek(3) == '[')
                        {
                            Location start = Here();
                            // A comment inside a line leaves the line's start as it was.
                            bool lineStart = LineStart;
                            Advance(4);
                            while (Position < Source.size() && !(Peek() == ']' && Peek(1) == ']'))
                            {
                                Advance();
                            }
                            if (Position >= Source.size())
                            {
                                Report(start, "this comment starts with --[[ but never ends with ]]");
                                return;
                            }
                            Advance(2);
                            LineStart = LineStart || lineStart;
                            continue;
                        }
                        while (Position < Source.size() && Peek() != '\n')
                        {
                            Advance();
                        }
                        continue;
                    }
                    return;
                }
            }

            bool ReadToken(Token& token)
            {
                char character = Peek();
                if (IsLetter(character))
                {
                    std::size_t start = Position;
                    while (IsLetter(Peek()) || IsDigit(Peek()))
                    {
                        Advance();
                    }
                    token.Spelling = std::string(Source.substr(start, Position - start));
                    token.Kind = IsKeyword(token.Spelling) ? TokenKind::Keyword : TokenKind::Name;
                    return true;
                }
                if (IsDigit(character) || (character == '.' && IsDigit(Peek(1))))
                {
                    return ReadNumber(token);
                }
                if (character == '"')
                {
                    return ReadText(token);
                }
                if (character == '#')
                {
                    std::size_t start = Position;
                    Advance();
                    while (IsHexDigit(Peek()))
                    {
                        Advance();
                    }
                    token.Spelling = std::string(Source.substr(start, Position - start));
                    token.Kind = TokenKind::ColorCode;
                    std::size_t digits = token.Spelling.size() - 1;
                    if (digits != 6 && digits != 8)
                    {
                        Report(token.Where, std::format("'{}' is not a color: write #RRGGBB or #RRGGBBAA", token.Spelling));
                    }
                    return true;
                }
                for (std::string_view symbol : Symbols)
                {
                    if (Source.substr(Position, symbol.size()) == symbol)
                    {
                        token.Kind = TokenKind::Symbol;
                        token.Spelling = std::string(symbol);
                        Advance(symbol.size());
                        return true;
                    }
                }
                for (std::string_view symbol : MoreSymbols)
                {
                    if (Source.substr(Position, symbol.size()) == symbol)
                    {
                        token.Kind = TokenKind::Symbol;
                        token.Spelling = std::string(symbol);
                        Advance(symbol.size());
                        return true;
                    }
                }
                Report(token.Where, std::format("'{}' is not something the language understands here", character));
                Advance();
                return false;
            }

            bool ReadNumber(Token& token)
            {
                std::size_t start = Position;
                token.Kind = TokenKind::Number;
                if (Peek() == '0' && (Peek(1) == 'x' || Peek(1) == 'X'))
                {
                    Advance(2);
                    while (IsHexDigit(Peek()))
                    {
                        Advance();
                    }
                }
                else
                {
                    while (IsDigit(Peek()))
                    {
                        Advance();
                    }
                    // A dot followed by a digit continues the number; `1.X` does not.
                    if (Peek() == '.' && IsDigit(Peek(1)))
                    {
                        Advance();
                        while (IsDigit(Peek()))
                        {
                            Advance();
                        }
                    }
                    if ((Peek() == 'e' || Peek() == 'E') &&
                        (IsDigit(Peek(1)) || ((Peek(1) == '+' || Peek(1) == '-') && IsDigit(Peek(2)))))
                    {
                        Advance(2);
                        while (IsDigit(Peek()))
                        {
                            Advance();
                        }
                    }
                }
                token.Spelling = std::string(Source.substr(start, Position - start));
                if (IsLetter(Peek()))
                {
                    Report(Here(), std::format("a number cannot be followed directly by '{}'", Peek()));
                }
                return true;
            }

            bool ReadText(Token& token)
            {
                token.Kind = TokenKind::Text;
                Advance();
                std::size_t start = Position;
                int braces = 0;
                while (Position < Source.size())
                {
                    char character = Peek();
                    if (character == '\\')
                    {
                        Advance(2);
                        continue;
                    }
                    if (character == '\n')
                    {
                        break;
                    }
                    // Quotes inside a `{...}` part belong to that part.
                    if (character == '{' && Peek(1) != '{')
                    {
                        ++braces;
                    }
                    else if (character == '{')
                    {
                        Advance(2);
                        continue;
                    }
                    if (character == '}' && braces > 0)
                    {
                        --braces;
                    }
                    if (character == '"' && braces == 0)
                    {
                        token.Spelling = std::string(Source.substr(start, Position - start));
                        Advance();
                        return true;
                    }
                    Advance();
                }
                Report(token.Where, "this text starts with a quote but the line ends before the closing quote");
                token.Spelling = std::string(Source.substr(start, Position - start));
                return true;
            }

            std::string_view Source;
            std::vector<Problem>& Problems;
            std::size_t Position = 0;
            int Line = 1;
            int Column = 1;
            bool LineStart = true;
        };
    }

    bool IsKeyword(std::string_view word)
    {
        for (std::string_view keyword : Keywords)
        {
            if (keyword == word)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<Token> Tokenize(std::string_view source, std::vector<Problem>& problems)
    {
        // Many Windows editors start UTF-8 files with a byte order mark.
        if (source.starts_with("\xEF\xBB\xBF"))
        {
            source.remove_prefix(3);
        }
        return Reader(source, problems).Read();
    }
}
