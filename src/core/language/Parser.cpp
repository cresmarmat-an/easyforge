#include <easyforge/core/language/Syntax.h>

#include <charconv>
#include <cstdlib>
#include <format>

namespace easyforge::language
{
    Expression::Expression() = default;
    Expression::Expression(Expression&&) noexcept = default;
    Expression& Expression::operator=(Expression&&) noexcept = default;
    Expression::~Expression() = default;

    Statement::Statement() = default;
    Statement::Statement(Statement&&) noexcept = default;
    Statement& Statement::operator=(Statement&&) noexcept = default;
    Statement::~Statement() = default;

    std::string TypeName::Spelling() const
    {
        return Element ? Name + " of " + Element->Spelling() : Name;
    }

    std::string Describe(const Problem& problem, std::string_view fileName)
    {
        return std::format("{}:{}:{}: {}", fileName, problem.Where.Line, problem.Where.Column, problem.Message);
    }

    namespace
    {
        // Thrown inside the parser to abandon a statement that cannot be read; the
        // parser catches it, skips to the next line, and carries on.
        struct Abandon
        {
        };

        using ExpressionPointer = std::unique_ptr<Expression>;
        using StatementPointer = std::unique_ptr<Statement>;

        class Parser
        {
        public:
            Parser(std::vector<Token> tokens, std::vector<Problem>& problems)
                : Tokens(std::move(tokens)), Problems(problems)
            {
            }

            Block ParseFile()
            {
                Block statements = ParseBlock();
                if (!At(TokenKind::EndOfFile))
                {
                    Report(Current().Where, std::format("'{}' does not close anything here", Current().Spelling));
                }
                return statements;
            }

            ExpressionPointer ParseLoneExpression()
            {
                ExpressionPointer expression = ParseExpression();
                if (!At(TokenKind::EndOfFile))
                {
                    Report(Current().Where, std::format("unexpected '{}' after the expression", Current().Spelling));
                }
                return expression;
            }

        private:
            const Token& Current() const { return Tokens[Index]; }
            const Token& Previous() const { return Tokens[Index > 0 ? Index - 1 : 0]; }
            bool At(TokenKind kind) const { return Current().Kind == kind; }
            bool AtKeyword(std::string_view word) const { return Current().IsKeyword(word); }
            bool AtSymbol(std::string_view symbol) const { return Current().IsSymbol(symbol); }

            const Token& Take()
            {
                const Token& token = Tokens[Index];
                if (Index + 1 < Tokens.size())
                {
                    ++Index;
                }
                return token;
            }

            bool TakeKeyword(std::string_view word)
            {
                if (AtKeyword(word))
                {
                    Take();
                    return true;
                }
                return false;
            }

            bool TakeSymbol(std::string_view symbol)
            {
                if (AtSymbol(symbol))
                {
                    Take();
                    return true;
                }
                return false;
            }

            void Report(Location where, std::string message) { Problems.push_back({ where, std::move(message) }); }

            [[noreturn]] void Fail(std::string message)
            {
                Report(Current().Where, std::move(message));
                throw Abandon {};
            }

            std::string Found() const
            {
                if (At(TokenKind::EndOfFile))
                {
                    return "the end of the file";
                }
                return std::format("'{}'", Current().Spelling);
            }

            void ExpectKeyword(std::string_view word, std::string_view purpose)
            {
                if (!TakeKeyword(word))
                {
                    Fail(std::format("expected {} {}, but found {}", word, purpose, Found()));
                }
            }

            void ExpectSymbol(std::string_view symbol, std::string_view purpose)
            {
                if (!TakeSymbol(symbol))
                {
                    Fail(std::format("expected '{}' {}, but found {}", symbol, purpose, Found()));
                }
            }

            std::string ExpectName(std::string_view purpose)
            {
                if (At(TokenKind::Keyword))
                {
                    Fail(std::format("'{}' is a word the language keeps for itself and cannot be used as {}",
                        Current().Spelling, purpose));
                }
                if (!At(TokenKind::Name))
                {
                    Fail(std::format("expected {}, but found {}", purpose, Found()));
                }
                return Take().Spelling;
            }

            // True when the current token ends a block.
            bool AtBlockEnd() const
            {
                return At(TokenKind::EndOfFile) || AtKeyword("end") || AtKeyword("else") || AtKeyword("catch");
            }

