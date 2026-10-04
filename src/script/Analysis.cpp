#include "Analysis.h"

#include <format>

namespace easyforge::internal::scripting
{
    using language::Block;
    using language::Expression;
    using language::ExpressionKind;
    using language::Location;
    using language::Parameter;
    using language::Problem;
    using language::Statement;
    using language::StatementKind;
    using language::TypeName;

    namespace
    {
        TypeInfo Named(std::string name)
        {
            TypeInfo type;
            type.Name = std::move(name);
            return type;
        }

        TypeInfo ListOf(TypeInfo element)
        {
            TypeInfo type = Named("list");
            type.Element = std::make_shared<TypeInfo>(std::move(element));
            return type;
        }

        bool IsBuiltinType(std::string_view name)
        {
            return name == "number" || name == "string" || name == "boolean" || name == "list" || name == "table" ||
                   name == "function" || name == "nothing" || name == "any";
        }

        // "a number", "an object", "nothing", "anything".
        std::string WithArticle(const TypeInfo& type)
        {
            if (type.Name == "nothing")
            {
                return "nothing";
            }
            if (type.Name == "any")
            {
                return "anything";
            }
            std::string spelling = type.Spelling();
            char first = spelling.empty() ? 'x' : static_cast<char>(std::tolower(static_cast<unsigned char>(spelling[0])));
            bool vowel = first == 'a' || first == 'e' || first == 'i' || first == 'o' || first == 'u';
            return (vowel ? "an " : "a ") + spelling;
        }

        bool Assignable(const TypeInfo& target, const TypeInfo& source)
        {
            if (target.IsAny() || source.IsAny() || source.Name == "nothing")
            {
                return true;
            }
            if (target.Name == source.Name)
            {
                if (target.Element && source.Element)
                {
                    return Assignable(*target.Element, *source.Element);
                }
                return true;
            }
            // A table written out can stand for a record; its fields are not
            // checked against the record's.
            return source.Name == "table" && !IsBuiltinType(target.Name);
        }

        std::string_view Ordinal(std::size_t index)
        {
            static constexpr std::string_view words[] = { "first", "second", "third", "fourth", "fifth", "sixth",
                "seventh", "eighth", "ninth", "tenth" };
            return index < std::size(words) ? words[index] : "next";
        }

        struct Declaration
        {
            const void* Key = nullptr;
            Location Where;
            TypeInfo Type;
            bool Constant = false;
            bool Global = false;
            int FunctionDepth = 0;
            std::shared_ptr<SignatureInfo> Signature;
            std::shared_ptr<RecordInfo> Record;
        };

        class Analyzer
        {
        public:
            Analyzer(const CompileSettings& settings, std::vector<Problem>& problems) : Settings(settings), Problems(problems) {}

            AnalysisResult Run(const Block& statements)
            {
                Scopes.emplace_back();
                Declare(statements);
                for (const std::unique_ptr<Statement>& statement : statements)
                {
                    Check(*statement, true);
                }
                return std::move(Result);
            }

        private:
            struct Scope
            {
                std::unordered_map<std::string, Declaration> Names;
                int FunctionDepth = 0;
            };

            struct FunctionContext
            {
                TypeInfo Returns;
                int Loops = 0;
            };

            void Report(Location where, std::string message) { Problems.push_back({ where, std::move(message) }); }

            // Top-level functions and types are known everywhere in the module, so
            // code above them can call them.
            void Declare(const Block& statements)
            {
                for (const std::unique_ptr<Statement>& statement : statements)
                {
                    if (statement->Kind == StatementKind::Type)
                    {
                        DeclareRecord(*statement, true);
                    }
                }
                for (const std::unique_ptr<Statement>& statement : statements)
                {
                    if (statement->Kind == StatementKind::Function)
                    {
                        Declaration declaration;
                        declaration.Key = statement.get();
                        declaration.Where = statement->Where;
                        declaration.Type = Named("function");
                        declaration.Constant = true;
                        declaration.Global = true;
                        declaration.Signature = SignatureOf(*statement);
                        AddDeclaration(statement->Name, declaration);
                    }
                    else if (statement->Kind == StatementKind::Variable || statement->Kind == StatementKind::Constant ||
                             statement->Kind == StatementKind::Import)
                    {
                        TopLevelNames.insert(NameDeclaredBy(*statement));
                    }
                }
            }

