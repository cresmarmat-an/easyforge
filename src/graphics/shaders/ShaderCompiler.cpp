#include "ShaderCompiler.h"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>

#include <easyforge/core/Color.h>
#include <easyforge/core/language/Syntax.h>

namespace easyforge::internal
{
    std::string_view Spelling(ShaderType type)
    {
        switch (type)
        {
        case ShaderType::Nothing: return "nothing";
        case ShaderType::Number: return "number";
        case ShaderType::Boolean: return "boolean";
        case ShaderType::Vector2: return "vector2";
        case ShaderType::Vector3: return "vector3";
        case ShaderType::Vector4: return "vector4";
        case ShaderType::Color: return "color";
        case ShaderType::PixelInput: return "PixelInput";
        case ShaderType::Texture: return "texture";
        }
        return "nothing";
    }

    int ComponentCount(ShaderType type)
    {
        switch (type)
        {
        case ShaderType::Number: return 1;
        case ShaderType::Boolean: return 1;
        case ShaderType::Vector2: return 2;
        case ShaderType::Vector3: return 3;
        case ShaderType::Vector4: return 4;
        case ShaderType::Color: return 4;
        default: return 0;
        }
    }

    namespace
    {
        using namespace language;

        bool IsNumeric(ShaderType type)
        {
            return type == ShaderType::Number || type == ShaderType::Vector2 || type == ShaderType::Vector3 ||
                   type == ShaderType::Vector4 || type == ShaderType::Color;
        }

        bool IsVector(ShaderType type)
        {
            return IsNumeric(type) && type != ShaderType::Number;
        }

        std::string HlslType(ShaderType type)
        {
            switch (type)
            {
            case ShaderType::Number: return "float";
            case ShaderType::Boolean: return "bool";
            case ShaderType::Vector2: return "float2";
            case ShaderType::Vector3: return "float3";
            case ShaderType::Vector4: return "float4";
            case ShaderType::Color: return "float4";
            case ShaderType::PixelInput: return "PixelInput";
            default: return "void";
            }
        }

        std::string Literal(double value)
        {
            std::string text = std::format("{:.9g}", value);
            if (text.find_first_of(".e") == std::string::npos && text.find("inf") == std::string::npos)
            {
                text += ".0";
            }
            return text;
        }

        std::optional<ShaderType> TypeFromName(std::string_view name)
        {
            if (name == "number")
            {
                return ShaderType::Number;
            }
            if (name == "boolean")
            {
                return ShaderType::Boolean;
            }
            if (name == "vector2")
            {
                return ShaderType::Vector2;
            }
            if (name == "vector3")
            {
                return ShaderType::Vector3;
            }
            if (name == "vector4")
            {
                return ShaderType::Vector4;
            }
            if (name == "color")
            {
                return ShaderType::Color;
            }
            if (name == "PixelInput")
            {
                return ShaderType::PixelInput;
            }
            return std::nullopt;
        }

        // Built-in functions and how they treat their arguments.
        enum class Rule
        {
            // One number or vector in, the same type out, part by part.
            Each,

            // Two of the same type, or a vector and a number, out the vector's type.
            Pair,

            Clamp,
            Lerp,
            SmoothStep,
            ArcTangent2,
            Length,
            Distance,
            Dot,
            Normalize,
            Cross,
            Sample,
        };

        struct BuiltInFunction
        {
            std::string_view Name;
            std::string_view Code;
            Rule Kind;
        };

        constexpr BuiltInFunction BuiltInFunctions[] = {
            { "Sine", "sin", Rule::Each },
            { "Cosine", "cos", Rule::Each },
            { "Tangent", "tan", Rule::Each },
            { "ArcSine", "asin", Rule::Each },
            { "ArcCosine", "acos", Rule::Each },
            { "ArcTangent", "atan", Rule::Each },
            { "SquareRoot", "sqrt", Rule::Each },
            { "Absolute", "abs", Rule::Each },
            { "Floor", "floor", Rule::Each },
            { "Ceiling", "ceil", Rule::Each },
            { "Fraction", "frac", Rule::Each },
            { "Round", "round", Rule::Each },
            { "Sign", "sign", Rule::Each },
            { "Exponential", "exp", Rule::Each },
            { "Logarithm", "log", Rule::Each },
            { "Radians", "radians", Rule::Each },
            { "Degrees", "degrees", Rule::Each },
            { "Power", "pow", Rule::Pair },
            { "Min", "min", Rule::Pair },
            { "Max", "max", Rule::Pair },
            { "Step", "step", Rule::Pair },
            { "Clamp", "clamp", Rule::Clamp },
            { "Lerp", "lerp", Rule::Lerp },
            { "SmoothStep", "smoothstep", Rule::SmoothStep },
            { "ArcTangent2", "atan2", Rule::ArcTangent2 },
            { "Length", "length", Rule::Length },
            { "Distance", "distance", Rule::Distance },
            { "Dot", "dot", Rule::Dot },
            { "Normalize", "normalize", Rule::Normalize },
            { "Cross", "cross", Rule::Cross },
            { "Sample", "SampleContent", Rule::Sample },
        };

        const BuiltInFunction* FindBuiltIn(std::string_view name)
        {
            for (const BuiltInFunction& function : BuiltInFunctions)
            {
                if (function.Name == name)
                {
                    return &function;
                }
            }
            return nullptr;
        }

        // What an expression turned into: its HLSL code and its type. An invalid
        // result has already been reported, so nothing using it reports again.
        struct Typed
        {
            std::string Code;
            ShaderType Type = ShaderType::Nothing;
            bool Valid = true;
        };

        Typed Invalid()
        {
            return { "0.0", ShaderType::Nothing, false };
        }

        struct Variable
        {
            ShaderType Type = ShaderType::Nothing;
            std::string Code;
            bool Assignable = false;

            // For a `value`: true, since the program changes it, so a constant
            // cannot be made from it.
            bool Changes = false;
        };

        struct FunctionInfo
        {
            const Statement* Declaration = nullptr;
            std::vector<ShaderType> Parameters;
            ShaderType Returns = ShaderType::Nothing;
            std::string Code;
            std::set<std::string> Calls;
        };

