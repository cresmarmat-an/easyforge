#include <easyforge/core/Testing.h>
#include <easyforge/core/language/Syntax.h>

using namespace easyforge;
using namespace easyforge::language;

namespace
{
    std::vector<Token> Read(std::string_view source)
    {
        std::vector<Problem> problems;
        std::vector<Token> tokens = Tokenize(source, problems);
        for (const Problem& problem : problems)
        {
            testing::ReportFailure(__FILE__, __LINE__, "unexpected problem: " + problem.Message);
        }
        return tokens;
    }

    // An expression written back out with parentheses, to check how it was grouped.
    std::string Shape(const Expression& expression)
    {
        switch (expression.Kind)
        {
        case ExpressionKind::Number: return std::format("{}", expression.Number);
        case ExpressionKind::Name: return expression.Name;
        case ExpressionKind::Boolean: return expression.Boolean ? "true" : "false";
        case ExpressionKind::Nothing: return "nothing";
        case ExpressionKind::ColorCode: return expression.Name;
        case ExpressionKind::Unary: return "(" + expression.Name + " " + Shape(*expression.Parts[0]) + ")";
        case ExpressionKind::Binary:
            return "(" + Shape(*expression.Parts[0]) + " " + expression.Name + " " + Shape(*expression.Parts[1]) + ")";
        case ExpressionKind::Member: return Shape(*expression.Parts[0]) + "." + expression.Name;
        case ExpressionKind::Index: return Shape(*expression.Parts[0]) + "[" + Shape(*expression.Parts[1]) + "]";
        case ExpressionKind::Call:
        {
            std::string text = Shape(*expression.Parts[0]) + "(";
            for (std::size_t index = 1; index < expression.Parts.size(); ++index)
            {
                text += (index > 1 ? ", " : "") + Shape(*expression.Parts[index]);
            }
            return text + ")";
        }
        case ExpressionKind::Text:
        {
            std::string text = "\"";
            for (std::size_t index = 0; index < expression.Pieces.size(); ++index)
            {
                text += expression.Pieces[index];
                if (index < expression.Parts.size())
                {
                    text += "{" + Shape(*expression.Parts[index]) + "}";
                }
            }
            return text + "\"";
        }
        default: return "?";
        }
    }

    std::string ShapeOf(std::string_view source)
    {
        SyntaxTree tree = Parse(std::string("variable x = ") + std::string(source));
        if (!tree || tree.Statements.size() != 1 || tree.Statements[0]->Expressions.empty())
        {
            return "problem: " + (tree.Problems.empty() ? std::string("no value") : tree.Problems[0].Message);
        }
        return Shape(*tree.Statements[0]->Expressions[0]);
    }
}

EASYFORGE_TEST(TokensOfEveryKind)
{
    std::vector<Token> tokens = Read("function Name(value2) returns string then\n    x += 0.5 * 1e3 - 0xFF\nend \"hi\" #FF8000");
    std::vector<TokenKind> kinds;
    std::vector<std::string> spellings;
    for (const Token& token : tokens)
    {
        kinds.push_back(token.Kind);
        spellings.push_back(token.Spelling);
    }
    std::vector<std::string> expected = { "function", "Name", "(", "value2", ")", "returns", "string", "then", "x", "+=",
        "0.5", "*", "1e3", "-", "0xFF", "end", "hi", "#FF8000", "" };
    EASYFORGE_EXPECT(spellings == expected);
    EASYFORGE_EXPECT(tokens[0].Kind == TokenKind::Keyword);
    EASYFORGE_EXPECT(tokens[1].Kind == TokenKind::Name);
    EASYFORGE_EXPECT(tokens[3].Kind == TokenKind::Name);
    EASYFORGE_EXPECT(tokens[9].Kind == TokenKind::Symbol);
    EASYFORGE_EXPECT(tokens[12].Kind == TokenKind::Number);
    EASYFORGE_EXPECT(tokens[16].Kind == TokenKind::Text);
    EASYFORGE_EXPECT(tokens[17].Kind == TokenKind::ColorCode);
    EASYFORGE_EXPECT(tokens.back().Kind == TokenKind::EndOfFile);

    // Lines and columns count from 1.
    EASYFORGE_EXPECT(tokens[8].Where == Location(2, 5));
    EASYFORGE_EXPECT(tokens[8].StartsLine);
    EASYFORGE_EXPECT(!tokens[9].StartsLine);
    EASYFORGE_EXPECT(tokens[15].Where == Location(3, 1));
}