            // Skips the rest of a statement that could not be read: everything up to
            // the next line.
            void Recover()
            {
                if (!At(TokenKind::EndOfFile))
                {
                    Take();
                }
                while (!At(TokenKind::EndOfFile) && !Current().StartsLine)
                {
                    Take();
                }
            }

            Block ParseBlock()
            {
                Block block;
                while (!AtBlockEnd())
                {
                    std::size_t before = Index;
                    try
                    {
                        block.push_back(ParseStatement());
                    }
                    catch (const Abandon&)
                    {
                        if (Index == before || !Current().StartsLine)
                        {
                            Recover();
                        }
                    }
                }
                return block;
            }

            std::unique_ptr<TypeName> ParseType()
            {
                auto type = std::make_unique<TypeName>();
                type->Where = Current().Where;
                // `nothing` and `function` are keywords, and also type names.
                if (At(TokenKind::Name) || AtKeyword("nothing") || AtKeyword("function"))
                {
                    type->Name = Take().Spelling;
                }
                else
                {
                    Fail(std::format("expected a type, such as number or string, but found {}", Found()));
                }
                if (Current().Is(TokenKind::Name, "of"))
                {
                    Take();
                    type->Element = ParseType();
                }
                return type;
            }

            StatementPointer NewStatement(StatementKind kind, Location where)
            {
                auto statement = std::make_unique<Statement>();
                statement->Kind = kind;
                statement->Where = where;
                return statement;
            }

            StatementPointer ParseStatement()
            {
                Location where = Current().Where;
                if (TakeKeyword("variable"))
                {
                    return ParseDeclaration(StatementKind::Variable, where);
                }
                if (TakeKeyword("constant"))
                {
                    return ParseDeclaration(StatementKind::Constant, where);
                }
                if (TakeKeyword("value"))
                {
                    return ParseDeclaration(StatementKind::Value, where);
                }
                if (AtKeyword("function"))
                {
                    Take();
                    return ParseFunction(where, true);
                }
                if (TakeKeyword("if"))
                {
                    return ParseIf(where);
                }
                if (TakeKeyword("while"))
                {
                    StatementPointer statement = NewStatement(StatementKind::While, where);
                    statement->Expressions.push_back(ParseExpression());
                    ExpectKeyword("then", "after the condition of while");
                    statement->Blocks.push_back(ParseBlock());
                    ExpectKeyword("end", "to close the while that starts on line " + std::to_string(where.Line));
                    return statement;
                }
                if (TakeKeyword("for"))
                {
                    StatementPointer statement = NewStatement(StatementKind::For, where);
                    statement->Name = ExpectName("the name of the loop's variable");
                    ExpectKeyword("in", "after the loop's variable");
                    statement->Expressions.push_back(ParseExpression());
                    if (TakeKeyword("to"))
                    {
                        statement->Expressions.push_back(ParseExpression());
                    }
                    ExpectKeyword("then", "to start the body of for");
                    statement->Blocks.push_back(ParseBlock());
                    ExpectKeyword("end", "to close the for that starts on line " + std::to_string(where.Line));
                    return statement;
                }
                if (TakeKeyword("return"))
                {
                    StatementPointer statement = NewStatement(StatementKind::Return, where);
                    if (!AtBlockEnd() && !Current().StartsLine)
                    {
                        statement->Expressions.push_back(ParseExpression());
                    }
                    return statement;
                }
                if (TakeKeyword("break"))
                {
                    return NewStatement(StatementKind::Break, where);
                }
                if (TakeKeyword("continue"))
                {
                    return NewStatement(StatementKind::Continue, where);
                }
                if (TakeKeyword("yield"))
                {
                    return NewStatement(StatementKind::YieldControl, where);
                }
                if (TakeKeyword("wait"))
                {
                    StatementPointer statement = NewStatement(StatementKind::Wait, where);
                    if (!AtBlockEnd() && !Current().StartsLine)
                    {
                        statement->Expressions.push_back(ParseExpression());
                    }
                    return statement;
                }
                if (TakeKeyword("spawn"))
                {
                    StatementPointer statement = NewStatement(StatementKind::Spawn, where);
                    ExpressionPointer call = ParseExpression();
                    if (call->Kind != ExpressionKind::Call)
                    {
                        Report(call->Where, "spawn needs a function call, such as spawn Blink(3)");
                    }
                    statement->Expressions.push_back(std::move(call));
                    return statement;
                }
                if (TakeKeyword("type"))
                {
                    StatementPointer statement = NewStatement(StatementKind::Type, where);
                    statement->Name = ExpectName("the name of the type");
                    while (!AtKeyword("end") && !At(TokenKind::EndOfFile))
                    {
                        Parameter field;
                        field.Where = Current().Where;
                        field.Name = ExpectName("a field name");
                        ExpectSymbol(":", "between the field's name and its type");
                        field.Type = ParseType();
                        statement->Parameters.push_back(std::move(field));
                    }
                    ExpectKeyword("end", "to close the type that starts on line " + std::to_string(where.Line));
                    return statement;
                }
                if (TakeKeyword("import"))
                {
                    StatementPointer statement = NewStatement(StatementKind::Import, where);
                    if (!At(TokenKind::Text))
                    {
                        Fail(std::format("import needs the module's name in quotes, but found {}", Found()));
                    }
                    statement->Name = Take().Spelling;
                    return statement;
                }
                if (TakeKeyword("try"))
                {
                    StatementPointer statement = NewStatement(StatementKind::Try, where);
                    statement->Blocks.push_back(ParseBlock());
                    ExpectKeyword("catch", "after the block of try");
                    statement->Name = ExpectName("a name for the error after catch");
                    ExpectKeyword("then", "after the error's name");
                    statement->Blocks.push_back(ParseBlock());
                    ExpectKeyword("end", "to close the try that starts on line " + std::to_string(where.Line));
                    return statement;
                }
                if (At(TokenKind::Keyword) && !AtKeyword("not") && !AtKeyword("true") && !AtKeyword("false") &&
                    !AtKeyword("nothing"))
                {
                    Fail(std::format("'{}' cannot start a statement", Current().Spelling));
                }

                ExpressionPointer expression = ParseExpression();
                for (std::string_view assign : { "=", "+=", "-=", "*=", "/=" })
                {
                    if (AtSymbol(assign))
                    {
                        Take();
                        if (expression->Kind != ExpressionKind::Name && expression->Kind != ExpressionKind::Member &&
                            expression->Kind != ExpressionKind::Index)
                        {
                            Report(expression->Where, "only a name, a field, or an item of a list can be assigned to");
                        }
                        StatementPointer statement = NewStatement(StatementKind::Assignment, where);
                        statement->Name = std::string(assign);
                        statement->Expressions.push_back(std::move(expression));
                        statement->Expressions.push_back(ParseExpression());
                        return statement;
                    }
                }
                if (expression->Kind != ExpressionKind::Call)
                {
                    Report(expression->Where, "this does nothing: a line needs to call a function, assign, or declare something");
                }
                StatementPointer statement = NewStatement(StatementKind::Expression, where);
                statement->Expressions.push_back(std::move(expression));
                return statement;
            }