        // Turns a number into the given vector type, when it needs to be.
        std::string Widen(const Typed& value, ShaderType type)
        {
            if (value.Type == type || type == ShaderType::Number)
            {
                return value.Code;
            }
            return std::format("(({})({}))", HlslType(type), value.Code);
        }

        bool AlwaysReturns(const Block& block)
        {
            if (block.empty())
            {
                return false;
            }
            const Statement& last = *block.back();
            if (last.Kind == StatementKind::Return)
            {
                return true;
            }
            if (last.Kind == StatementKind::If && last.Blocks.size() > last.Expressions.size())
            {
                return std::all_of(last.Blocks.begin(), last.Blocks.end(), [](const Block& branch) { return AlwaysReturns(branch); });
            }
            return false;
        }

        class Compiler
        {
        public:
            explicit Compiler(std::string_view name) : Name(name) {}

            Result<CompiledShader> Compile(std::string_view source)
            {
                SyntaxTree tree = Parse(source);
                Problems = tree.Problems;
                if (Problems.empty())
                {
                    DeclareTopLevel(tree.Statements);
                }
                if (Problems.empty())
                {
                    for (auto& [name, function] : Functions)
                    {
                        CompileFunction(name, function);
                    }
                }
                std::vector<std::string> order;
                if (Problems.empty())
                {
                    order = OrderFunctions();
                }
                if (!Problems.empty())
                {
                    std::string message;
                    for (const Problem& problem : Problems)
                    {
                        if (!message.empty())
                        {
                            message += '\n';
                        }
                        message += Describe(problem, Name);
                    }
                    return Failure(message);
                }
                return CompiledShader { Assemble(order), Values };
            }

        private:
            void Report(Location where, std::string message) { Problems.push_back({ where, std::move(message) }); }

            std::optional<ShaderType> ResolveType(const TypeName& type)
            {
                if (type.Element)
                {
                    Report(type.Where, "shaders have no lists; use number, boolean, vector2, vector3, vector4, or color");
                    return std::nullopt;
                }
                std::optional<ShaderType> resolved = TypeFromName(type.Name);
                if (!resolved)
                {
                    Report(type.Where, std::format("'{}' is not a type shaders have; use number, boolean, vector2, "
                                                   "vector3, vector4, or color",
                                           type.Name));
                }
                return resolved;
            }

            bool NameIsFree(const std::string& name, Location where)
            {
                if (FindBuiltIn(name) || TypeFromName(name))
                {
                    Report(where, std::format("'{}' is already the name of something built in", name));
                    return false;
                }
                if (Globals.contains(name) || Functions.contains(name))
                {
                    Report(where, std::format("'{}' is already declared", name));
                    return false;
                }
                for (const auto& scope : Scopes)
                {
                    if (scope.contains(name))
                    {
                        Report(where, std::format("'{}' is already declared", name));
                        return false;
                    }
                }
                return true;
            }

            // A value's starting value: numbers, colors, and vectors of numbers.
            std::optional<std::array<float, 4>> ConstantOf(const Expression& expression, ShaderType type)
            {
                std::array<float, 4> result {};
                if (expression.Kind == ExpressionKind::Number && type == ShaderType::Number)
                {
                    result[0] = static_cast<float>(expression.Number);
                    return result;
                }
                if (expression.Kind == ExpressionKind::Unary && expression.Name == "-" && type == ShaderType::Number &&
                    expression.Parts[0]->Kind == ExpressionKind::Number)
                {
                    result[0] = -static_cast<float>(expression.Parts[0]->Number);
                    return result;
                }
                if (expression.Kind == ExpressionKind::Boolean && type == ShaderType::Boolean)
                {
                    result[0] = expression.Boolean ? 1.0f : 0.0f;
                    return result;
                }
                if (expression.Kind == ExpressionKind::ColorCode && type == ShaderType::Color && Color::IsHex(expression.Name))
                {
                    Color color = Color::Hex(expression.Name);
                    return std::array<float, 4> { color.Red, color.Green, color.Blue, color.Alpha };
                }
                if (expression.Kind == ExpressionKind::Call && expression.Parts[0]->Kind == ExpressionKind::Name &&
                    TypeFromName(expression.Parts[0]->Name) == type && IsVector(type))
                {
                    std::size_t count = expression.Parts.size() - 1;
                    std::size_t wanted = static_cast<std::size_t>(ComponentCount(type));
                    bool shortColor = type == ShaderType::Color && count == 3;
                    if (count != wanted && !shortColor && count != 1)
                    {
                        return std::nullopt;
                    }
                    for (std::size_t index = 0; index < wanted; ++index)
                    {
                        std::size_t source = count == 1 ? 1 : index + 1;
                        if (shortColor && index == 3)
                        {
                            result[3] = 1.0f;
                            continue;
                        }
                        std::optional<std::array<float, 4>> part = ConstantOf(*expression.Parts[source], ShaderType::Number);
                        if (!part)
                        {
                            return std::nullopt;
                        }
                        result[index] = (*part)[0];
                    }
                    return result;
                }
                return std::nullopt;
            }

            void DeclareTopLevel(const Block& statements)
            {
                for (const auto& statement : statements)
                {
                    switch (statement->Kind)
                    {
                    case StatementKind::Value: DeclareValue(*statement); break;
                    case StatementKind::Constant: DeclareGlobalConstant(*statement); break;
                    case StatementKind::Function: DeclareFunction(*statement); break;
                    case StatementKind::Variable:
                        Report(statement->Where, "a shader cannot have variables outside its functions; use value for "
                                                 "something the program sets, or constant");
                        break;
                    default:
                        Report(statement->Where, "only value, constant, and function can be at the top of a shader");
                        break;
                    }
                }
                auto pixel = Functions.find("Pixel");
                if (pixel == Functions.end())
                {
                    Report({ 1, 1 }, "a shader needs a Pixel function: function Pixel(input: PixelInput) returns color then");
                    return;
                }
                const FunctionInfo& info = pixel->second;
                if (info.Parameters != std::vector<ShaderType> { ShaderType::PixelInput } || info.Returns != ShaderType::Color)
                {
                    Report(info.Declaration->Where,
                        "Pixel must take one PixelInput and return a color: function Pixel(input: PixelInput) returns color then");
                }
            }