EASYFORGE_TEST(CommentsAreLeftOut)
{
    std::vector<Token> tokens = Read("-- a whole line\nconstant speed --[[ in metres\nper second ]] = 4 -- the rest");
    std::vector<std::string> spellings;
    for (const Token& token : tokens)
    {
        spellings.push_back(token.Spelling);
    }
    EASYFORGE_EXPECT(spellings == std::vector<std::string>({ "constant", "speed", "=", "4", "" }));
    EASYFORGE_EXPECT(tokens[2].Where == Location(3, 15));
}

EASYFORGE_TEST(TokenProblemsSayWhere)
{
    std::vector<Problem> problems;
    Tokenize("x = 1 @ 2\ny = \"open\n--[[ never closed", problems);
    EASYFORGE_REQUIRE(problems.size() == 3);
    EASYFORGE_EXPECT(problems[0].Where == Location(1, 7));
    EASYFORGE_EXPECT(problems[1].Message.find("closing quote") != std::string::npos);
    EASYFORGE_EXPECT(problems[2].Message.find("]]") != std::string::npos);
    EASYFORGE_EXPECT_EQUAL(Describe(problems[0], "game.script"), std::string("game.script:1:7: '@' is not something the language understands here"));

    problems.clear();
    Tokenize("#FFF #12345", problems);
    EASYFORGE_EXPECT_EQUAL(problems.size(), std::size_t { 2 });
}

EASYFORGE_TEST(TheFirstExampleParses)
{
    SyntaxTree tree = Parse(R"(function SomeName(parameter) returns string then
    print("Output: {parameter}")
    return "Output: {parameter}"
end

SomeName("Hello")
)");
    EASYFORGE_REQUIRE(tree);
    EASYFORGE_REQUIRE(tree.Statements.size() == 2);
    const Statement& function = *tree.Statements[0];
    EASYFORGE_EXPECT(function.Kind == StatementKind::Function);
    EASYFORGE_EXPECT_EQUAL(function.Name, std::string("SomeName"));
    EASYFORGE_REQUIRE(function.Parameters.size() == 1);
    EASYFORGE_EXPECT_EQUAL(function.Parameters[0].Name, std::string("parameter"));
    EASYFORGE_EXPECT(!function.Parameters[0].Type);
    EASYFORGE_EXPECT_EQUAL(function.Type->Spelling(), std::string("string"));
    EASYFORGE_REQUIRE(function.Blocks.size() == 1 && function.Blocks[0].size() == 2);
    EASYFORGE_EXPECT_EQUAL(Shape(*function.Blocks[0][0]->Expressions[0]), std::string("print(\"Output: {parameter}\")"));
    EASYFORGE_EXPECT(function.Blocks[0][1]->Kind == StatementKind::Return);

    const Statement& call = *tree.Statements[1];
    EASYFORGE_EXPECT(call.Kind == StatementKind::Expression);
    EASYFORGE_EXPECT_EQUAL(Shape(*call.Expressions[0]), std::string("SomeName(\"Hello\")"));
    EASYFORGE_EXPECT(call.Where == Location(6, 1));
}