            StatementPointer ParseDeclaration(StatementKind kind, Location where)
            {
                StatementPointer statement = NewStatement(kind, where);
                statement->Name = ExpectName("a name to declare");
                if (TakeSymbol(":"))
                {
                    statement->Type = ParseType();
                }
                if (TakeSymbol("="))
                {
                    statement->Expressions.push_back(ParseExpression());
                }
                else if (kind == StatementKind::Constant)
                {
                    Fail(std::format("the constant {} needs a value: constant {} = ...", statement->Name, statement->Name));
                }
                else if (kind == StatementKind::Value && !statement->Type)
                {
                    Fail(std::format("the value {} needs a type or a starting value", statement->Name));
                }
                return statement;
            }

            StatementPointer ParseFunction(Location where, bool named)
            {
                StatementPointer statement = NewStatement(StatementKind::Function, where);
                if (named)
                {
                    statement->Name = ExpectName("the function's name");
                }
                ExpectSymbol("(", "to start the function's parameters");
                if (!AtSymbol(")"))
                {
                    do
                    {
                        Parameter parameter;
                        parameter.Where = Current().Where;
                        parameter.Name = ExpectName("a parameter name");
                        if (TakeSymbol(":"))
                        {
                            parameter.Type = ParseType();
                        }
                        statement->Parameters.push_back(std::move(parameter));
                    } while (TakeSymbol(","));
                }
                ExpectSymbol(")", "to end the function's parameters");
                if (TakeKeyword("returns"))
                {
                    statement->Type = ParseType();
                }
                ExpectKeyword("then", "to start the function's body");
                statement->Blocks.push_back(ParseBlock());
                ExpectKeyword("end", "to close the function that starts on line " + std::to_string(where.Line));
                return statement;
            }