            void DeclareValue(const Statement& statement)
            {
                if (!NameIsFree(statement.Name, statement.Where))
                {
                    return;
                }
                ShaderType type = ShaderType::Number;
                if (statement.Type)
                {
                    std::optional<ShaderType> resolved = ResolveType(*statement.Type);
                    if (!resolved)
                    {
                        return;
                    }
                    type = *resolved;
                }
                else
                {
                    const Expression& start = *statement.Expressions[0];
                    type = start.Kind == ExpressionKind::ColorCode   ? ShaderType::Color
                           : start.Kind == ExpressionKind::Boolean   ? ShaderType::Boolean
                           : start.Kind == ExpressionKind::Call && start.Parts[0]->Kind == ExpressionKind::Name &&
                                   TypeFromName(start.Parts[0]->Name)
                               ? *TypeFromName(start.Parts[0]->Name)
                               : ShaderType::Number;
                }
                if (type == ShaderType::PixelInput)
                {
                    Report(statement.Where, "a value cannot be a PixelInput");
                    return;
                }

                ShaderValueSlot slot;
                slot.Name = statement.Name;
                slot.Type = type;
                if (!statement.Expressions.empty())
                {
                    std::optional<std::array<float, 4>> start = ConstantOf(*statement.Expressions[0], type);
                    if (!start)
                    {
                        Report(statement.Expressions[0]->Where,
                            std::format("the starting value of {} must be a {} written out, such as {}", statement.Name,
                                Spelling(type),
                                type == ShaderType::Color     ? "#FFFFFF"
                                : type == ShaderType::Boolean ? "true"
                                : type == ShaderType::Number  ? "1"
                                                              : std::string(Spelling(type)) + "(0, 1)"));
                        return;
                    }
                    slot.Default = *start;
                }
                Values.push_back(slot);

                std::string field = "Value" + statement.Name;
                std::string code = field;
                switch (type)
                {
                case ShaderType::Number: code += ".x"; break;
                case ShaderType::Boolean: code = "(" + field + ".x != 0.0)"; break;
                case ShaderType::Vector2: code += ".xy"; break;
                case ShaderType::Vector3: code += ".xyz"; break;
                default: break;
                }
                Globals[statement.Name] = { type, code, false, true };
            }

            void DeclareGlobalConstant(const Statement& statement)
            {
                if (!NameIsFree(statement.Name, statement.Where))
                {
                    return;
                }
                InGlobalConstant = true;
                Typed value = Emit(*statement.Expressions[0]);
                InGlobalConstant = false;
                if (!value.Valid)
                {
                    return;
                }
                ShaderType type = value.Type;
                if (statement.Type)
                {
                    std::optional<ShaderType> declared = ResolveType(*statement.Type);
                    if (!declared)
                    {
                        return;
                    }
                    if (*declared != type)
                    {
                        Report(statement.Where, std::format("{} is declared as {} but its value is {}", statement.Name,
                                                    Spelling(*declared), Spelling(type)));
                        return;
                    }
                }
                if (!IsNumeric(type) && type != ShaderType::Boolean)
                {
                    Report(statement.Where, std::format("a constant cannot be {}", Spelling(type)));
                    return;
                }
                std::string code = "User" + statement.Name;
                GlobalConstantCode += std::format("static const {} {} = {};\n", HlslType(type), code, value.Code);
                Globals[statement.Name] = { type, code, false, false };
            }

            void DeclareFunction(const Statement& statement)
            {
                if (!NameIsFree(statement.Name, statement.Where))
                {
                    return;
                }
                FunctionInfo info;
                info.Declaration = &statement;
                for (const Parameter& parameter : statement.Parameters)
                {
                    if (!parameter.Type)
                    {
                        Report(parameter.Where, std::format("the parameter {} needs a type, such as {}: number",
                                                    parameter.Name, parameter.Name));
                        return;
                    }
                    std::optional<ShaderType> type = ResolveType(*parameter.Type);
                    if (!type)
                    {
                        return;
                    }
                    info.Parameters.push_back(*type);
                }
                if (statement.Type)
                {
                    std::optional<ShaderType> returns = ResolveType(*statement.Type);
                    if (!returns)
                    {
                        return;
                    }
                    info.Returns = *returns;
                }
                Functions[statement.Name] = std::move(info);
            }

            void CompileFunction(const std::string& name, FunctionInfo& info)
            {
                const Statement& statement = *info.Declaration;
                Current = &info;
                Scopes.assign(1, {});
                std::string parameters;
                for (std::size_t index = 0; index < statement.Parameters.size(); ++index)
                {
                    const Parameter& parameter = statement.Parameters[index];
                    if (!NameIsFree(parameter.Name, parameter.Where))
                    {
                        continue;
                    }
                    std::string code = "User" + parameter.Name;
                    Scopes.back()[parameter.Name] = { info.Parameters[index], code, true, false };
                    if (!parameters.empty())
                    {
                        parameters += ", ";
                    }
                    parameters += HlslType(info.Parameters[index]) + " " + code;
                }
                std::string body = EmitBlock(statement.Blocks[0], 1);
                if (info.Returns != ShaderType::Nothing && !AlwaysReturns(statement.Blocks[0]))
                {
                    Report(statement.Where, std::format("{} must end by returning a {} on every path", name, Spelling(info.Returns)));
                }
                info.Code = std::format("{} User{}({})\n{{\n{}}}\n", HlslType(info.Returns), name, parameters, body);
                Scopes.clear();
                Current = nullptr;
            }