            static std::string NameDeclaredBy(const Statement& statement)
            {
                if (statement.Kind != StatementKind::Import)
                {
                    return statement.Name;
                }
                std::string name = statement.Name;
                std::size_t slash = name.find_last_of("/\\");
                if (slash != std::string::npos)
                {
                    name = name.substr(slash + 1);
                }
                if (name.size() > 7 && name.ends_with(".script"))
                {
                    name.resize(name.size() - 7);
                }
                return name;
            }

            void DeclareRecord(const Statement& statement, bool topLevel)
            {
                auto record = std::make_shared<RecordInfo>();
                record->Name = statement.Name;
                Records[statement.Name] = record;
                for (const Parameter& field : statement.Parameters)
                {
                    record->Fields.emplace_back(field.Name, Resolve(field.Type.get()));
                }
                Declaration declaration;
                declaration.Key = &statement;
                declaration.Where = statement.Where;
                declaration.Type = Named("function");
                declaration.Constant = true;
                declaration.Global = topLevel;
                declaration.Record = record;
                AddDeclaration(statement.Name, declaration);
            }

            std::shared_ptr<SignatureInfo> SignatureOf(const Statement& function)
            {
                auto signature = std::make_shared<SignatureInfo>();
                signature->Name = function.Name;
                for (const Parameter& parameter : function.Parameters)
                {
                    signature->Parameters.push_back(Resolve(parameter.Type.get()));
                }
                signature->Returns = Resolve(function.Type.get());
                return signature;
            }

            TypeInfo Resolve(const TypeName* written)
            {
                if (!written)
                {
                    return {};
                }
                TypeInfo type = Named(written->Name);
                if (!IsBuiltinType(written->Name) && !Records.contains(written->Name))
                {
                    Report(written->Where, std::format("there is no type named {}", written->Name));
                    return {};
                }
                if (written->Element)
                {
                    if (written->Name != "list")
                    {
                        Report(written->Where, std::format("only list can say what it holds with of, not {}", written->Name));
                        return type;
                    }
                    return ListOf(Resolve(written->Element.get()));
                }
                return type;
            }

            void AddDeclaration(const std::string& name, Declaration declaration)
            {
                Scope& scope = Scopes.back();
                declaration.FunctionDepth = FunctionDepth;
                auto found = scope.Names.find(name);
                if (found != scope.Names.end() && found->second.Key != declaration.Key)
                {
                    Report(declaration.Where,
                        std::format("{} is already declared here, on line {}", name, found->second.Where.Line));
                    return;
                }
                scope.Names[name] = std::move(declaration);
            }

            Declaration* Find(const std::string& name)
            {
                for (std::size_t index = Scopes.size(); index-- > 0;)
                {
                    auto found = Scopes[index].Names.find(name);
                    if (found == Scopes[index].Names.end())
                    {
                        continue;
                    }
                    Declaration& declaration = found->second;
                    if (!declaration.Global && declaration.FunctionDepth < FunctionDepth)
                    {
                        Result.Captured.insert(declaration.Key);
                    }
                    return &declaration;
                }
                return nullptr;
            }

            bool Known(const std::string& name) const
            {
                return TopLevelNames.contains(name) || (Settings.IsDefined && Settings.IsDefined(name));
            }

            void PushScope()
            {
                Scopes.emplace_back();
                Scopes.back().FunctionDepth = FunctionDepth;
            }

            void PopScope() { Scopes.pop_back(); }

            void CheckBlock(const Block& block)
            {
                PushScope();
                for (const std::unique_ptr<Statement>& statement : block)
                {
                    Check(*statement, false);
                }
                PopScope();
            }

            void ExpectNumber(const Expression& expression, std::string_view purpose)
            {
                TypeInfo type = TypeOf(expression);
                if (!Assignable(Named("number"), type))
                {
                    Report(expression.Where, std::format("{} needs a number, but this is {}", purpose, WithArticle(type)));
                }
            }

            void CheckFunction(const Statement& function)
            {
                ++FunctionDepth;
                Functions.push_back({ Resolve(function.Type.get()), 0 });
                PushScope();
                for (const Parameter& parameter : function.Parameters)
                {
                    Declaration declaration;
                    declaration.Key = &parameter;
                    declaration.Where = parameter.Where;
                    declaration.Type = Resolve(parameter.Type.get());
                    AddDeclaration(parameter.Name, declaration);
                }
                for (const std::unique_ptr<Statement>& statement : function.Blocks[0])
                {
                    Check(*statement, false);
                }
                PopScope();
                Functions.pop_back();
                --FunctionDepth;
            }