            StatementPointer ParseIf(Location where)
            {
                StatementPointer statement = NewStatement(StatementKind::If, where);
                statement->Expressions.push_back(ParseExpression());
                ExpectKeyword("then", "after the condition of if");
                statement->Blocks.push_back(ParseBlock());
                while (AtKeyword("else"))
                {
                    Take();
                    if (TakeKeyword("if"))
                    {
                        statement->Expressions.push_back(ParseExpression());
                        ExpectKeyword("then", "after the condition of else if");
                        statement->Blocks.push_back(ParseBlock());
                        continue;
                    }
                    statement->Blocks.push_back(ParseBlock());
                    break;
                }
                ExpectKeyword("end", "to close the if that starts on line " + std::to_string(where.Line));
                return statement;
            }

            ExpressionPointer NewExpression(ExpressionKind kind, Location where)
            {
                auto expression = std::make_unique<Expression>();
                expression->Kind = kind;
                expression->Where = where;
                return expression;
            }

            ExpressionPointer Binary(std::string symbol, ExpressionPointer left, ExpressionPointer right)
            {
                ExpressionPointer expression = NewExpression(ExpressionKind::Binary, left->Where);
                expression->Name = std::move(symbol);
                expression->Parts.push_back(std::move(left));
                expression->Parts.push_back(std::move(right));
                return expression;
            }

            ExpressionPointer ParseExpression() { return ParseOr(); }

            ExpressionPointer ParseOr()
            {
                ExpressionPointer left = ParseAnd();
                while (TakeKeyword("or"))
                {
                    left = Binary("or", std::move(left), ParseAnd());
                }
                return left;
            }

            ExpressionPointer ParseAnd()
            {
                ExpressionPointer left = ParseNot();
                while (TakeKeyword("and"))
                {
                    left = Binary("and", std::move(left), ParseNot());
                }
                return left;
            }

            ExpressionPointer ParseNot()
            {
                if (AtKeyword("not"))
                {
                    Location where = Take().Where;
                    ExpressionPointer expression = NewExpression(ExpressionKind::Unary, where);
                    expression->Name = "not";
                    expression->Parts.push_back(ParseNot());
                    return expression;
                }
                return ParseComparison();
            }

            ExpressionPointer ParseComparison()
            {
                ExpressionPointer left = ParseAdditive();
                for (std::string_view symbol : { "==", "!=", "<=", ">=", "<", ">" })
                {
                    if (AtSymbol(symbol))
                    {
                        Take();
                        left = Binary(std::string(symbol), std::move(left), ParseAdditive());
                        for (std::string_view again : { "==", "!=", "<=", ">=", "<", ">" })
                        {
                            if (AtSymbol(again))
                            {
                                Fail("comparisons cannot be chained; join them with and");
                            }
                        }
                        break;
                    }
                }
                return left;
            }

            ExpressionPointer ParseAdditive()
            {
                ExpressionPointer left = ParseMultiplicative();
                while (AtSymbol("+") || AtSymbol("-"))
                {
                    std::string symbol = Take().Spelling;
                    left = Binary(symbol, std::move(left), ParseMultiplicative());
                }
                return left;
            }

            ExpressionPointer ParseMultiplicative()
            {
                ExpressionPointer left = ParseNegation();
                while (AtSymbol("*") || AtSymbol("/") || AtSymbol("%"))
                {
                    std::string symbol = Take().Spelling;
                    left = Binary(symbol, std::move(left), ParseNegation());
                }
                return left;
            }

            ExpressionPointer ParseNegation()
            {
                if (AtSymbol("-"))
                {
                    Location where = Take().Where;
                    ExpressionPointer expression = NewExpression(ExpressionKind::Unary, where);
                    expression->Name = "-";
                    expression->Parts.push_back(ParseNegation());
                    return expression;
                }
                return ParsePower();
            }

            // `^` binds tighter than a leading minus and groups to the right, so
            // -2 ^ 2 is -4 and 2 ^ 3 ^ 2 is 2 ^ 9.
            ExpressionPointer ParsePower()
            {
                ExpressionPointer base = ParsePostfix();
                if (TakeSymbol("^"))
                {
                    return Binary("^", std::move(base), ParseNegation());
                }
                return base;
            }