            std::vector<std::string> OrderFunctions()
            {
                // Callees come before their callers, since HLSL reads top to bottom.
                std::vector<std::string> order;
                std::map<std::string, int> state;
                std::function<bool(const std::string&, std::vector<std::string>&)> visit =
                    [&](const std::string& name, std::vector<std::string>& path) {
                        int& mark = state[name];
                        if (mark == 2)
                        {
                            return true;
                        }
                        path.push_back(name);
                        if (mark == 1)
                        {
                            std::string cycle;
                            auto start = std::find(path.begin(), path.end(), name);
                            for (auto step = start; step != path.end(); ++step)
                            {
                                cycle += (step == start ? "" : " calls ") + *step;
                            }
                            Report(Functions[name].Declaration->Where,
                                std::format("shader functions cannot call themselves, directly or through others: {}", cycle));
                            return false;
                        }
                        mark = 1;
                        for (const std::string& callee : Functions[name].Calls)
                        {
                            if (!visit(callee, path))
                            {
                                return false;
                            }
                        }
                        mark = 2;
                        path.pop_back();
                        order.push_back(name);
                        return true;
                    };
                for (const auto& [name, function] : Functions)
                {
                    std::vector<std::string> path;
                    if (!visit(name, path))
                    {
                        break;
                    }
                }
                return order;
            }

            std::string Assemble(const std::vector<std::string>& order)
            {
                std::string code = R"hlsl(cbuffer Shader : register(b0)
{
    float2 TargetSize;
    float2 AreaOrigin;
    float2 AreaSize;
    float2 PointSize;
    float4 Clip;
    float Time;
    float Scale;
    float HasContent;
    float Unused;
)hlsl";
                for (const ShaderValueSlot& value : Values)
                {
                    code += std::format("    float4 Value{};\n", value.Name);
                }
                code += R"hlsl(};

Texture2D Content : register(t0);
SamplerState LinearClamp : register(s0);

struct PixelInput
{
    float2 Position;
    float2 Size;
    float2 Coordinates;
    float Time;
};

// The content holds colors multiplied by alpha; shaders work with plain colors.
float4 SampleContent(float2 coordinates)
{
    float4 color = Content.Sample(LinearClamp, coordinates);
    return color.a > 0.0 ? float4(color.rgb / color.a, color.a) : float4(0.0, 0.0, 0.0, 0.0);
}

)hlsl";
                code += GlobalConstantCode;
                code += '\n';
                for (const std::string& name : order)
                {
                    code += Functions[name].Code;
                    code += '\n';
                }
                code += R"hlsl(struct Interpolated
{
    float4 Position : SV_Position;
    float2 Local : LOCAL;
};

Interpolated VertexMain(uint vertex : SV_VertexID)
{
    float2 corner = float2(vertex & 1, vertex >> 1);
    float2 local = corner * AreaSize;
    float2 pixel = AreaOrigin + local;
    Interpolated output;
    output.Position = float4(pixel / TargetSize * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.Local = local;
    return output;
}

float4 PixelMain(Interpolated input) : SV_Target
{
    float2 pixel = input.Position.xy;
    if (pixel.x < Clip.x || pixel.y < Clip.y || pixel.x >= Clip.z || pixel.y >= Clip.w)
    {
        discard;
    }
    PixelInput shaderInput;
    shaderInput.Position = input.Local / Scale;
    shaderInput.Size = PointSize;
    shaderInput.Coordinates = input.Local / AreaSize;
    shaderInput.Time = Time;
    float4 color = saturate(UserPixel(shaderInput));
    return float4(color.rgb * color.a, color.a);
}
)hlsl";
                return code;
            }

            std::string Indent(int depth) const { return std::string(static_cast<std::size_t>(depth) * 4, ' '); }

            std::string EmitBlock(const Block& block, int depth)
            {
                Scopes.emplace_back();
                std::string code;
                for (const auto& statement : block)
                {
                    code += EmitStatement(*statement, depth);
                }
                Scopes.pop_back();
                return code;
            }

            const Variable* Find(const std::string& name) const
            {
                for (auto scope = Scopes.rbegin(); scope != Scopes.rend(); ++scope)
                {
                    auto found = scope->find(name);
                    if (found != scope->end())
                    {
                        return &found->second;
                    }
                }
                auto global = Globals.find(name);
                return global != Globals.end() ? &global->second : nullptr;
            }

            Typed Condition(const Expression& expression, std::string_view what)
            {
                Typed condition = Emit(expression);
                if (condition.Valid && condition.Type != ShaderType::Boolean)
                {
                    Report(expression.Where, std::format("the condition of {} must be true or false, but it is a {}", what,
                                                 Spelling(condition.Type)));
                    return Invalid();
                }
                return condition;
            }