            void Check(const Statement& statement, bool topLevel)
            {
                switch (statement.Kind)
                {
                case StatementKind::Variable:
                case StatementKind::Constant:
                {
                    TypeInfo written = Resolve(statement.Type.get());
                    TypeInfo value = statement.Expressions.empty() ? Named("nothing") : TypeOf(*statement.Expressions[0]);
                    if (statement.Type && !statement.Expressions.empty() && !Assignable(written, value))
                    {
                        Report(statement.Expressions[0]->Where,
                            std::format("{} is {}, but this is {}", statement.Name, WithArticle(written), WithArticle(value)));
                    }
                    Declaration declaration;
                    declaration.Key = &statement;
                    declaration.Where = statement.Where;
                    declaration.Constant = statement.Kind == StatementKind::Constant;
                    declaration.Global = topLevel;
                    // A constant's type is known from its value; a variable without a
                    // written type may hold anything later.
                    declaration.Type = statement.Type ? written : declaration.Constant ? value : TypeInfo {};
                    AddDeclaration(statement.Name, declaration);
                    return;
                }
                case StatementKind::Value: Report(statement.Where, "value is for shaders; in a script, use variable or constant"); return;
                case StatementKind::Function:
                {
                    if (!topLevel)
                    {
                        Declaration declaration;
                        declaration.Key = &statement;
                        declaration.Where = statement.Where;
                        declaration.Type = Named("function");
                        declaration.Constant = true;
                        declaration.Signature = SignatureOf(statement);
                        AddDeclaration(statement.Name, declaration);
                    }
                    CheckFunction(statement);
                    return;
                }
                case StatementKind::If:
                    for (const std::unique_ptr<Expression>& condition : statement.Expressions)
                    {
                        TypeOf(*condition);
                    }
                    for (const Block& block : statement.Blocks)
                    {
                        CheckBlock(block);
                    }
                    return;
                case StatementKind::While:
                    TypeOf(*statement.Expressions[0]);
                    CheckLoopBody(statement.Blocks[0], nullptr, {}, nullptr);
                    return;
                case StatementKind::For:
                {
                    TypeInfo item;
                    if (statement.Expressions.size() == 2)
                    {
                        ExpectNumber(*statement.Expressions[0], "for ... to");
                        ExpectNumber(*statement.Expressions[1], "for ... to");
                        item = Named("number");
                    }
                    else
                    {
                        TypeInfo collection = TypeOf(*statement.Expressions[0]);
                        if (collection.Name == "list" && collection.Element)
                        {
                            item = *collection.Element;
                        }
                        else if (collection.Name == "string" || collection.Name == "table")
                        {
                            item = Named("string");
                        }
                        else if (collection.Name == "number" || collection.Name == "boolean" || collection.Name == "function")
                        {
                            Report(statement.Expressions[0]->Where,
                                std::format("for goes through a list, a table, or a string, but this is {}", WithArticle(collection)));
                        }
                    }
                    CheckLoopBody(statement.Blocks[0], &statement.Name, item, &statement);
                    return;
                }
                case StatementKind::Return:
                {
                    if (Functions.empty())
                    {
                        if (!statement.Expressions.empty())
                        {
                            TypeOf(*statement.Expressions[0]);
                        }
                        return;
                    }
                    // A copy: checking the value may analyze a function inside, which
                    // adds to Functions.
                    TypeInfo wanted = Functions.back().Returns;
                    if (statement.Expressions.empty())
                    {
                        if (!wanted.IsAny() && wanted.Name != "nothing")
                        {
                            Report(statement.Where, std::format("this function returns {}, but this return gives nothing",
                                                        WithArticle(wanted)));
                        }
                        return;
                    }
                    TypeInfo given = TypeOf(*statement.Expressions[0]);
                    if (!Assignable(wanted, given))
                    {
                        Report(statement.Expressions[0]->Where,
                            std::format("this function returns {}, but this is {}", WithArticle(wanted), WithArticle(given)));
                    }
                    return;
                }
                case StatementKind::Break:
                case StatementKind::Continue:
                    if (Functions.empty() ? TopLevelLoops == 0 : Functions.back().Loops == 0)
                    {
                        Report(statement.Where, std::format("{} only works inside a loop",
                                                    statement.Kind == StatementKind::Break ? "break" : "continue"));
                    }
                    return;
                case StatementKind::Type:
                    if (!topLevel)
                    {
                        DeclareRecord(statement, false);
                    }
                    return;
                case StatementKind::Import:
                {
                    Declaration declaration;
                    declaration.Key = &statement;
                    declaration.Where = statement.Where;
                    declaration.Constant = true;
                    declaration.Global = topLevel;
                    declaration.Type = Named("table");
                    AddDeclaration(NameDeclaredBy(statement), declaration);
                    return;
                }
                case StatementKind::Try:
                    CheckBlock(statement.Blocks[0]);
                    PushScope();
                    {
                        Declaration declaration;
                        declaration.Key = &statement.Name;
                        declaration.Where = statement.Where;
                        declaration.Type = Named("string");
                        AddDeclaration(statement.Name, declaration);
                        for (const std::unique_ptr<Statement>& inner : statement.Blocks[1])
                        {
                            Check(*inner, false);
                        }
                    }
                    PopScope();
                    return;
                case StatementKind::Spawn: TypeOf(*statement.Expressions[0]); return;
                case StatementKind::Wait:
                    if (!statement.Expressions.empty())
                    {
                        ExpectNumber(*statement.Expressions[0], "wait");
                    }
                    return;
                case StatementKind::YieldControl: return;
                case StatementKind::Assignment: CheckAssignment(statement); return;
                case StatementKind::Expression: TypeOf(*statement.Expressions[0]); return;
                }
            }