EASYFORGE_TEST(ControlFlowParses)
{
    SyntaxTree tree = Parse(R"(
variable count = 0
constant limit = 10

if count < limit then
    count = count + 1
else if count == limit then
    print("full")
else
    print("over")
end

while count < limit then
    count += 1
end

for index in 1 to 10 then
    print(index)
end

for item in items then
    print(item)
end

type Point
    X: number
    Y: number
end

function Distance(first: Point, second: Point) returns number then
    return SquareRoot((second.X - first.X) ^ 2 + (second.Y - first.Y) ^ 2)
end

try
    Risky()
catch problem then
    print("failed: {problem}")
end

import "enemies"
spawn Blink(3)
wait 0.5
yield
)");
    EASYFORGE_REQUIRE(tree);
    std::vector<StatementKind> kinds;
    for (const auto& statement : tree.Statements)
    {
        kinds.push_back(statement->Kind);
    }
    std::vector<StatementKind> expected = { StatementKind::Variable, StatementKind::Constant, StatementKind::If,
        StatementKind::While, StatementKind::For, StatementKind::For, StatementKind::Type, StatementKind::Function,
        StatementKind::Try, StatementKind::Import, StatementKind::Spawn, StatementKind::Wait, StatementKind::YieldControl };
    EASYFORGE_EXPECT(kinds == expected);

    const Statement& branches = *tree.Statements[2];
    EASYFORGE_EXPECT_EQUAL(branches.Expressions.size(), std::size_t { 2 });
    EASYFORGE_EXPECT_EQUAL(branches.Blocks.size(), std::size_t { 3 });

    const Statement& range = *tree.Statements[4];
    EASYFORGE_EXPECT_EQUAL(range.Name, std::string("index"));
    EASYFORGE_EXPECT_EQUAL(range.Expressions.size(), std::size_t { 2 });
    EASYFORGE_EXPECT_EQUAL(tree.Statements[5]->Expressions.size(), std::size_t { 1 });

    const Statement& point = *tree.Statements[6];
    EASYFORGE_REQUIRE(point.Parameters.size() == 2);
    EASYFORGE_EXPECT_EQUAL(point.Parameters[1].Type->Spelling(), std::string("number"));

    const Statement& distance = *tree.Statements[7];
    EASYFORGE_EXPECT_EQUAL(distance.Parameters[0].Type->Spelling(), std::string("Point"));

    const Statement& attempt = *tree.Statements[8];
    EASYFORGE_EXPECT_EQUAL(attempt.Name, std::string("problem"));
    EASYFORGE_EXPECT_EQUAL(attempt.Blocks.size(), std::size_t { 2 });
    EASYFORGE_EXPECT_EQUAL(tree.Statements[9]->Name, std::string("enemies"));
}