            ExpressionPointer ParsePostfix()
            {
                ExpressionPointer expression = ParsePrimary();
                while (true)
                {
                    // A call or index has to start on the same line, so a line that
                    // begins with ( or [ is never taken as part of the one before.
                    if (AtSymbol("(") && !Current().StartsLine)
                    {
                        Take();
                        ExpressionPointer call = NewExpression(ExpressionKind::Call, expression->Where);
                        call->Parts.push_back(std::move(expression));
                        if (!AtSymbol(")"))
                        {
                            do
                            {
                                call->Parts.push_back(ParseExpression());
                            } while (TakeSymbol(","));
                        }
                        ExpectSymbol(")", "to end the call's arguments");
                        expression = std::move(call);
                        continue;
                    }
                    if (AtSymbol("[") && !Current().StartsLine)
                    {
                        Take();
                        ExpressionPointer index = NewExpression(ExpressionKind::Index, expression->Where);
                        index->Parts.push_back(std::move(expression));
                        index->Parts.push_back(ParseExpression());
                        ExpectSymbol("]", "to end the index");
                        expression = std::move(index);
                        continue;
                    }
                    if (AtSymbol("."))
                    {
                        Take();
                        ExpressionPointer member = NewExpression(ExpressionKind::Member, expression->Where);
                        member->Parts.push_back(std::move(expression));
                        member->Name = ExpectName("a field name after the dot");
                        expression = std::move(member);
                        continue;
                    }
                    return expression;
                }
            }

            ExpressionPointer ParsePrimary()
            {
                const Token& token = Current();
                Location where = token.Where;
                switch (token.Kind)
                {
                case TokenKind::Number:
                {
                    ExpressionPointer expression = NewExpression(ExpressionKind::Number, where);
                    expression->Number = ReadNumber(token);
                    Take();
                    return expression;
                }
                case TokenKind::Text:
                {
                    ExpressionPointer expression = ReadText(token);
                    Take();
                    return expression;
                }
                case TokenKind::ColorCode:
                {
                    ExpressionPointer expression = NewExpression(ExpressionKind::ColorCode, where);
                    expression->Name = Take().Spelling;
                    return expression;
                }
                case TokenKind::Name:
                {
                    ExpressionPointer expression = NewExpression(ExpressionKind::Name, where);
                    expression->Name = Take().Spelling;
                    return expression;
                }
                default:
                    break;
                }

                if (AtKeyword("true") || AtKeyword("false"))
                {
                    ExpressionPointer expression = NewExpression(ExpressionKind::Boolean, where);
                    expression->Boolean = Take().Spelling == "true";
                    return expression;
                }
                if (TakeKeyword("nothing"))
                {
                    return NewExpression(ExpressionKind::Nothing, where);
                }
                if (TakeKeyword("function"))
                {
                    ExpressionPointer expression = NewExpression(ExpressionKind::Function, where);
                    expression->Function = ParseFunction(where, false);
                    return expression;
                }
                if (TakeSymbol("("))
                {
                    ExpressionPointer inner = ParseExpression();
                    ExpectSymbol(")", "to close the parenthesis");
                    return inner;
                }
                if (TakeSymbol("["))
                {
                    ExpressionPointer list = NewExpression(ExpressionKind::List, where);
                    if (!AtSymbol("]"))
                    {
                        do
                        {
                            if (AtSymbol("]"))
                            {
                                break;
                            }
                            list->Parts.push_back(ParseExpression());
                        } while (TakeSymbol(","));
                    }
                    ExpectSymbol("]", "to end the list");
                    return list;
                }
                if (TakeSymbol("{"))
                {
                    ExpressionPointer table = NewExpression(ExpressionKind::Table, where);
                    if (!AtSymbol("}"))
                    {
                        do
                        {
                            if (AtSymbol("}"))
                            {
                                break;
                            }
                            table->Pieces.push_back(ExpectName("a field name"));
                            ExpectSymbol("=", "between the field's name and its value");
                            table->Parts.push_back(ParseExpression());
                        } while (TakeSymbol(","));
                    }
                    ExpectSymbol("}", "to end the table");
                    return table;
                }
                Fail(std::format("expected a value, such as a number, a name, or text, but found {}", Found()));
            }