            void CheckLoopBody(const Block& body, const std::string* variable, const TypeInfo& variableType, const void* key)
            {
                (Functions.empty() ? TopLevelLoops : Functions.back().Loops)++;
                PushScope();
                if (variable)
                {
                    Declaration declaration;
                    declaration.Key = variable;
                    declaration.Where = static_cast<const Statement*>(key)->Where;
                    declaration.Type = variableType;
                    AddDeclaration(*variable, declaration);
                }
                for (const std::unique_ptr<Statement>& statement : body)
                {
                    Check(*statement, false);
                }
                PopScope();
                (Functions.empty() ? TopLevelLoops : Functions.back().Loops)--;
            }

            void CheckAssignment(const Statement& statement)
            {
                const Expression& target = *statement.Expressions[0];
                TypeInfo value = TypeOf(*statement.Expressions[1]);
                bool compound = statement.Name != "=";
                if (target.Kind == ExpressionKind::Name)
                {
                    Declaration* declaration = Find(target.Name);
                    if (!declaration)
                    {
                        if (!Known(target.Name))
                        {
                            Report(target.Where, std::format("there is no {} to assign to; declare it with variable {} = ...",
                                                     target.Name, target.Name));
                        }
                        return;
                    }
                    if (declaration->Constant)
                    {
                        Report(target.Where, std::format("{} is a constant and cannot be changed", target.Name));
                        return;
                    }
                    if (compound)
                    {
                        CheckArithmetic(statement.Name.substr(0, 1), declaration->Type, value, statement.Expressions[1]->Where);
                    }
                    else if (!Assignable(declaration->Type, value))
                    {
                        Report(statement.Expressions[1]->Where,
                            std::format("{} is {}, but this is {}", target.Name, WithArticle(declaration->Type), WithArticle(value)));
                    }
                    return;
                }
                TypeInfo field = TypeOf(target);
                if (compound)
                {
                    CheckArithmetic(statement.Name.substr(0, 1), field, value, statement.Expressions[1]->Where);
                }
                else if (!Assignable(field, value))
                {
                    Report(statement.Expressions[1]->Where,
                        std::format("this is {}, but it is given {}", WithArticle(field), WithArticle(value)));
                }
            }

            TypeInfo CheckArithmetic(const std::string& symbol, const TypeInfo& left, const TypeInfo& right, Location where)
            {
                if (symbol == "+")
                {
                    if (left.Name == "string" && right.Name == "string")
                    {
                        return Named("string");
                    }
                    if ((left.Name == "string" && right.Name == "number") || (left.Name == "number" && right.Name == "string"))
                    {
                        Report(where, "a string and a number cannot be added; to join them, write \"{first}{second}\"");
                        return {};
                    }
                    if (left.Name == "number" && right.Name == "number")
                    {
                        return Named("number");
                    }
                    if (!left.IsAny() && left.Name != "nothing" && left.Name != "number" && left.Name != "string")
                    {
                        Report(where, std::format("{} cannot be added to", WithArticle(left)));
                    }
                    else if (!right.IsAny() && right.Name != "nothing" && right.Name != "number" && right.Name != "string")
                    {
                        Report(where, std::format("{} cannot be added", WithArticle(right)));
                    }
                    return {};
                }
                for (const TypeInfo* side : { &left, &right })
                {
                    if (!Assignable(Named("number"), *side))
                    {
                        Report(where, std::format("{} works on numbers, but this is {}", symbol, WithArticle(*side)));
                        return Named("number");
                    }
                }
                return Named("number");
            }