EASYFORGE_TEST(OperatorsGroupAsExpected)
{
    EASYFORGE_EXPECT_EQUAL(ShapeOf("1 + 2 * 3"), std::string("(1 + (2 * 3))"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("1 - 2 - 3"), std::string("((1 - 2) - 3)"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("-2 ^ 2"), std::string("(- (2 ^ 2))"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("2 ^ 3 ^ 2"), std::string("(2 ^ (3 ^ 2))"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("not a and b or c"), std::string("(((not a) and b) or c)"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("a + 1 < b * 2"), std::string("((a + 1) < (b * 2))"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("(1 + 2) * 3"), std::string("((1 + 2) * 3)"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("input.Position.X * 20"), std::string("(input.Position.X * 20)"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("items[1].Name"), std::string("items[1].Name"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("Sample(input.Content, input.Coordinates + vector2(0, wave * 0.01)) * Tint"),
        std::string("(Sample(input.Content, (input.Coordinates + vector2(0, (wave * 0.01)))) * Tint)"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("#FF8000"), std::string("#FF8000"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("true"), std::string("true"));
    EASYFORGE_EXPECT_EQUAL(ShapeOf("0xFF"), std::string("255"));
    EASYFORGE_EXPECT(ShapeOf("1 < 2 < 3").find("chained") != std::string::npos);
}

EASYFORGE_TEST(TextPartsAndEscapes)
{
    EASYFORGE_EXPECT_EQUAL(ShapeOf(R"("Score: {score}!")"), std::string("\"Score: {score}!\""));
    EASYFORGE_EXPECT_EQUAL(ShapeOf(R"("{{literal}} braces")"), std::string("\"{literal} braces\""));
    EASYFORGE_EXPECT_EQUAL(ShapeOf(R"("a {Name("x")} b")"), std::string("\"a {Name(\"x\")} b\""));
    EASYFORGE_EXPECT_EQUAL(ShapeOf(R"("{1 + 2}{3}")"), std::string("\"{(1 + 2)}{3}\""));

    SyntaxTree tree = Parse(R"(variable x = "line\n\t\"quoted\"\\")");
    EASYFORGE_REQUIRE(tree);
    EASYFORGE_EXPECT_EQUAL(tree.Statements[0]->Expressions[0]->Pieces[0], std::string("line\n\t\"quoted\"\\"));

    // A problem inside a part is placed where it is in the file.
    tree = Parse(R"(variable x = "total: {1 + }")");
    EASYFORGE_REQUIRE(!tree.Problems.empty());
    EASYFORGE_EXPECT_EQUAL(tree.Problems[0].Where.Line, 1);
    EASYFORGE_EXPECT_EQUAL(tree.Problems[0].Where.Column, 27);
}

EASYFORGE_TEST(AssignmentsAndCalls)
{
    SyntaxTree tree = Parse("x = 1\nx += 2\npoint.X = 3\nitems[1] = 4\nDo()\nvariable a = f\n(g)()");
    EASYFORGE_REQUIRE(tree);
    EASYFORGE_REQUIRE(tree.Statements.size() == 7);
    EASYFORGE_EXPECT(tree.Statements[0]->Kind == StatementKind::Assignment);
    EASYFORGE_EXPECT_EQUAL(tree.Statements[1]->Name, std::string("+="));
    EASYFORGE_EXPECT(tree.Statements[4]->Kind == StatementKind::Expression);
    // A call has to start on the same line as what it calls.
    EASYFORGE_EXPECT_EQUAL(Shape(*tree.Statements[5]->Expressions[0]), std::string("f"));
    EASYFORGE_EXPECT_EQUAL(Shape(*tree.Statements[6]->Expressions[0]), std::string("g()"));

    EASYFORGE_EXPECT(!Parse("1 = 2"));
    EASYFORGE_EXPECT(Parse("x + 1").Problems[0].Message.find("does nothing") != std::string::npos);
}

EASYFORGE_TEST(ProblemsAreFoundAndReadingGoesOn)
{
    SyntaxTree tree = Parse(R"(if ready then
    Go()

function Broken(
    x = 1
variable end = 2
constant limit
)");
    EASYFORGE_EXPECT(!tree);
    EASYFORGE_REQUIRE(tree.Problems.size() >= 3);
    bool keyword = false;
    bool constant = false;
    for (const Problem& problem : tree.Problems)
    {
        keyword = keyword || problem.Message.find("'end' is a word the language keeps") != std::string::npos;
        constant = constant || problem.Message.find("needs a value") != std::string::npos;
    }
    EASYFORGE_EXPECT(keyword);
    EASYFORGE_EXPECT(constant);

    SyntaxTree unclosed = Parse("while true then\n    Step()\n");
    EASYFORGE_REQUIRE(!unclosed.Problems.empty());
    EASYFORGE_EXPECT(unclosed.Problems.back().Message.find("close the while that starts on line 1") != std::string::npos);

    SyntaxTree stray = Parse("Go()\nend\n");
    EASYFORGE_REQUIRE(!stray.Problems.empty());
    EASYFORGE_EXPECT(stray.Problems[0].Message.find("does not close anything") != std::string::npos);
}

EASYFORGE_TEST(ShaderSourceParses)
{
    SyntaxTree tree = Parse(R"(-- ripple.shader
value Speed: number = 1
value Tint: color = #FFFFFF

function Pixel(input: PixelInput) returns color then
    constant wave = Sine(input.Position.X * 20 + input.Time * Speed)
    return Sample(input.Content, input.Coordinates + vector2(0, wave * 0.01)) * Tint
end
)");
    EASYFORGE_REQUIRE(tree);
    EASYFORGE_REQUIRE(tree.Statements.size() == 3);
    EASYFORGE_EXPECT(tree.Statements[0]->Kind == StatementKind::Value);
    EASYFORGE_EXPECT_EQUAL(tree.Statements[1]->Type->Spelling(), std::string("color"));
    EASYFORGE_EXPECT_EQUAL(tree.Statements[2]->Parameters[0].Type->Spelling(), std::string("PixelInput"));

    SyntaxTree lists = Parse("variable scores: list of number = [1, 2, 3]\nvariable spot = { X = 1, Y = 2 }");
    EASYFORGE_REQUIRE(lists);
    EASYFORGE_EXPECT_EQUAL(lists.Statements[0]->Type->Spelling(), std::string("list of number"));
    EASYFORGE_EXPECT_EQUAL(lists.Statements[0]->Expressions[0]->Parts.size(), std::size_t { 3 });
    EASYFORGE_EXPECT(lists.Statements[1]->Expressions[0]->Pieces == std::vector<std::string>({ "X", "Y" }));
}