            std::string EmitStatement(const Statement& statement, int depth)
            {
                std::string indent = Indent(depth);
                switch (statement.Kind)
                {
                case StatementKind::Variable:
                case StatementKind::Constant:
                {
                    bool constant = statement.Kind == StatementKind::Constant;
                    if (statement.Expressions.empty())
                    {
                        Report(statement.Where, std::format("the variable {} needs a starting value", statement.Name));
                        return {};
                    }
                    Typed value = Emit(*statement.Expressions[0]);
                    if (!value.Valid || !NameIsFree(statement.Name, statement.Where))
                    {
                        return {};
                    }
                    ShaderType type = value.Type;
                    if (statement.Type)
                    {
                        std::optional<ShaderType> declared = ResolveType(*statement.Type);
                        if (!declared)
                        {
                            return {};
                        }
                        if (*declared != type)
                        {
                            Report(statement.Expressions[0]->Where, std::format("{} is declared as {} but its value is {}",
                                                                        statement.Name, Spelling(*declared), Spelling(type)));
                            return {};
                        }
                    }
                    if (type == ShaderType::Texture || type == ShaderType::Nothing)
                    {
                        Report(statement.Where, type == ShaderType::Texture
                                                    ? "a texture cannot be kept in a variable; use input.Content where it is sampled"
                                                    : "this has no value to keep");
                        return {};
                    }
                    std::string code = "User" + statement.Name;
                    Scopes.back()[statement.Name] = { type, code, !constant, false };
                    return std::format("{}{}{} {} = {};\n", indent, constant ? "const " : "", HlslType(type), code, value.Code);
                }
                case StatementKind::Assignment:
                {
                    Typed target = EmitTarget(*statement.Expressions[0]);
                    Typed value = Emit(*statement.Expressions[1]);
                    if (!target.Valid || !value.Valid)
                    {
                        return {};
                    }
                    bool widened = statement.Name != "=" && value.Type == ShaderType::Number && IsVector(target.Type);
                    if (value.Type != target.Type && !widened)
                    {
                        Report(statement.Expressions[1]->Where,
                            std::format("cannot assign a {} to a {}", Spelling(value.Type), Spelling(target.Type)));
                        return {};
                    }
                    if (statement.Name != "=" && !IsNumeric(target.Type))
                    {
                        Report(statement.Where, std::format("{} only works with numbers and vectors", statement.Name));
                        return {};
                    }
                    return std::format("{}{} {} {};\n", indent, target.Code, statement.Name, value.Code);
                }
                case StatementKind::If:
                {
                    std::string code;
                    for (std::size_t index = 0; index < statement.Blocks.size(); ++index)
                    {
                        if (index < statement.Expressions.size())
                        {
                            Typed condition = Condition(*statement.Expressions[index], "if");
                            code += std::format("{}{}if ({})\n", index == 0 ? indent : indent + "else ", "", condition.Code);
                        }
                        else
                        {
                            code += indent + "else\n";
                        }
                        code += indent + "{\n" + EmitBlock(statement.Blocks[index], depth + 1) + indent + "}\n";
                    }
                    return code;
                }
                case StatementKind::While:
                {
                    Typed condition = Condition(*statement.Expressions[0], "while");
                    ++LoopDepth;
                    std::string body = EmitBlock(statement.Blocks[0], depth + 1);
                    --LoopDepth;
                    return std::format("{}while ({})\n{}{{\n{}{}}}\n", indent, condition.Code, indent, body, indent);
                }
                case StatementKind::For:
                {
                    if (statement.Expressions.size() != 2)
                    {
                        Report(statement.Where, "shaders loop over a range of numbers: for index in 1 to 10 then");
                        return {};
                    }
                    Typed start = Emit(*statement.Expressions[0]);
                    Typed end = Emit(*statement.Expressions[1]);
                    if (!start.Valid || !end.Valid)
                    {
                        return {};
                    }
                    if (start.Type != ShaderType::Number || end.Type != ShaderType::Number)
                    {
                        Report(statement.Where, "the start and end of a for loop must be numbers");
                        return {};
                    }
                    if (!NameIsFree(statement.Name, statement.Where))
                    {
                        return {};
                    }
                    std::string code = "User" + statement.Name;
                    Scopes.emplace_back();
                    Scopes.back()[statement.Name] = { ShaderType::Number, code, false, false };
                    ++LoopDepth;
                    std::string body = EmitBlock(statement.Blocks[0], depth + 1);
                    --LoopDepth;
                    Scopes.pop_back();
                    return std::format("{}for (float {} = {}; {} <= {}; {} += 1.0)\n{}{{\n{}{}}}\n", indent, code, start.Code,
                        code, end.Code, code, indent, body, indent);
                }
                case StatementKind::Return:
                {
                    if (statement.Expressions.empty())
                    {
                        if (Current->Returns != ShaderType::Nothing)
                        {
                            Report(statement.Where, std::format("this function returns a {}; return needs one",
                                                        Spelling(Current->Returns)));
                        }
                        return indent + "return;\n";
                    }
                    Typed value = Emit(*statement.Expressions[0]);
                    if (!value.Valid)
                    {
                        return {};
                    }
                    if (value.Type != Current->Returns)
                    {
                        Report(statement.Expressions[0]->Where,
                            Current->Returns == ShaderType::Nothing
                                ? std::string("this function returns nothing; add returns and a type to return a value")
                                : std::format("this function returns a {}, but this is a {}", Spelling(Current->Returns),
                                      Spelling(value.Type)));
                        return {};
                    }
                    return std::format("{}return {};\n", indent, value.Code);
                }
                case StatementKind::Break:
                case StatementKind::Continue:
                {
                    std::string word = statement.Kind == StatementKind::Break ? "break" : "continue";
                    if (LoopDepth == 0)
                    {
                        Report(statement.Where, std::format("{} only works inside a loop", word));
                        return {};
                    }
                    return indent + word + ";\n";
                }
                case StatementKind::Expression:
                {
                    Typed value = Emit(*statement.Expressions[0]);
                    return value.Valid ? indent + value.Code + ";\n" : std::string();
                }
                case StatementKind::Value:
                    Report(statement.Where, "value can only be at the top of a shader, outside its functions");
                    return {};
                case StatementKind::Function:
                    Report(statement.Where, "shader functions cannot be inside other functions");
                    return {};
                default:
                    Report(statement.Where, "shaders cannot use this; they have values, constants, variables, if, "
                                            "while, for, and return");
                    return {};
                }
            }

            // Something assigned to: a variable, or a part of a vector variable.
            Typed EmitTarget(const Expression& expression)
            {
                if (expression.Kind == ExpressionKind::Name)
                {
                    const Variable* variable = Find(expression.Name);
                    if (!variable)
                    {
                        Report(expression.Where, std::format("'{}' is not declared", expression.Name));
                        return Invalid();
                    }
                    if (!variable->Assignable)
                    {
                        Report(expression.Where, std::format("'{}' cannot be changed", expression.Name));
                        return Invalid();
                    }
                    return { variable->Code, variable->Type };
                }
                if (expression.Kind == ExpressionKind::Member && expression.Parts[0]->Kind == ExpressionKind::Name)
                {
                    const Variable* variable = Find(expression.Parts[0]->Name);
                    if (variable && !variable->Assignable)
                    {
                        Report(expression.Where, std::format("'{}' cannot be changed", expression.Parts[0]->Name));
                        return Invalid();
                    }
                    return Emit(expression);
                }
                Report(expression.Where, "only a variable or a part of one can be assigned to");
                return Invalid();
            }