            TypeInfo TypeOf(const Expression& expression)
            {
                switch (expression.Kind)
                {
                case ExpressionKind::Number: return Named("number");
                case ExpressionKind::Text:
                    for (const std::unique_ptr<Expression>& part : expression.Parts)
                    {
                        TypeOf(*part);
                    }
                    return Named("string");
                case ExpressionKind::ColorCode:
                {
                    std::size_t digits = expression.Name.size() - 1;
                    if (digits != 6 && digits != 8 && digits != 3)
                    {
                        Report(expression.Where, std::format("{} is not a color; write #RRGGBB or #RRGGBBAA", expression.Name));
                    }
                    return Named("table");
                }
                case ExpressionKind::Boolean: return Named("boolean");
                case ExpressionKind::Nothing: return Named("nothing");
                case ExpressionKind::Name:
                {
                    Declaration* declaration = Find(expression.Name);
                    if (declaration)
                    {
                        return declaration->Type;
                    }
                    if (Settings.ReportUnknownNames && !Known(expression.Name))
                    {
                        Report(expression.Where, std::format("there is no {}", expression.Name));
                    }
                    return {};
                }
                case ExpressionKind::Unary:
                    if (expression.Name == "not")
                    {
                        TypeOf(*expression.Parts[0]);
                        return Named("boolean");
                    }
                    ExpectNumber(*expression.Parts[0], "-");
                    return Named("number");
                case ExpressionKind::Binary: return TypeOfBinary(expression);
                case ExpressionKind::Call: return TypeOfCall(expression);
                case ExpressionKind::Member:
                {
                    TypeInfo object = TypeOf(*expression.Parts[0]);
                    auto record = Records.find(object.Name);
                    if (record != Records.end())
                    {
                        for (const auto& [name, type] : record->second->Fields)
                        {
                            if (name == expression.Name)
                            {
                                return type;
                            }
                        }
                        Report(expression.Where, std::format("{} has no field {}", WithArticle(object), expression.Name));
                    }
                    else if (object.Name == "number" || object.Name == "boolean" || object.Name == "nothing")
                    {
                        Report(expression.Where, std::format("{} has no fields, so it has no {}", WithArticle(object), expression.Name));
                    }
                    return {};
                }
                case ExpressionKind::Index:
                {
                    TypeInfo object = TypeOf(*expression.Parts[0]);
                    TypeInfo index = TypeOf(*expression.Parts[1]);
                    if (object.Name == "list")
                    {
                        if (!Assignable(Named("number"), index))
                        {
                            Report(expression.Parts[1]->Where,
                                std::format("a list's items are numbered, but this is {}", WithArticle(index)));
                        }
                        return object.Element ? *object.Element : TypeInfo {};
                    }
                    return {};
                }
                case ExpressionKind::List:
                {
                    std::vector<TypeInfo> items;
                    for (const std::unique_ptr<Expression>& item : expression.Parts)
                    {
                        items.push_back(TypeOf(*item));
                    }
                    if (items.empty())
                    {
                        return Named("list");
                    }
                    for (const TypeInfo& item : items)
                    {
                        if (item.IsAny() || item.Spelling() != items[0].Spelling())
                        {
                            return Named("list");
                        }
                    }
                    return ListOf(items[0]);
                }
                case ExpressionKind::Table:
                    for (const std::unique_ptr<Expression>& value : expression.Parts)
                    {
                        TypeOf(*value);
                    }
                    return Named("table");
                case ExpressionKind::Function:
                    CheckFunction(*expression.Function);
                    return Named("function");
                }
                return {};
            }

