#include "Compiler.h"

#include <bit>
#include <format>
#include <unordered_map>

#include "Analysis.h"

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

    namespace
    {
        // Registers a function may use; instructions name them in a byte.
        constexpr int MostRegisters = 250;

        struct Local
        {
            std::string Name;
            int Register = 0;
            bool Boxed = false;
            const void* Key = nullptr;
        };

        struct LoopContext
        {
            std::vector<std::size_t> Breaks;
            std::vector<std::size_t> Continues;
            int TryDepth = 0;
        };

        struct FunctionState
        {
            FunctionState* Enclosing = nullptr;
            Prototype* Code = nullptr;
            bool TopLevel = false;
            std::vector<Local> Locals;
            std::vector<std::size_t> ScopeStarts;
            int FreeRegister = 0;
            std::vector<const void*> CaptureKeys;
            std::vector<LoopContext> Loops;
            int TryDepth = 0;
            std::unordered_map<std::uint64_t, std::uint16_t> Numbers;
            std::unordered_map<TextObject*, std::uint16_t> Texts;
        };

        // Where a name's value is kept.
        struct Place
        {
            enum class Kind
            {
                Register,
                Box,
                Capture,
                Global,
            };
            Kind Type = Kind::Global;
            int Index = 0;
        };

        std::string ModuleNameOf(const std::string& path)
        {
            std::string name = path;
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

        class Emitter
        {
        public:
            Emitter(Heap& heap, const AnalysisResult& analysis, std::vector<Problem>& problems)
                : TheHeap(heap), Analysis(analysis), Problems(problems)
            {
            }

            std::unique_ptr<Prototype> CompileMain(const Block& statements)
            {
                auto main = std::make_unique<Prototype>();
                main->Name = "the module";
                FunctionState state;
                state.Code = main.get();
                state.TopLevel = true;
                State = &state;
                state.ScopeStarts.push_back(0);
                Statements(statements);
                Emit(Operation::Return, 0, 0, 0, Location {});
                State = nullptr;
                return main;
            }

        private:
            void Report(Location where, std::string message) { Problems.push_back({ where, std::move(message) }); }

            // ---- Instructions and registers --------------------------------------

            std::size_t Emit(Operation code, int a, int b, int c, Location where)
            {
                Prototype& prototype = *State->Code;
                prototype.Code.push_back({ code, static_cast<std::uint8_t>(a), static_cast<std::uint16_t>(b), static_cast<std::uint16_t>(c) });
                prototype.Places.push_back(where);
                return prototype.Code.size() - 1;
            }

            std::size_t EmitJump(Operation code, int a, Location where) { return Emit(code, a, 0, 0, where); }

            std::size_t Here() const { return State->Code->Code.size(); }

            void Patch(std::size_t jump, std::size_t target)
            {
                Instruction& instruction = State->Code->Code[jump];
                instruction.B = static_cast<std::uint16_t>(target & 0xFFFF);
                instruction.C = static_cast<std::uint16_t>(target >> 16);
            }

            int Allocate(Location where)
            {
                int index = State->FreeRegister++;
                if (index >= MostRegisters)
                {
                    if (index == MostRegisters)
                    {
                        Report(where, "this function uses too many values at once; split it into smaller functions");
                    }
                    index = MostRegisters - 1;
                }
                State->Code->RegisterCount = std::max(State->Code->RegisterCount, State->FreeRegister);
                return index;
            }

            std::uint16_t AddConstant(Value value)
            {
                std::vector<Value>& constants = State->Code->Constants;
                if (constants.size() >= 0xFFFF)
                {
                    Report({}, "this function holds too many different values; split it into smaller functions");
                    return 0;
                }
                constants.push_back(value);
                return static_cast<std::uint16_t>(constants.size() - 1);
            }

            std::uint16_t Number(double number)
            {
                std::uint64_t bits = std::bit_cast<std::uint64_t>(number);
                auto found = State->Numbers.find(bits);
                if (found != State->Numbers.end())
                {
                    return found->second;
                }
                std::uint16_t index = AddConstant(Value::Of(number));
                State->Numbers.emplace(bits, index);
                return index;
            }

            std::uint16_t Text(std::string_view text)
            {
                TextObject* object = TheHeap.Text(text);
                auto found = State->Texts.find(object);
                if (found != State->Texts.end())
                {
                    return found->second;
                }
                std::uint16_t index = AddConstant(Value::Of(static_cast<Object*>(object)));
                State->Texts.emplace(object, index);
                return index;
            }

            // ---- Scopes and names ---------------------------------------------------

            bool AtModuleScope() const { return State->TopLevel && State->ScopeStarts.size() == 1; }

            void PushScope() { State->ScopeStarts.push_back(State->Locals.size()); }

            void PopScope()
            {
                std::size_t start = State->ScopeStarts.back();
                State->ScopeStarts.pop_back();
                if (start < State->Locals.size())
                {
                    State->FreeRegister = State->Locals[start].Register;
                    State->Locals.resize(start);
                }
            }

            bool Captured(const void* key) const { return Analysis.Captured.contains(key); }

            void AddLocal(const std::string& name, int registerIndex, bool boxed, const void* key)
            {
                State->Locals.push_back({ name, registerIndex, boxed, key });
            }

            static Local* FindLocal(FunctionState& state, const std::string& name)
            {
                for (std::size_t index = state.Locals.size(); index-- > 0;)
                {
                    if (state.Locals[index].Name == name)
                    {
                        return &state.Locals[index];
                    }
                }
                return nullptr;
            }

            int AddCapture(FunctionState& state, const void* key, CaptureSource source)
            {
                for (std::size_t index = 0; index < state.CaptureKeys.size(); ++index)
                {
                    if (state.CaptureKeys[index] == key)
                    {
                        return static_cast<int>(index);
                    }
                }
                state.CaptureKeys.push_back(key);
                state.Code->Captures.push_back(source);
                return static_cast<int>(state.CaptureKeys.size() - 1);
            }

            int FindCapture(FunctionState& state, const std::string& name)
            {
                if (!state.Enclosing)
                {
                    return -1;
                }
                if (Local* local = FindLocal(*state.Enclosing, name))
                {
                    return AddCapture(state, local->Key, { true, static_cast<std::uint16_t>(local->Register) });
                }
                int outer = FindCapture(*state.Enclosing, name);
                if (outer < 0)
                {
                    return -1;
                }
                return AddCapture(state, state.Enclosing->CaptureKeys[static_cast<std::size_t>(outer)],
                    { false, static_cast<std::uint16_t>(outer) });
            }

            Place Locate(const std::string& name)
            {
                if (Local* local = FindLocal(*State, name))
                {
                    return { local->Boxed ? Place::Kind::Box : Place::Kind::Register, local->Register };
                }
                int capture = FindCapture(*State, name);
                if (capture >= 0)
                {
                    return { Place::Kind::Capture, capture };
                }
                return { Place::Kind::Global, 0 };
            }

            void Load(const Place& place, const std::string& name, int target, Location where)
            {
                switch (place.Type)
                {
                case Place::Kind::Register:
                    if (place.Index != target)
                    {
                        Emit(Operation::Move, target, place.Index, 0, where);
                    }
                    break;
                case Place::Kind::Box: Emit(Operation::GetBox, target, place.Index, 0, where); break;
                case Place::Kind::Capture: Emit(Operation::GetCapture, target, place.Index, 0, where); break;
                case Place::Kind::Global: Emit(Operation::GetGlobal, target, Text(name), 0, where); break;
                }
            }

            void Store(const Place& place, const std::string& name, int source, Location where)
            {
                switch (place.Type)
                {
                case Place::Kind::Register:
                    if (place.Index != source)
                    {
                        Emit(Operation::Move, place.Index, source, 0, where);
                    }
                    break;
                case Place::Kind::Box: Emit(Operation::SetBox, place.Index, source, 0, where); break;
                case Place::Kind::Capture: Emit(Operation::SetCapture, source, place.Index, 0, where); break;
                case Place::Kind::Global: Emit(Operation::SetGlobal, source, Text(name), 0, where); break;
                }
            }

            // Declares a name whose value is in `source`: a global at the module's top
            // level, a local register elsewhere.
            void DeclareHere(const std::string& name, const void* key, int source, Location where)
            {
                if (AtModuleScope())
                {
                    Emit(Operation::DefineGlobal, source, Text(name), 0, where);
                    return;
                }
                int registerIndex = Allocate(where);
                if (registerIndex != source)
                {
                    Emit(Operation::Move, registerIndex, source, 0, where);
                }
                bool boxed = Captured(key);
                if (boxed)
                {
                    Emit(Operation::MakeBox, registerIndex, 0, 0, where);
                }
                AddLocal(name, registerIndex, boxed, key);
            }

            // ---- Statements -----------------------------------------------------------

            void Statements(const Block& block)
            {
                for (const std::unique_ptr<Statement>& statement : block)
                {
                    StatementCode(*statement);
                }
            }

            void ScopedBlock(const Block& block)
            {
                PushScope();
                Statements(block);
                PopScope();
            }

            void StatementCode(const Statement& statement)
            {
                Location where = statement.Where;
                switch (statement.Kind)
                {
                case StatementKind::Variable:
                case StatementKind::Constant:
                {
                    if (AtModuleScope())
                    {
                        int saved = State->FreeRegister;
                        int value = Allocate(where);
                        ValueOrNothing(statement, value);
                        Emit(Operation::DefineGlobal, value, Text(statement.Name), 0, where);
                        State->FreeRegister = saved;
                        return;
                    }
                    int registerIndex = Allocate(where);
                    ValueOrNothing(statement, registerIndex);
                    bool boxed = Captured(&statement);
                    if (boxed)
                    {
                        Emit(Operation::MakeBox, registerIndex, 0, 0, where);
                    }
                    AddLocal(statement.Name, registerIndex, boxed, &statement);
                    return;
                }
                case StatementKind::Value: return;
                case StatementKind::Function: FunctionStatement(statement); return;
                case StatementKind::If:
                {
                    std::vector<std::size_t> ends;
                    for (std::size_t index = 0; index < statement.Expressions.size(); ++index)
                    {
                        int saved = State->FreeRegister;
                        int condition = Operand(*statement.Expressions[index]);
                        std::size_t skip = EmitJump(Operation::JumpIfFalse, condition, statement.Expressions[index]->Where);
                        State->FreeRegister = saved;
                        ScopedBlock(statement.Blocks[index]);
                        if (index + 1 < statement.Blocks.size())
                        {
                            ends.push_back(EmitJump(Operation::Jump, 0, where));
                        }
                        Patch(skip, Here());
                    }
                    if (statement.Blocks.size() > statement.Expressions.size())
                    {
                        ScopedBlock(statement.Blocks.back());
                    }
                    for (std::size_t end : ends)
                    {
                        Patch(end, Here());
                    }
                    return;
                }
                case StatementKind::While:
                {
                    std::size_t start = Here();
                    int saved = State->FreeRegister;
                    int condition = Operand(*statement.Expressions[0]);
                    std::size_t exit = EmitJump(Operation::JumpIfFalse, condition, where);
                    State->FreeRegister = saved;
                    State->Loops.push_back({ {}, {}, State->TryDepth });
                    ScopedBlock(statement.Blocks[0]);
                    Patch(EmitJump(Operation::Jump, 0, where), start);
                    FinishLoop(Here(), start);
                    Patch(exit, Here());
                    PatchBreaks(Here());
                    return;
                }
                case StatementKind::For: ForStatement(statement); return;
                case StatementKind::Return:
                {
                    for (int index = 0; index < State->TryDepth; ++index)
                    {
                        Emit(Operation::TryEnd, 0, 0, 0, where);
                    }
                    if (statement.Expressions.empty())
                    {
                        Emit(Operation::Return, 0, 0, 0, where);
                        return;
                    }
                    int saved = State->FreeRegister;
                    int value = Operand(*statement.Expressions[0]);
                    Emit(Operation::Return, value, 1, 0, where);
                    State->FreeRegister = saved;
                    return;
                }
                case StatementKind::Break:
                case StatementKind::Continue:
                {
                    if (State->Loops.empty())
                    {
                        return;
                    }
                    LoopContext& loop = State->Loops.back();
                    for (int index = loop.TryDepth; index < State->TryDepth; ++index)
                    {
                        Emit(Operation::TryEnd, 0, 0, 0, where);
                    }
                    std::size_t jump = EmitJump(Operation::Jump, 0, where);
                    (statement.Kind == StatementKind::Break ? loop.Breaks : loop.Continues).push_back(jump);
                    return;
                }
                case StatementKind::Type:
                {
                    // The name and the field names sit next to each other in the constants.
                    std::uint16_t name = AddConstant(Value::Of(static_cast<Object*>(TheHeap.Text(statement.Name))));
                    for (const Parameter& field : statement.Parameters)
                    {
                        AddConstant(Value::Of(static_cast<Object*>(TheHeap.Text(field.Name))));
                    }
                    int saved = State->FreeRegister;
                    int record = Allocate(where);
                    Emit(Operation::MakeRecord, record, name, static_cast<int>(statement.Parameters.size()), where);
                    State->FreeRegister = saved;
                    DeclareFrom(statement.Name, &statement, record, saved, where);
                    return;
                }
                case StatementKind::Import:
                {
                    int saved = State->FreeRegister;
                    int module = Allocate(where);
                    Emit(Operation::Import, module, Text(statement.Name), 0, where);
                    State->FreeRegister = saved;
                    DeclareFrom(ModuleNameOf(statement.Name), &statement, module, saved, where);
                    return;
                }
                case StatementKind::Try: TryStatement(statement); return;
                case StatementKind::Spawn:
                {
                    const Expression& call = *statement.Expressions[0];
                    if (call.Kind != ExpressionKind::Call)
                    {
                        return;
                    }
                    int saved = State->FreeRegister;
                    int base = Allocate(where);
                    ExpressionCode(*call.Parts[0], base);
                    for (std::size_t index = 1; index < call.Parts.size(); ++index)
                    {
                        ExpressionCode(*call.Parts[index], Allocate(call.Parts[index]->Where));
                    }
                    Emit(Operation::Spawn, base, static_cast<int>(call.Parts.size() - 1), 0, where);
                    State->FreeRegister = saved;
                    return;
                }
                case StatementKind::Wait:
                {
                    if (statement.Expressions.empty())
                    {
                        Emit(Operation::Wait, 0, 0, 0, where);
                        return;
                    }
                    int saved = State->FreeRegister;
                    int seconds = Operand(*statement.Expressions[0]);
                    Emit(Operation::Wait, seconds, 1, 0, where);
                    State->FreeRegister = saved;
                    return;
                }
                case StatementKind::YieldControl: Emit(Operation::Wait, 0, 0, 0, where); return;
                case StatementKind::Assignment: Assignment(statement); return;
                case StatementKind::Expression:
                {
                    int saved = State->FreeRegister;
                    ExpressionCode(*statement.Expressions[0], Allocate(where));
                    State->FreeRegister = saved;
                    return;
                }
                }
            }

            // A declaration whose value was put in a register freed just after: the
            // register is taken back as the local's own, or the value stored as a global.
            void DeclareFrom(const std::string& name, const void* key, int valueRegister, int saved, Location where)
            {
                if (AtModuleScope())
                {
                    Emit(Operation::DefineGlobal, valueRegister, Text(name), 0, where);
                    return;
                }
                State->FreeRegister = saved;
                DeclareHere(name, key, valueRegister, where);
            }

            void ValueOrNothing(const Statement& statement, int target)
            {
                if (statement.Expressions.empty())
                {
                    Emit(Operation::LoadNothing, target, 0, 0, statement.Where);
                }
                else
                {
                    ExpressionCode(*statement.Expressions[0], target);
                }
            }

            void FinishLoop(std::size_t breakTarget, std::size_t continueTarget)
            {
                LoopContext& loop = State->Loops.back();
                for (std::size_t jump : loop.Continues)
                {
                    Patch(jump, continueTarget);
                }
                PendingBreaks = std::move(loop.Breaks);
                State->Loops.pop_back();
                (void)breakTarget;
            }

            void PatchBreaks(std::size_t target)
            {
                for (std::size_t jump : PendingBreaks)
                {
                    Patch(jump, target);
                }
                PendingBreaks.clear();
            }

            void FunctionStatement(const Statement& statement)
            {
                Location where = statement.Where;
                if (AtModuleScope())
                {
                    int saved = State->FreeRegister;
                    int function = Allocate(where);
                    Emit(Operation::Closure, function, CompileFunction(statement, statement.Name), 0, where);
                    Emit(Operation::DefineGlobal, function, Text(statement.Name), 0, where);
                    State->FreeRegister = saved;
                    return;
                }
                int registerIndex = Allocate(where);
                if (Captured(&statement))
                {
                    // Declared first, in a box, so the function can call itself.
                    Emit(Operation::LoadNothing, registerIndex, 0, 0, where);
                    Emit(Operation::MakeBox, registerIndex, 0, 0, where);
                    AddLocal(statement.Name, registerIndex, true, &statement);
                    int saved = State->FreeRegister;
                    int function = Allocate(where);
                    Emit(Operation::Closure, function, CompileFunction(statement, statement.Name), 0, where);
                    Emit(Operation::SetBox, registerIndex, function, 0, where);
                    State->FreeRegister = saved;
                    return;
                }
                Emit(Operation::Closure, registerIndex, CompileFunction(statement, statement.Name), 0, where);
                AddLocal(statement.Name, registerIndex, false, &statement);
            }

            int CompileFunction(const Statement& function, const std::string& name)
            {
                auto prototype = std::make_unique<Prototype>();
                prototype->Name = name.empty() ? "a function" : name;
                prototype->ParameterCount = static_cast<int>(function.Parameters.size());
                FunctionState state;
                state.Enclosing = State;
                state.Code = prototype.get();
                state.ScopeStarts.push_back(0);
                FunctionState* outer = State;
                State = &state;
                for (const Parameter& parameter : function.Parameters)
                {
                    int registerIndex = Allocate(parameter.Where);
                    bool boxed = Captured(&parameter);
                    AddLocal(parameter.Name, registerIndex, boxed, &parameter);
                }
                for (const Local& local : state.Locals)
                {
                    if (local.Boxed)
                    {
                        Emit(Operation::MakeBox, local.Register, 0, 0, function.Where);
                    }
                }
                Statements(function.Blocks[0]);
                Emit(Operation::Return, 0, 0, 0, function.Where);
                State = outer;
                State->Code->Children.push_back(std::move(prototype));
                return static_cast<int>(State->Code->Children.size() - 1);
            }

            void ForStatement(const Statement& statement)
            {
                Location where = statement.Where;
                int saved = State->FreeRegister;
                int base = Allocate(where);
                int position = Allocate(where);
                int variable = Allocate(where);
                std::size_t loopStart = 0;
                std::size_t exit = 0;
                if (statement.Expressions.size() == 2)
                {
                    ExpressionCode(*statement.Expressions[0], base);
                    ExpressionCode(*statement.Expressions[1], position);
                    loopStart = Here();
                    exit = EmitJump(Operation::RangeCheck, base, where);
                }
                else
                {
                    ExpressionCode(*statement.Expressions[0], base);
                    Emit(Operation::EachStart, base, 0, 0, where);
                    loopStart = Here();
                    exit = EmitJump(Operation::EachNext, base, where);
                }
                State->Loops.push_back({ {}, {}, State->TryDepth });
                PushScope();
                bool boxed = Captured(&statement.Name);
                if (boxed)
                {
                    Emit(Operation::MakeBox, variable, 0, 0, where);
                }
                AddLocal(statement.Name, variable, boxed, &statement.Name);
                Statements(statement.Blocks[0]);
                PopScope();
                State->FreeRegister = variable + 1;
                std::size_t continueTarget = Here();
                if (statement.Expressions.size() == 2)
                {
                    Patch(EmitJump(Operation::RangeStep, base, where), loopStart);
                }
                else
                {
                    Patch(EmitJump(Operation::Jump, 0, where), loopStart);
                }
                FinishLoop(Here(), continueTarget);
                Patch(exit, Here());
                PatchBreaks(Here());
                State->FreeRegister = saved;
            }

            void TryStatement(const Statement& statement)
            {
                Location where = statement.Where;
                int saved = State->FreeRegister;
                int problem = Allocate(where);
                std::size_t start = EmitJump(Operation::TryStart, problem, where);
                ++State->TryDepth;
                ScopedBlock(statement.Blocks[0]);
                --State->TryDepth;
                Emit(Operation::TryEnd, 0, 0, 0, where);
                std::size_t end = EmitJump(Operation::Jump, 0, where);
                Patch(start, Here());
                PushScope();
                bool boxed = Captured(&statement.Name);
                if (boxed)
                {
                    Emit(Operation::MakeBox, problem, 0, 0, where);
                }
                AddLocal(statement.Name, problem, boxed, &statement.Name);
                Statements(statement.Blocks[1]);
                PopScope();
                Patch(end, Here());
                State->FreeRegister = saved;
            }

            Operation ArithmeticFor(std::string_view symbol)
            {
                if (symbol == "+")
                {
                    return Operation::Add;
                }
                if (symbol == "-")
                {
                    return Operation::Subtract;
                }
                if (symbol == "*")
                {
                    return Operation::Multiply;
                }
                if (symbol == "/")
                {
                    return Operation::Divide;
                }
                if (symbol == "%")
                {
                    return Operation::Remainder;
                }
                return Operation::Power;
            }

            void Assignment(const Statement& statement)
            {
                Location where = statement.Where;
                const Expression& target = *statement.Expressions[0];
                const Expression& value = *statement.Expressions[1];
                bool compound = statement.Name != "=";
                Operation arithmetic = compound ? ArithmeticFor(statement.Name.substr(0, 1)) : Operation::Add;
                int saved = State->FreeRegister;
                if (target.Kind == ExpressionKind::Name)
                {
                    Place place = Locate(target.Name);
                    int result = Allocate(where);
                    if (compound)
                    {
                        Load(place, target.Name, result, target.Where);
                        int right = Operand(value);
                        Emit(arithmetic, result, result, right, where);
                    }
                    else
                    {
                        ExpressionCode(value, result);
                    }
                    Store(place, target.Name, result, where);
                }
                else if (target.Kind == ExpressionKind::Member)
                {
                    int object = Operand(*target.Parts[0]);
                    std::uint16_t name = Text(target.Name);
                    int result = Allocate(where);
                    if (compound)
                    {
                        Emit(Operation::GetField, result, object, name, target.Where);
                        int right = Operand(value);
                        Emit(arithmetic, result, result, right, where);
                    }
                    else
                    {
                        ExpressionCode(value, result);
                    }
                    Emit(Operation::SetField, object, name, result, where);
                }
                else if (target.Kind == ExpressionKind::Index)
                {
                    int object = Operand(*target.Parts[0]);
                    int index = Operand(*target.Parts[1]);
                    int result = Allocate(where);
                    if (compound)
                    {
                        Emit(Operation::GetIndex, result, object, index, target.Where);
                        int right = Operand(value);
                        Emit(arithmetic, result, result, right, where);
                    }
                    else
                    {
                        ExpressionCode(value, result);
                    }
                    Emit(Operation::SetIndex, object, index, result, where);
                }
                State->FreeRegister = saved;
            }

            // ---- Expressions ----------------------------------------------------------

            // The register an expression's value is in: a local's own register, or a
            // new one it was computed into. Free registers after use.
            int Operand(const Expression& expression)
            {
                if (expression.Kind == ExpressionKind::Name)
                {
                    if (Local* local = FindLocal(*State, expression.Name); local && !local->Boxed)
                    {
                        return local->Register;
                    }
                }
                int target = Allocate(expression.Where);
                ExpressionCode(expression, target);
                return target;
            }

            void ExpressionCode(const Expression& expression, int target)
            {
                Location where = expression.Where;
                int saved = State->FreeRegister;
                switch (expression.Kind)
                {
                case ExpressionKind::Number: Emit(Operation::LoadConstant, target, Number(expression.Number), 0, where); break;
                case ExpressionKind::Text:
                {
                    if (expression.Parts.empty())
                    {
                        Emit(Operation::LoadConstant, target, Text(expression.Pieces.empty() ? "" : expression.Pieces[0]), 0, where);
                        break;
                    }
                    int first = State->FreeRegister;
                    int count = 0;
                    for (std::size_t index = 0; index < expression.Pieces.size(); ++index)
                    {
                        if (!expression.Pieces[index].empty())
                        {
                            Emit(Operation::LoadConstant, Allocate(where), Text(expression.Pieces[index]), 0, where);
                            ++count;
                        }
                        if (index < expression.Parts.size())
                        {
                            ExpressionCode(*expression.Parts[index], Allocate(expression.Parts[index]->Where));
                            ++count;
                        }
                    }
                    Emit(Operation::BuildText, target, first, count, where);
                    break;
                }
                case ExpressionKind::ColorCode: ColorCode(expression, target); break;
                case ExpressionKind::Boolean: Emit(Operation::LoadBoolean, target, expression.Boolean ? 1 : 0, 0, where); break;
                case ExpressionKind::Nothing: Emit(Operation::LoadNothing, target, 0, 0, where); break;
                case ExpressionKind::Name: Load(Locate(expression.Name), expression.Name, target, where); break;
                case ExpressionKind::Unary:
                {
                    int operand = Operand(*expression.Parts[0]);
                    Emit(expression.Name == "not" ? Operation::Not : Operation::Negate, target, operand, 0, where);
                    break;
                }
                case ExpressionKind::Binary: Binary(expression, target); break;
                case ExpressionKind::Call: Call(expression, target); break;
                case ExpressionKind::Member:
                {
                    int object = Operand(*expression.Parts[0]);
                    Emit(Operation::GetField, target, object, Text(expression.Name), where);
                    break;
                }
                case ExpressionKind::Index:
                {
                    int object = Operand(*expression.Parts[0]);
                    int index = Operand(*expression.Parts[1]);
                    Emit(Operation::GetIndex, target, object, index, where);
                    break;
                }
                case ExpressionKind::List:
                {
                    int first = State->FreeRegister;
                    for (const std::unique_ptr<Expression>& item : expression.Parts)
                    {
                        ExpressionCode(*item, Allocate(item->Where));
                    }
                    Emit(Operation::NewList, target, first, static_cast<int>(expression.Parts.size()), where);
                    break;
                }
                case ExpressionKind::Table:
                {
                    // Built in a register of its own, so a value naming the target
                    // reads it before the table replaces it.
                    int table = Allocate(where);
                    Emit(Operation::NewTable, table, 0, 0, where);
                    for (std::size_t index = 0; index < expression.Parts.size(); ++index)
                    {
                        int inner = State->FreeRegister;
                        int value = Operand(*expression.Parts[index]);
                        Emit(Operation::SetField, table, Text(expression.Pieces[index]), value, expression.Parts[index]->Where);
                        State->FreeRegister = inner;
                    }
                    Emit(Operation::Move, target, table, 0, where);
                    break;
                }
                case ExpressionKind::Function:
                    Emit(Operation::Closure, target, CompileFunction(*expression.Function, ""), 0, where);
                    break;
                }
                State->FreeRegister = saved;
            }

            void ColorCode(const Expression& expression, int target)
            {
                std::string digits = expression.Name.substr(1);
                if (digits.size() == 3)
                {
                    digits = { digits[0], digits[0], digits[1], digits[1], digits[2], digits[2] };
                }
                if (digits.size() == 6)
                {
                    digits += "FF";
                }
                auto channel = [&](std::size_t index) {
                    if (digits.size() < index * 2 + 2)
                    {
                        return 1.0;
                    }
                    return static_cast<double>(std::stoi(digits.substr(index * 2, 2), nullptr, 16)) / 255.0;
                };
                int table = Allocate(expression.Where);
                Emit(Operation::NewTable, table, 0, 0, expression.Where);
                static constexpr std::string_view names[] = { "Red", "Green", "Blue", "Alpha" };
                for (std::size_t index = 0; index < 4; ++index)
                {
                    int value = Allocate(expression.Where);
                    Emit(Operation::LoadConstant, value, Number(channel(index)), 0, expression.Where);
                    Emit(Operation::SetField, table, Text(names[index]), value, expression.Where);
                    --State->FreeRegister;
                }
                Emit(Operation::Move, target, table, 0, expression.Where);
            }

            void Binary(const Expression& expression, int target)
            {
                const std::string& symbol = expression.Name;
                Location where = expression.Where;
                if (symbol == "and" || symbol == "or")
                {
                    int result = Allocate(where);
                    ExpressionCode(*expression.Parts[0], result);
                    std::size_t skip = EmitJump(symbol == "and" ? Operation::JumpIfFalse : Operation::JumpIfTrue, result, where);
                    ExpressionCode(*expression.Parts[1], result);
                    Patch(skip, Here());
                    Emit(Operation::Move, target, result, 0, where);
                    return;
                }
                int left = Operand(*expression.Parts[0]);
                int right = Operand(*expression.Parts[1]);
                if (symbol == "==")
                {
                    Emit(Operation::Equal, target, left, right, where);
                }
                else if (symbol == "!=")
                {
                    Emit(Operation::NotEqual, target, left, right, where);
                }
                else if (symbol == "<")
                {
                    Emit(Operation::Less, target, left, right, where);
                }
                else if (symbol == "<=")
                {
                    Emit(Operation::LessEqual, target, left, right, where);
                }
                else if (symbol == ">")
                {
                    Emit(Operation::Less, target, right, left, where);
                }
                else if (symbol == ">=")
                {
                    Emit(Operation::LessEqual, target, right, left, where);
                }
                else
                {
                    Emit(ArithmeticFor(symbol), target, left, right, where);
                }
            }

            void Call(const Expression& call, int target)
            {
                Location where = call.Where;
                const Expression& callee = *call.Parts[0];
                int base = Allocate(where);
                bool member = callee.Kind == ExpressionKind::Member;
                ExpressionCode(member ? *callee.Parts[0] : callee, base);
                for (std::size_t index = 1; index < call.Parts.size(); ++index)
                {
                    ExpressionCode(*call.Parts[index], Allocate(call.Parts[index]->Where));
                }
                int count = static_cast<int>(call.Parts.size() - 1);
                if (member)
                {
                    Emit(Operation::Invoke, base, count, Text(callee.Name), where);
                }
                else
                {
                    Emit(Operation::Call, base, count, 0, where);
                }
                if (base != target)
                {
                    Emit(Operation::Move, target, base, 0, where);
                }
            }

            Heap& TheHeap;
            const AnalysisResult& Analysis;
            std::vector<Problem>& Problems;
            FunctionState* State = nullptr;
            std::vector<std::size_t> PendingBreaks;
        };
    }

    Compiled Compile(const language::SyntaxTree& tree, Heap& heap, const CompileSettings& settings)
    {
        Compiled result;
        result.Problems = tree.Problems;
        AnalysisResult analysis = Analyze(tree.Statements, settings, result.Problems);
        if (!result.Problems.empty())
        {
            return result;
        }
        Emitter emitter(heap, analysis, result.Problems);
        result.Main = emitter.CompileMain(tree.Statements);
        return result;
    }
}