            double ReadNumber(const Token& token)
            {
                const std::string& text = token.Spelling;
                if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
                {
                    std::uint64_t value = 0;
                    std::from_chars(text.data() + 2, text.data() + text.size(), value, 16);
                    return static_cast<double>(value);
                }
                double value = 0.0;
                std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
                if (result.ec != std::errc())
                {
                    Report(token.Where, std::format("'{}' is too large a number", text));
                }
                return value;
            }

            // Splits text into literal pieces and `{...}` parts, reading each part
            // as an expression. `{{` and `}}` are literal braces.
            ExpressionPointer ReadText(const Token& token)
            {
                ExpressionPointer expression = NewExpression(ExpressionKind::Text, token.Where);
                const std::string& raw = token.Spelling;
                std::string piece;
                std::size_t position = 0;
                auto columnOf = [&](std::size_t offset) { return token.Where.Column + 1 + static_cast<int>(offset); };
                while (position < raw.size())
                {
                    char character = raw[position];
                    if (character == '\\' && position + 1 < raw.size())
                    {
                        char escaped = raw[position + 1];
                        switch (escaped)
                        {
                        case 'n': piece += '\n'; break;
                        case 't': piece += '\t'; break;
                        case 'r': piece += '\r'; break;
                        case '"': piece += '"'; break;
                        case '\\': piece += '\\'; break;
                        default:
                            Report({ token.Where.Line, columnOf(position) },
                                std::format("\\{} is not an escape the language knows; use \\n, \\t, \\\", or \\\\", escaped));
                            piece += escaped;
                            break;
                        }
                        position += 2;
                        continue;
                    }
                    if (character == '{' && position + 1 < raw.size() && raw[position + 1] == '{')
                    {
                        piece += '{';
                        position += 2;
                        continue;
                    }
                    if (character == '}' && position + 1 < raw.size() && raw[position + 1] == '}')
                    {
                        piece += '}';
                        position += 2;
                        continue;
                    }
                    if (character == '{')
                    {
                        std::size_t start = position + 1;
                        std::size_t end = start;
                        int depth = 1;
                        bool quoted = false;
                        while (end < raw.size() && depth > 0)
                        {
                            if (raw[end] == '"')
                            {
                                quoted = !quoted;
                            }
                            else if (!quoted && raw[end] == '{')
                            {
                                ++depth;
                            }
                            else if (!quoted && raw[end] == '}')
                            {
                                --depth;
                                if (depth == 0)
                                {
                                    break;
                                }
                            }
                            ++end;
                        }
                        if (depth != 0)
                        {
                            Report({ token.Where.Line, columnOf(position) }, "this { is never closed; write {{ for a brace");
                            piece += raw.substr(position);
                            break;
                        }
                        expression->Pieces.push_back(std::move(piece));
                        piece.clear();
                        expression->Parts.push_back(ParseInner(std::string_view(raw).substr(start, end - start),
                            { token.Where.Line, columnOf(start) }));
                        position = end + 1;
                        continue;
                    }
                    if (character == '}')
                    {
                        Report({ token.Where.Line, columnOf(position) }, "this } closes nothing; write }} for a brace");
                    }
                    piece += character;
                    ++position;
                }
                expression->Pieces.push_back(std::move(piece));
                return expression;
            }

            // Reads the expression inside a `{...}` part of text, placing its
            // problems where the part is in the file.
            ExpressionPointer ParseInner(std::string_view text, Location start)
            {
                auto place = [&](Location where) { return Location { start.Line, start.Column + where.Column - 1 }; };
                std::vector<Problem> tokenizing;
                std::vector<Token> tokens = Tokenize(text, tokenizing);
                for (Problem& problem : tokenizing)
                {
                    Problems.push_back({ place(problem.Where), std::move(problem.Message) });
                }
                for (Token& token : tokens)
                {
                    token.StartsLine = false;
                    token.Where = place(token.Where);
                }
                Parser parser(std::move(tokens), Problems);
                try
                {
                    return parser.ParseLoneExpression();
                }
                catch (const Abandon&)
                {
                    return NewExpression(ExpressionKind::Nothing, start);
                }
            }

            std::vector<Token> Tokens;
            std::size_t Index = 0;
            std::vector<Problem>& Problems;
        };
    }

    SyntaxTree Parse(std::string_view source)
    {
        SyntaxTree tree;
        std::vector<Token> tokens = Tokenize(source, tree.Problems);
        Parser parser(std::move(tokens), tree.Problems);
        tree.Statements = parser.ParseFile();
        return tree;
    }
}