            Typed Emit(const Expression& expression)
            {
                switch (expression.Kind)
                {
                case ExpressionKind::Number: return { Literal(expression.Number), ShaderType::Number };
                case ExpressionKind::Boolean: return { expression.Boolean ? "true" : "false", ShaderType::Boolean };
                case ExpressionKind::ColorCode:
                {
                    if (!Color::IsHex(expression.Name))
                    {
                        Report(expression.Where, std::format("'{}' is not a color", expression.Name));
                        return Invalid();
                    }
                    Color color = Color::Hex(expression.Name);
                    return { std::format("float4({}, {}, {}, {})", Literal(color.Red), Literal(color.Green),
                                 Literal(color.Blue), Literal(color.Alpha)),
                        ShaderType::Color };
                }
                case ExpressionKind::Name:
                {
                    const Variable* variable = Find(expression.Name);
                    if (variable)
                    {
                        if (InGlobalConstant && variable->Changes)
                        {
                            Report(expression.Where, std::format("a constant cannot use the value {}, which the program "
                                                                 "can change",
                                                         expression.Name));
                            return Invalid();
                        }
                        return { variable->Code, variable->Type };
                    }
                    if (FindBuiltIn(expression.Name) || Functions.contains(expression.Name))
                    {
                        Report(expression.Where, std::format("{} is a function; call it with ()", expression.Name));
                        return Invalid();
                    }
                    Report(expression.Where, std::format("'{}' is not declared", expression.Name));
                    return Invalid();
                }
                case ExpressionKind::Unary:
                {
                    Typed operand = Emit(*expression.Parts[0]);
                    if (!operand.Valid)
                    {
                        return operand;
                    }
                    if (expression.Name == "not")
                    {
                        if (operand.Type != ShaderType::Boolean)
                        {
                            Report(expression.Where, "not works on true or false");
                            return Invalid();
                        }
                        return { "(!" + operand.Code + ")", ShaderType::Boolean };
                    }
                    if (!IsNumeric(operand.Type))
                    {
                        Report(expression.Where, "a minus sign works on numbers and vectors");
                        return Invalid();
                    }
                    return { "(-" + operand.Code + ")", operand.Type };
                }
                case ExpressionKind::Binary: return EmitBinary(expression);
                case ExpressionKind::Member: return EmitMember(expression);
                case ExpressionKind::Call: return EmitCall(expression);
                case ExpressionKind::Text: Report(expression.Where, "shaders have no text"); return Invalid();
                case ExpressionKind::Nothing: Report(expression.Where, "shaders have no nothing value"); return Invalid();
                case ExpressionKind::List:
                case ExpressionKind::Index: Report(expression.Where, "shaders have no lists"); return Invalid();
                case ExpressionKind::Table: Report(expression.Where, "shaders have no tables"); return Invalid();
                case ExpressionKind::Function:
                    Report(expression.Where, "shader functions have to be declared at the top of the shader");
                    return Invalid();
                }
                return Invalid();
            }

            Typed EmitBinary(const Expression& expression)
            {
                Typed left = Emit(*expression.Parts[0]);
                Typed right = Emit(*expression.Parts[1]);
                if (!left.Valid || !right.Valid)
                {
                    return Invalid();
                }
                const std::string& symbol = expression.Name;
                auto mismatch = [&] {
                    Report(expression.Where, std::format("{} cannot be used between a {} and a {}", symbol,
                                                 Spelling(left.Type), Spelling(right.Type)));
                    return Invalid();
                };

                if (symbol == "and" || symbol == "or")
                {
                    if (left.Type != ShaderType::Boolean || right.Type != ShaderType::Boolean)
                    {
                        return mismatch();
                    }
                    return { std::format("({} {} {})", left.Code, symbol == "and" ? "&&" : "||", right.Code), ShaderType::Boolean };
                }
                if (symbol == "==" || symbol == "!=")
                {
                    bool comparable = left.Type == right.Type && (left.Type == ShaderType::Number || left.Type == ShaderType::Boolean);
                    if (!comparable)
                    {
                        if (IsVector(left.Type) && left.Type == right.Type)
                        {
                            Report(expression.Where, "compare the parts of vectors, such as position.X == 0");
                            return Invalid();
                        }
                        return mismatch();
                    }
                    return { std::format("({} {} {})", left.Code, symbol, right.Code), ShaderType::Boolean };
                }
                if (symbol == "<" || symbol == "<=" || symbol == ">" || symbol == ">=")
                {
                    if (left.Type != ShaderType::Number || right.Type != ShaderType::Number)
                    {
                        if (IsVector(left.Type) || IsVector(right.Type))
                        {
                            Report(expression.Where, "compare the parts of vectors, such as position.X < 10");
                            return Invalid();
                        }
                        return mismatch();
                    }
                    return { std::format("({} {} {})", left.Code, symbol, right.Code), ShaderType::Boolean };
                }

                // Arithmetic: the same types, or a vector with a number.
                if (!IsNumeric(left.Type) || !IsNumeric(right.Type))
                {
                    return mismatch();
                }
                ShaderType result = left.Type;
                if (left.Type != right.Type)
                {
                    if (left.Type == ShaderType::Number)
                    {
                        result = right.Type;
                    }
                    else if (right.Type != ShaderType::Number)
                    {
                        return mismatch();
                    }
                }
                std::string a = Widen(left, result);
                std::string b = Widen(right, result);
                if (symbol == "^")
                {
                    return { std::format("pow({}, {})", a, b), result };
                }
                if (symbol == "%")
                {
                    return { std::format("fmod({}, {})", a, b), result };
                }
                return { std::format("({} {} {})", left.Code, symbol, right.Code), result };
            }

