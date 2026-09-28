#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/language/Tokens.h>

namespace easyforge::language
{
    struct Statement;

    // A type written in the source: `number`, `vector2`, `Point`, or
    // `list of number`, where Element is the type after `of`.
    struct TypeName
    {
        std::string Name;
        std::unique_ptr<TypeName> Element;
        Location Where;

        // The type as written, such as "list of number".
        std::string Spelling() const;
    };

    enum class ExpressionKind
    {
        Number,
        Text,
        ColorCode,
        Boolean,
        Nothing,
        Name,
        Unary,
        Binary,
        Call,
        Member,
        Index,
        List,
        Table,
        Function,
    };

    // One node of an expression. Which members are set depends on the kind:
    //
    //   Number     Number
    //   Text       Pieces holds the literal text around each `{...}` part, one
    //              more piece than there are Parts; Parts holds the parts
    //   ColorCode  Name holds the code, such as "#FF8000"
    //   Boolean    Boolean
    //   Name       Name
    //   Unary      Name holds the operator (`-` or `not`); Parts holds the operand
    //   Binary     Name holds the operator; Parts holds the left and right sides
    //   Call       Parts holds what is called, then the arguments
    //   Member     Parts holds the object; Name holds the member
    //   Index      Parts holds the object and the index
    //   List       Parts holds the items
    //   Table      Pieces holds the field names; Parts holds their values
    //   Function   Function holds the function, with an empty name
    struct Expression
    {
        Expression();
        Expression(Expression&&) noexcept;
        Expression& operator=(Expression&&) noexcept;
        ~Expression();

        ExpressionKind Kind = ExpressionKind::Nothing;
        Location Where;
        double Number = 0.0;
        bool Boolean = false;
        std::string Name;
        std::vector<std::string> Pieces;
        std::vector<std::unique_ptr<Expression>> Parts;
        std::unique_ptr<Statement> Function;
    };

    struct Parameter
    {
        std::string Name;
        std::unique_ptr<TypeName> Type;
        Location Where;
    };

    using Block = std::vector<std::unique_ptr<Statement>>;

    enum class StatementKind
    {
        Variable,
        Constant,
        Value,
        Function,
        If,
        While,
        For,
        Return,
        Break,
        Continue,
        Type,
        Import,
        Try,
        Spawn,
        Wait,
        YieldControl,
        Assignment,
        Expression,
    };

    // One statement. Which members are set depends on the kind:
    //
    //   Variable, Constant, Value  Name, Type (may be empty), Expressions holds
    //                              the starting value, if there is one
    //   Function                   Name, Parameters, Type holds what it returns,
    //                              Blocks holds the body
    //   If                         Expressions holds each condition; Blocks holds
    //                              each branch's block, then the else block if any
    //   While                      Expressions holds the condition; Blocks the body
    //   For                        Name is the loop variable; Expressions holds
    //                              what it goes through, or the start and the end
    //                              of a `to` range; Blocks the body
    //   Return                     Expressions holds the value, if there is one
    //   Type                       Name; Parameters holds the fields
    //   Import                     Name holds the module path
    //   Try                        Name holds the caught error's name; Blocks
    //                              holds the tried block and the catch block
    //   Spawn                      Expressions holds the call
    //   Wait                       Expressions holds the seconds, if given
    //   Assignment                 Name holds the operator (`=`, `+=`, ...);
    //                              Expressions holds the target and the value
    //   Expression                 Expressions holds a call
    struct Statement
    {
        Statement();
        Statement(Statement&&) noexcept;
        Statement& operator=(Statement&&) noexcept;
        ~Statement();

        StatementKind Kind = StatementKind::Expression;
        Location Where;
        std::string Name;
        std::unique_ptr<TypeName> Type;
        std::vector<Parameter> Parameters;
        std::vector<std::unique_ptr<Expression>> Expressions;
        std::vector<Block> Blocks;
    };

    // A whole source file, read into statements.
    struct SyntaxTree
    {
        Block Statements;

        // Everything that could not be read. The statements hold whatever could.
        std::vector<Problem> Problems;

        explicit operator bool() const { return Problems.empty(); }
    };

    // Reads source text in the easyforge language: the script language, and the
    // shader language, which has the same syntax. The parser accepts both; each
    // language then checks which parts it allows.
    SyntaxTree Parse(std::string_view source);

    // "file.script:3:14: expected then", for showing problems to people.
    std::string Describe(const Problem& problem, std::string_view fileName);
}