            TypeInfo TypeOfBinary(const Expression& expression)
            {
                const std::string& symbol = expression.Name;
                TypeInfo left = TypeOf(*expression.Parts[0]);
                TypeInfo right = TypeOf(*expression.Parts[1]);
                if (symbol == "and" || symbol == "or")
                {
                    return left.Name == "boolean" && right.Name == "boolean" ? Named("boolean") : TypeInfo {};
                }
                if (symbol == "==" || symbol == "!=")
                {
                    return Named("boolean");
                }
                if (symbol == "<" || symbol == ">" || symbol == "<=" || symbol == ">=")
                {
                    bool leftKnown = left.Name == "number" || left.Name == "string";
                    bool rightKnown = right.Name == "number" || right.Name == "string";
                    if (leftKnown && rightKnown && left.Name != right.Name)
                    {
                        Report(expression.Where, std::format("{} and {} cannot be compared", WithArticle(left), WithArticle(right)));
                    }
                    for (const TypeInfo* side : { &left, &right })
                    {
                        if (!side->IsAny() && side->Name != "number" && side->Name != "string")
                        {
                            Report(expression.Where, std::format("{} compares numbers or strings, but this is {}", symbol,
                                                         WithArticle(*side)));
                            break;
                        }
                    }
                    return Named("boolean");
                }
                return CheckArithmetic(symbol, left, right, expression.Where);
            }

            TypeInfo TypeOfCall(const Expression& call)
            {
                const Expression& callee = *call.Parts[0];
                std::vector<TypeInfo> arguments;
                for (std::size_t index = 1; index < call.Parts.size(); ++index)
                {
                    arguments.push_back(TypeOf(*call.Parts[index]));
                }
                if (callee.Kind == ExpressionKind::Member)
                {
                    TypeOf(*callee.Parts[0]);
                    return {};
                }
                if (callee.Kind != ExpressionKind::Name)
                {
                    TypeOf(callee);
                    return {};
                }
                Declaration* declaration = Find(callee.Name);
                if (!declaration)
                {
                    if (Settings.ReportUnknownNames && !Known(callee.Name))
                    {
                        Report(callee.Where, std::format("there is no {}", callee.Name));
                    }
                    return {};
                }
                if (declaration->Record)
                {
                    const RecordInfo& record = *declaration->Record;
                    if (arguments.size() > record.Fields.size())
                    {
                        Report(call.Where, std::format("{} has {} field{}, but was given {} values", record.Name,
                                               record.Fields.size(), record.Fields.size() == 1 ? "" : "s", arguments.size()));
                    }
                    for (std::size_t index = 0; index < arguments.size() && index < record.Fields.size(); ++index)
                    {
                        const auto& [name, type] = record.Fields[index];
                        if (!Assignable(type, arguments[index]))
                        {
                            Report(call.Parts[index + 1]->Where,
                                std::format("{}'s {} is {}, but this is {}", record.Name, name, WithArticle(type),
                                    WithArticle(arguments[index])));
                        }
                    }
                    return Named(record.Name);
                }
                if (declaration->Signature)
                {
                    const SignatureInfo& signature = *declaration->Signature;
                    if (arguments.size() != signature.Parameters.size())
                    {
                        Report(call.Where, std::format("{} takes {} argument{}, but was given {}", signature.Name,
                                               signature.Parameters.size(), signature.Parameters.size() == 1 ? "" : "s",
                                               arguments.size()));
                    }
                    for (std::size_t index = 0; index < arguments.size() && index < signature.Parameters.size(); ++index)
                    {
                        if (!Assignable(signature.Parameters[index], arguments[index]))
                        {
                            Report(call.Parts[index + 1]->Where,
                                std::format("{}'s {} argument should be {}, but this is {}", signature.Name, Ordinal(index),
                                    WithArticle(signature.Parameters[index]), WithArticle(arguments[index])));
                        }
                    }
                    return signature.Returns;
                }
                if (!declaration->Type.IsAny() && declaration->Type.Name != "function")
                {
                    Report(callee.Where, std::format("{} is {}, which cannot be called", callee.Name, WithArticle(declaration->Type)));
                }
                return {};
            }

            const CompileSettings& Settings;
            std::vector<Problem>& Problems;
            AnalysisResult Result;
            std::vector<Scope> Scopes;
            std::vector<FunctionContext> Functions;
            std::unordered_map<std::string, std::shared_ptr<RecordInfo>> Records;
            std::unordered_set<std::string> TopLevelNames;
            int FunctionDepth = 0;
            int TopLevelLoops = 0;
        };
    }

    AnalysisResult Analyze(const Block& statements, const CompileSettings& settings, std::vector<Problem>& problems)
    {
        Analyzer analyzer(settings, problems);
        return analyzer.Run(statements);
    }
}