            Typed EmitMember(const Expression& expression)
            {
                Typed object = Emit(*expression.Parts[0]);
                if (!object.Valid)
                {
                    return object;
                }
                const std::string& member = expression.Name;
                if (object.Type == ShaderType::PixelInput)
                {
                    if (member == "Position" || member == "Size" || member == "Coordinates")
                    {
                        return { object.Code + "." + member, ShaderType::Vector2 };
                    }
                    if (member == "Time")
                    {
                        return { object.Code + ".Time", ShaderType::Number };
                    }
                    if (member == "Content")
                    {
                        return { "Content", ShaderType::Texture };
                    }
                    Report(expression.Where, std::format("PixelInput has Position, Size, Coordinates, Time, and "
                                                         "Content, not {}",
                                                 member));
                    return Invalid();
                }
                if (object.Type == ShaderType::Color)
                {
                    static const std::map<std::string, std::string> parts = { { "Red", "r" }, { "Green", "g" },
                        { "Blue", "b" }, { "Alpha", "a" } };
                    auto found = parts.find(member);
                    if (found == parts.end())
                    {
                        Report(expression.Where, std::format("a color has Red, Green, Blue, and Alpha, not {}", member));
                        return Invalid();
                    }
                    return { object.Code + "." + found->second, ShaderType::Number };
                }
                if (IsVector(object.Type))
                {
                    static const std::string names = "XYZW";
                    std::size_t index = member.size() == 1 ? names.find(member[0]) : std::string::npos;
                    int count = ComponentCount(object.Type);
                    if (index == std::string::npos || static_cast<int>(index) >= count)
                    {
                        Report(expression.Where, std::format("a {} has {}, not {}", Spelling(object.Type),
                                                     count == 2 ? "X and Y" : count == 3 ? "X, Y, and Z" : "X, Y, Z, and W", member));
                        return Invalid();
                    }
                    return { object.Code + "." + std::string(1, "xyzw"[index]), ShaderType::Number };
                }
                Report(expression.Where, std::format("a {} has no parts", Spelling(object.Type)));
                return Invalid();
            }

            Typed EmitCall(const Expression& expression)
            {
                if (expression.Parts[0]->Kind != ExpressionKind::Name)
                {
                    Report(expression.Where, "only functions can be called, by name");
                    return Invalid();
                }
                const std::string& name = expression.Parts[0]->Name;
                std::vector<Typed> arguments;
                bool valid = true;
                for (std::size_t index = 1; index < expression.Parts.size(); ++index)
                {
                    arguments.push_back(Emit(*expression.Parts[index]));
                    valid = valid && arguments.back().Valid;
                }
                if (!valid)
                {
                    return Invalid();
                }

                if (std::optional<ShaderType> type = TypeFromName(name))
                {
                    return EmitConstruction(expression, *type, arguments);
                }
                if (const BuiltInFunction* builtIn = FindBuiltIn(name))
                {
                    return EmitBuiltIn(expression, *builtIn, arguments);
                }
                auto function = Functions.find(name);
                if (function == Functions.end())
                {
                    Report(expression.Where, std::format("there is no function called {}", name));
                    return Invalid();
                }
                if (InGlobalConstant)
                {
                    Report(expression.Where, "a constant at the top of a shader cannot call the shader's own functions");
                    return Invalid();
                }
                const FunctionInfo& info = function->second;
                if (arguments.size() != info.Parameters.size())
                {
                    Report(expression.Where, std::format("{} takes {} arguments, but {} were given", name,
                                                 info.Parameters.size(), arguments.size()));
                    return Invalid();
                }
                std::string code = "User" + name + "(";
                for (std::size_t index = 0; index < arguments.size(); ++index)
                {
                    if (arguments[index].Type != info.Parameters[index])
                    {
                        Report(expression.Parts[index + 1]->Where,
                            std::format("{} needs a {} here, but this is a {}", name, Spelling(info.Parameters[index]),
                                Spelling(arguments[index].Type)));
                        return Invalid();
                    }
                    code += (index > 0 ? ", " : "") + arguments[index].Code;
                }
                if (Current)
                {
                    Current->Calls.insert(name);
                }
                return { code + ")", info.Returns };
            }

            Typed EmitConstruction(const Expression& expression, ShaderType type, const std::vector<Typed>& arguments)
            {
                if (!IsVector(type))
                {
                    Report(expression.Where, std::format("{} cannot be made this way", Spelling(type)));
                    return Invalid();
                }
                int wanted = ComponentCount(type);
                std::string name = std::string(Spelling(type));
                // A single number fills every part.
                if (arguments.size() == 1 && arguments[0].Type == ShaderType::Number)
                {
                    return { std::format("(({})({}))", HlslType(type), arguments[0].Code), type };
                }
                // A color and a vector4 turn into each other.
                if (arguments.size() == 1 && ComponentCount(arguments[0].Type) == 4 && wanted == 4)
                {
                    return { arguments[0].Code, type };
                }
                int total = 0;
                std::string code;
                for (const Typed& argument : arguments)
                {
                    if (!IsNumeric(argument.Type))
                    {
                        Report(expression.Where, std::format("{} is made from numbers and vectors, not a {}", name,
                                                     Spelling(argument.Type)));
                        return Invalid();
                    }
                    total += ComponentCount(argument.Type);
                    code += (code.empty() ? "" : ", ") + argument.Code;
                }
                // color(red, green, blue) is opaque.
                if (type == ShaderType::Color && total == 3)
                {
                    code += ", 1.0";
                    total = 4;
                }
                if (total != wanted)
                {
                    Report(expression.Where, std::format("{} needs {} numbers in all, but {} were given", name, wanted, total));
                    return Invalid();
                }
                return { std::format("{}({})", HlslType(type), code), type };
            }

            Typed EmitBuiltIn(const Expression& expression, const BuiltInFunction& function, const std::vector<Typed>& arguments)
            {
                std::string name(function.Name);
                auto count = [&](std::size_t wanted) {
                    if (arguments.size() != wanted)
                    {
                        Report(expression.Where, std::format("{} takes {} argument{}, but {} were given", name, wanted,
                                                     wanted == 1 ? "" : "s", arguments.size()));
                        return false;
                    }
                    return true;
                };
                auto numeric = [&](const Typed& argument) {
                    if (!IsNumeric(argument.Type))
                    {
                        Report(expression.Where, std::format("{} works on numbers and vectors, not a {}", name,
                                                     Spelling(argument.Type)));
                        return false;
                    }
                    return true;
                };
                auto vectorOnly = [&](const Typed& argument) {
                    if (!IsVector(argument.Type))
                    {
                        Report(expression.Where, std::format("{} works on vectors, not a {}", name, Spelling(argument.Type)));
                        return false;
                    }
                    return true;
                };
                // Arguments that must match `type`, or be a number, which fills every part.
                auto fits = [&](const Typed& argument, ShaderType type) {
                    if (argument.Type != type && argument.Type != ShaderType::Number)
                    {
                        Report(expression.Where, std::format("{} needs a {} or a number here, but this is a {}", name,
                                                     Spelling(type), Spelling(argument.Type)));
                        return false;
                    }
                    return true;
                };
                std::string code(function.Code);

                switch (function.Kind)
                {
                case Rule::Each:
                    if (!count(1) || !numeric(arguments[0]))
                    {
                        return Invalid();
                    }
                    return { std::format("{}({})", code, arguments[0].Code), arguments[0].Type };
                case Rule::Pair:
                {
                    if (!count(2) || !numeric(arguments[0]) || !numeric(arguments[1]))
                    {
                        return Invalid();
                    }
                    ShaderType type = arguments[0].Type == ShaderType::Number ? arguments[1].Type : arguments[0].Type;
                    if (!fits(arguments[0], type) || !fits(arguments[1], type))
                    {
                        return Invalid();
                    }
                    return { std::format("{}({}, {})", code, Widen(arguments[0], type), Widen(arguments[1], type)), type };
                }
                case Rule::Clamp:
                {
                    if (!count(3) || !numeric(arguments[0]))
                    {
                        return Invalid();
                    }
                    ShaderType type = arguments[0].Type;
                    if (!fits(arguments[1], type) || !fits(arguments[2], type))
                    {
                        return Invalid();
                    }
                    return { std::format("clamp({}, {}, {})", arguments[0].Code, Widen(arguments[1], type),
                                 Widen(arguments[2], type)),
                        type };
                }
                case Rule::Lerp:
                {
                    if (!count(3) || !numeric(arguments[0]))
                    {
                        return Invalid();
                    }
                    ShaderType type = arguments[0].Type;
                    if (arguments[1].Type != type)
                    {
                        Report(expression.Where, std::format("Lerp blends two of the same type, but these are a {} and a {}",
                                                     Spelling(type), Spelling(arguments[1].Type)));
                        return Invalid();
                    }
                    if (!fits(arguments[2], type))
                    {
                        return Invalid();
                    }
                    return { std::format("lerp({}, {}, {})", arguments[0].Code, arguments[1].Code, Widen(arguments[2], type)),
                        type };
                }
                case Rule::SmoothStep:
                {
                    if (!count(3) || !numeric(arguments[2]))
                    {
                        return Invalid();
                    }
                    ShaderType type = arguments[2].Type;
                    if (!fits(arguments[0], type) || !fits(arguments[1], type))
                    {
                        return Invalid();
                    }
                    return { std::format("smoothstep({}, {}, {})", Widen(arguments[0], type), Widen(arguments[1], type),
                                 arguments[2].Code),
                        type };
                }
                case Rule::ArcTangent2:
                    if (!count(2) || !fits(arguments[0], ShaderType::Number) || !fits(arguments[1], ShaderType::Number))
                    {
                        return Invalid();
                    }
                    return { std::format("atan2({}, {})", arguments[0].Code, arguments[1].Code), ShaderType::Number };
                case Rule::Length:
                case Rule::Normalize:
                    if (!count(1) || !vectorOnly(arguments[0]))
                    {
                        return Invalid();
                    }
                    return { std::format("{}({})", code, arguments[0].Code),
                        function.Kind == Rule::Length ? ShaderType::Number : arguments[0].Type };
                case Rule::Distance:
                case Rule::Dot:
                    if (!count(2) || !vectorOnly(arguments[0]))
                    {
                        return Invalid();
                    }
                    if (arguments[1].Type != arguments[0].Type)
                    {
                        Report(expression.Where, std::format("{} needs two of the same type, but these are a {} and a {}",
                                                     name, Spelling(arguments[0].Type), Spelling(arguments[1].Type)));
                        return Invalid();
                    }
                    return { std::format("{}({}, {})", code, arguments[0].Code, arguments[1].Code), ShaderType::Number };
                case Rule::Cross:
                    if (!count(2) || arguments[0].Type != ShaderType::Vector3 || arguments[1].Type != ShaderType::Vector3)
                    {
                        if (arguments.size() == 2)
                        {
                            Report(expression.Where, "Cross works on two vector3s");
                        }
                        return Invalid();
                    }
                    return { std::format("cross({}, {})", arguments[0].Code, arguments[1].Code), ShaderType::Vector3 };
                case Rule::Sample:
                    if (!count(2))
                    {
                        return Invalid();
                    }
                    if (arguments[0].Type != ShaderType::Texture)
                    {
                        Report(expression.Where, "Sample reads a texture, such as input.Content");
                        return Invalid();
                    }
                    if (arguments[1].Type != ShaderType::Vector2)
                    {
                        Report(expression.Where, std::format("Sample needs a vector2 of where to read, but this is a {}",
                                                     Spelling(arguments[1].Type)));
                        return Invalid();
                    }
                    return { std::format("SampleContent({})", arguments[1].Code), ShaderType::Color };
                }
                return Invalid();
            }

            std::string Name;
            std::vector<Problem> Problems;
            std::vector<ShaderValueSlot> Values;
            std::map<std::string, Variable> Globals;
            std::map<std::string, FunctionInfo> Functions;
            std::vector<std::map<std::string, Variable>> Scopes;
            std::string GlobalConstantCode;
            FunctionInfo* Current = nullptr;
            int LoopDepth = 0;
            bool InGlobalConstant = false;
        };
    }

    Result<CompiledShader> CompileShader(std::string_view source, std::string_view name)
    {
        return Compiler(name).Compile(source);
    }
}
