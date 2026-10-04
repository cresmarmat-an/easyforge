#include "Machine.h"

#include <cmath>
#include <format>

#include "Builtins.h"
#include "EngineState.h"

namespace easyforge::internal::scripting
{
    namespace
    {
        // Calls deeper than this stop with an error rather than using up memory.
        // Script frames live in the task, not on the program's stack.
        constexpr std::size_t DeepestCalls = 1000;

        bool IsKind(Value value, ObjectKind kind) { return value.IsReference() && value.Pointer->Kind == kind; }

        template <typename Type>
        Type* As(Value value)
        {
            return static_cast<Type*>(value.Pointer);
        }

        // The byte where the character after `index` starts in UTF-8 text.
        std::size_t NextCharacterAt(const std::string& text, std::size_t index)
        {
            ++index;
            while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80)
            {
                ++index;
            }
            return index;
        }
    }

    bool Machine::Fail(std::string message)
    {
        Problem = std::move(message);
        return false;
    }

    std::string Machine::Where(const Task& task) const
    {
        if (task.Frames.empty())
        {
            return "script";
        }
        const Frame& frame = task.Frames.back();
        const Prototype& code = *frame.Function->Code;
        std::size_t at = frame.Next > 0 ? frame.Next - 1 : 0;
        language::Location place = at < code.Places.size() ? code.Places[at] : language::Location {};
        const std::string& module = frame.Function->Module ? frame.Function->Module->Name : std::string("script");
        return std::format("{}:{}:{}", module, place.Line, place.Column);
    }

    bool Machine::Recover(Task& task)
    {
        if (Fatal)
        {
            return false;
        }
        while (!task.Handlers.empty())
        {
            Handler handler = task.Handlers.back();
            task.Handlers.pop_back();
            if (handler.Frames > task.Frames.size() || handler.Frames == 0)
            {
                continue;
            }
            task.Frames.resize(handler.Frames);
            task.Stack[handler.ProblemSlot] = Value::Of(static_cast<Object*>(Engine.TheHeap.Text(Problem)));
            task.Frames.back().Next = handler.Catch;
            Problem.clear();
            return true;
        }
        return false;
    }

    bool Machine::Begin(Task& task, Value callee, std::span<const Value> arguments)
    {
        task.Stack.assign(1, callee);
        task.Stack.insert(task.Stack.end(), arguments.begin(), arguments.end());
        task.Frames.clear();
        task.Handlers.clear();
        if (!IsKind(callee, ObjectKind::Function))
        {
            return Fail(std::format("{} cannot be started as a task; give spawn a function written in the script",
                Engine.Describe(callee)));
        }
        return CallAt(task, 0, arguments.size());
    }

    bool Machine::CallAt(Task& task, std::size_t slot, std::size_t count)
    {
        Value callee = task.Stack[slot];
        if (IsKind(callee, ObjectKind::Function))
        {
            FunctionObject* function = As<FunctionObject>(callee);
            const Prototype& code = *function->Code;
            if (count > static_cast<std::size_t>(code.ParameterCount))
            {
                return Fail(std::format("{} takes {} argument{}, but was given {}", code.Name, code.ParameterCount,
                    code.ParameterCount == 1 ? "" : "s", count));
            }
            if (task.Frames.size() >= DeepestCalls)
            {
                return Fail(std::format("the calls go more than {} deep; a function may be calling itself without end",
                    DeepestCalls));
            }
            std::size_t base = slot + 1;
            std::size_t top = base + static_cast<std::size_t>(code.RegisterCount);
            if (task.Stack.size() < top)
            {
                task.Stack.resize(top);
            }
            for (std::size_t index = base + count; index < top; ++index)
            {
                task.Stack[index] = Value();
            }
            task.Frames.push_back({ function, base, 0, slot });
            return true;
        }
        std::vector<Value> arguments(task.Stack.begin() + static_cast<std::ptrdiff_t>(slot + 1),
            task.Stack.begin() + static_cast<std::ptrdiff_t>(slot + 1 + count));
        if (IsKind(callee, ObjectKind::Native))
        {
            NativeObject* native = As<NativeObject>(callee);
            Value result;
            if (native->Builtin)
            {
                if (!native->Bound.IsNothing())
                {
                    arguments.insert(arguments.begin(), native->Bound);
                }
                if (!native->Builtin(*this, arguments, result))
                {
                    return false;
                }
            }
            else
            {
                std::vector<ScriptValue> given;
                given.reserve(arguments.size());
                for (const Value& argument : arguments)
                {
                    given.push_back(Engine.ToScript(argument));
                }
                Result<ScriptValue> returned = native->Program(given);
                if (!returned)
                {
                    return Fail(returned.Error());
                }
                result = Engine.ToValue(*returned);
            }
            task.Stack[slot] = result;
            return true;
        }
        if (IsKind(callee, ObjectKind::Record))
        {
            RecordObject* record = As<RecordObject>(callee);
            if (count > record->Fields.size())
            {
                return Fail(std::format("{} has {} field{}, but was given {} values", record->Name->Text,
                    record->Fields.size(), record->Fields.size() == 1 ? "" : "s", count));
            }
            TableObject* table = Engine.TheHeap.Make<TableObject>();
            table->Record = record;
            for (std::size_t index = 0; index < record->Fields.size(); ++index)
            {
                table->Set(record->Fields[index], index < count ? arguments[index] : Value());
            }
            Engine.TheHeap.Grew(*table, record->Fields.size() * sizeof(std::pair<TextObject*, Value>) * 2);
            task.Stack[slot] = Value::Of(static_cast<Object*>(table));
            return true;
        }
        if (callee.IsNothing())
        {
            return Fail("this is nothing, so it cannot be called; the function may not be defined yet");
        }
        return Fail(std::format("{} cannot be called", Engine.Describe(callee)));
    }

    bool Machine::Invoke(Task& task, std::size_t slot, std::size_t count, TextObject* name)
    {
        Value object = task.Stack[slot];
        if (IsKind(object, ObjectKind::Table))
        {
            const Value* field = As<TableObject>(object)->Find(name);
            if (!field || field->IsNothing())
            {
                return Fail(std::format("{} has no {} to call", Engine.Describe(object), name->Text));
            }
            task.Stack[slot] = *field;
            return CallAt(task, slot, count);
        }
        if (IsKind(object, ObjectKind::Host))
        {
            ScriptValue member = As<HostObject>(object)->Held->Get(name->Text);
            if (member.IsNothing())
            {
                return Fail(std::format("{} has no {}", Engine.Describe(object), name->Text));
            }
            task.Stack[slot] = Engine.ToValue(member);
            return CallAt(task, slot, count);
        }
        std::vector<Value> arguments;
        arguments.reserve(count + 1);
        arguments.push_back(object);
        for (std::size_t index = 0; index < count; ++index)
        {
            arguments.push_back(task.Stack[slot + 1 + index]);
        }
        Value result;
        MemberResult found = CallMember(*this, name, arguments, result);
        if (found == MemberResult::Failed)
        {
            return false;
        }
        if (found == MemberResult::Missing)
        {
            if (object.IsNothing())
            {
                return Fail(std::format("this is nothing, so it has no {} to call", name->Text));
            }
            return Fail(std::format("{} has no {} to call", Engine.Describe(object), name->Text));
        }
        task.Stack[slot] = result;
        return true;
    }

    Outcome Machine::Run(Task& task)
    {
        Engine.Running.push_back(&task);
        struct Leave
        {
            EngineState& Engine;
            ~Leave() { Engine.Running.pop_back(); }
        } leave { Engine };

        // On a failure: to the innermost try that catches it, or out of the task.
#define EASYFORGE_FAIL_IF(failed)                                                                                         \
    if (failed)                                                                                                           \
    {                                                                                                                     \
        if (Recover(task))                                                                                                \
        {                                                                                                                 \
            continue;                                                                                                     \
        }                                                                                                                 \
        Problem = std::format("{}: {}", Where(task), Problem);                                                            \
        return Outcome::Failed;                                                                                           \
    }

        while (true)
        {
            std::string limit;
            if (!Engine.BetweenInstructions(limit) || !Engine.CountInstruction())
            {
                Fatal = true;
                Problem = limit.empty() ? std::format("the script ran more than {} instructions", Engine.Settings.InstructionLimit)
                                        : limit;
                Problem = std::format("{}: {}", Where(task), Problem);
                return Outcome::Failed;
            }

            Frame& frame = task.Frames.back();
            FunctionObject* function = frame.Function;
            const Prototype& code = *function->Code;
            const Instruction instruction = code.Code[frame.Next++];
            const std::size_t base = frame.Base;
            Value* registers = task.Stack.data() + base;
            const Value* constants = code.Constants.data();
            auto text = [&](std::uint16_t index) { return static_cast<TextObject*>(constants[index].Pointer); };

            switch (instruction.Code)
            {
            case Operation::LoadNothing: registers[instruction.A] = Value(); break;
            case Operation::LoadBoolean: registers[instruction.A] = Value::Of(instruction.B != 0); break;
            case Operation::LoadConstant: registers[instruction.A] = constants[instruction.B]; break;
            case Operation::Move: registers[instruction.A] = registers[instruction.B]; break;
            case Operation::GetGlobal:
            {
                TextObject* name = text(instruction.B);
                const Value* found = function->Module ? function->Module->Globals->Find(name) : nullptr;
                if (!found)
                {
                    found = Engine.Globals->Find(name);
                }
                EASYFORGE_FAIL_IF(!found && !Fail(std::format("there is no {}", name->Text)));
                registers[instruction.A] = *found;
                break;
            }
            case Operation::SetGlobal:
            {
                TextObject* name = text(instruction.B);
                TableObject* module = function->Module ? function->Module->Globals : Engine.Globals;
                if (!module->Find(name) && Engine.Globals->Find(name))
                {
                    module = Engine.Globals;
                }
                module->Set(name, registers[instruction.A]);
                break;
            }
            case Operation::DefineGlobal:
            {
                TableObject* module = function->Module ? function->Module->Globals : Engine.Globals;
                module->Set(text(instruction.B), registers[instruction.A]);
                Engine.TheHeap.Grew(*module, sizeof(std::pair<TextObject*, Value>) * 2);
                break;
            }
            case Operation::GetCapture: registers[instruction.A] = function->Captures[instruction.B]->Held; break;
            case Operation::SetCapture: function->Captures[instruction.B]->Held = registers[instruction.A]; break;
            case Operation::MakeBox:
            {
                BoxObject* box = Engine.TheHeap.Make<BoxObject>();
                box->Held = registers[instruction.A];
                registers[instruction.A] = Value::Of(static_cast<Object*>(box));
                break;
            }
            case Operation::GetBox: registers[instruction.A] = As<BoxObject>(registers[instruction.B])->Held; break;
            case Operation::SetBox: As<BoxObject>(registers[instruction.A])->Held = registers[instruction.B]; break;
            case Operation::Closure:
            {
                const Prototype* child = code.Children[instruction.B].get();
                FunctionObject* made = Engine.TheHeap.Make<FunctionObject>();
                made->Code = child;
                made->Module = function->Module;
                made->Captures.reserve(child->Captures.size());
                for (const CaptureSource& source : child->Captures)
                {
                    made->Captures.push_back(source.FromRegister ? As<BoxObject>(registers[source.Index])
                                                                 : function->Captures[source.Index]);
                }
                Engine.TheHeap.Grew(*made, made->Captures.size() * sizeof(BoxObject*));
                registers[instruction.A] = Value::Of(static_cast<Object*>(made));
                break;
            }
            case Operation::Negate:
            {
                Value operand = registers[instruction.B];
                EASYFORGE_FAIL_IF(!operand.IsNumber() &&
                                  !Fail(std::format("- works on numbers, but this is {}", Engine.Describe(operand))));
                registers[instruction.A] = Value::Of(-operand.Number);
                break;
            }
            case Operation::Not: registers[instruction.A] = Value::Of(!registers[instruction.B].IsTrue()); break;
            case Operation::Add:
            {
                Value left = registers[instruction.B];
                Value right = registers[instruction.C];
                if (left.IsNumber() && right.IsNumber())
                {
                    registers[instruction.A] = Value::Of(left.Number + right.Number);
                    break;
                }
                if (IsKind(left, ObjectKind::Text) && IsKind(right, ObjectKind::Text))
                {
                    std::string joined = As<TextObject>(left)->Text + As<TextObject>(right)->Text;
                    registers[instruction.A] = Value::Of(static_cast<Object*>(Engine.TheHeap.Text(joined)));
                    break;
                }
                bool mixed = (IsKind(left, ObjectKind::Text) && right.IsNumber()) ||
                             (left.IsNumber() && IsKind(right, ObjectKind::Text));
                EASYFORGE_FAIL_IF(mixed ? !Fail("a string and a number cannot be added; to join them, write \"{first}{second}\"")
                                        : !Fail(std::format("{} and {} cannot be added", Engine.Describe(left),
                                              Engine.Describe(right))));
                break;
            }
            case Operation::Subtract:
            case Operation::Multiply:
            case Operation::Divide:
            case Operation::Remainder:
            case Operation::Power:
            {
                Value left = registers[instruction.B];
                Value right = registers[instruction.C];
                if (!left.IsNumber() || !right.IsNumber())
                {
                    static constexpr std::string_view symbols[] = { "-", "*", "/", "%", "^" };
                    std::string_view symbol =
                        symbols[static_cast<int>(instruction.Code) - static_cast<int>(Operation::Subtract)];
                    Value wrong = left.IsNumber() ? right : left;
                    EASYFORGE_FAIL_IF(!Fail(std::format("{} works on numbers, but this is {}", symbol, Engine.Describe(wrong))));
                }
                double a = left.Number;
                double b = right.Number;
                double result = 0.0;
                switch (instruction.Code)
                {
                case Operation::Subtract: result = a - b; break;
                case Operation::Multiply: result = a * b; break;
                case Operation::Divide: result = a / b; break;
                case Operation::Remainder: result = a - std::floor(a / b) * b; break;
                default: result = std::pow(a, b); break;
                }
                registers[instruction.A] = Value::Of(result);
                break;
            }
            case Operation::Equal:
                registers[instruction.A] = Value::Of(Engine.Same(registers[instruction.B], registers[instruction.C]));
                break;
            case Operation::NotEqual:
                registers[instruction.A] = Value::Of(!Engine.Same(registers[instruction.B], registers[instruction.C]));
                break;
            case Operation::Less:
            case Operation::LessEqual:
            {
                Value left = registers[instruction.B];
                Value right = registers[instruction.C];
                bool orEqual = instruction.Code == Operation::LessEqual;
                if (left.IsNumber() && right.IsNumber())
                {
                    registers[instruction.A] = Value::Of(orEqual ? left.Number <= right.Number : left.Number < right.Number);
                    break;
                }
                if (IsKind(left, ObjectKind::Text) && IsKind(right, ObjectKind::Text))
                {
                    int order = As<TextObject>(left)->Text.compare(As<TextObject>(right)->Text);
                    registers[instruction.A] = Value::Of(orEqual ? order <= 0 : order < 0);
                    break;
                }
                EASYFORGE_FAIL_IF(!Fail(std::format("{} and {} cannot be compared", Engine.Describe(left), Engine.Describe(right))));
                break;
            }
            case Operation::Jump: frame.Next = instruction.Target(); break;
            case Operation::JumpIfFalse:
                if (!registers[instruction.A].IsTrue())
                {
                    frame.Next = instruction.Target();
                }
                break;
            case Operation::JumpIfTrue:
                if (registers[instruction.A].IsTrue())
                {
                    frame.Next = instruction.Target();
                }
                break;
            case Operation::Call: EASYFORGE_FAIL_IF(!CallAt(task, base + instruction.A, instruction.B)); break;
            case Operation::Invoke: EASYFORGE_FAIL_IF(!Invoke(task, base + instruction.A, instruction.B, text(instruction.C))); break;
            case Operation::Return:
            {
                Value result = instruction.B ? registers[instruction.A] : Value();
                std::size_t slot = frame.ResultSlot;
                task.Frames.pop_back();
                while (!task.Handlers.empty() && task.Handlers.back().Frames > task.Frames.size())
                {
                    task.Handlers.pop_back();
                }
                if (task.Frames.empty())
                {
                    task.Result = result;
                    return Outcome::Finished;
                }
                task.Stack[slot] = result;
                break;
            }
            case Operation::NewList:
            {
                ListObject* list = Engine.TheHeap.Make<ListObject>();
                list->Items.assign(registers + instruction.B, registers + instruction.B + instruction.C);
                Engine.TheHeap.Grew(*list, list->Items.size() * sizeof(Value));
                registers[instruction.A] = Value::Of(static_cast<Object*>(list));
                break;
            }
            case Operation::NewTable:
                registers[instruction.A] = Value::Of(static_cast<Object*>(Engine.TheHeap.Make<TableObject>()));
                break;
            case Operation::GetField:
            {
                Value object = registers[instruction.B];
                TextObject* name = text(instruction.C);
                Value result;
                EASYFORGE_FAIL_IF(!GetMember(*this, object, name, result));
                registers = task.Stack.data() + base;
                registers[instruction.A] = result;
                break;
            }
            case Operation::SetField:
                EASYFORGE_FAIL_IF(!SetMember(*this, registers[instruction.A], text(instruction.B), registers[instruction.C]));
                break;
            case Operation::GetIndex:
            {
                Value result;
                EASYFORGE_FAIL_IF(!GetItem(*this, registers[instruction.B], registers[instruction.C], result));
                registers = task.Stack.data() + base;
                registers[instruction.A] = result;
                break;
            }
            case Operation::SetIndex:
                EASYFORGE_FAIL_IF(!SetItem(*this, registers[instruction.A], registers[instruction.B], registers[instruction.C]));
                break;
            case Operation::BuildText:
            {
                std::string joined;
                for (int index = 0; index < instruction.C; ++index)
                {
                    joined += Engine.Display(registers[instruction.B + index]);
                }
                registers[instruction.A] = Value::Of(static_cast<Object*>(Engine.TheHeap.Text(joined)));
                break;
            }
            case Operation::RangeCheck:
            {
                Value current = registers[instruction.A];
                Value end = registers[instruction.A + 1];
                EASYFORGE_FAIL_IF((!current.IsNumber() || !end.IsNumber()) &&
                                  !Fail(std::format("for ... to counts with numbers, but this is {}",
                                      Engine.Describe(current.IsNumber() ? end : current))));
                if (current.Number > end.Number)
                {
                    frame.Next = instruction.Target();
                }
                else
                {
                    registers[instruction.A + 2] = current;
                }
                break;
            }
            case Operation::RangeStep:
                registers[instruction.A].Number += 1.0;
                frame.Next = instruction.Target();
                break;
            case Operation::EachStart:
            {
                Value collection = registers[instruction.A];
                bool fits = IsKind(collection, ObjectKind::List) || IsKind(collection, ObjectKind::Text) ||
                            IsKind(collection, ObjectKind::Table);
                EASYFORGE_FAIL_IF(!fits && !Fail(std::format("for goes through a list, a table, or a string, but this is {}",
                                                Engine.Describe(collection))));
                registers[instruction.A + 1] = Value::Of(0.0);
                break;
            }
            case Operation::EachNext:
            {
                Value collection = registers[instruction.A];
                std::size_t position = static_cast<std::size_t>(registers[instruction.A + 1].Number);
                bool done = true;
                if (IsKind(collection, ObjectKind::List))
                {
                    const std::vector<Value>& items = As<ListObject>(collection)->Items;
                    if (position < items.size())
                    {
                        registers[instruction.A + 2] = items[position];
                        registers[instruction.A + 1] = Value::Of(static_cast<double>(position + 1));
                        done = false;
                    }
                }
                else if (IsKind(collection, ObjectKind::Table))
                {
                    const auto& fields = As<TableObject>(collection)->Fields;
                    if (position < fields.size())
                    {
                        registers[instruction.A + 2] = Value::Of(static_cast<Object*>(fields[position].first));
                        registers[instruction.A + 1] = Value::Of(static_cast<double>(position + 1));
                        done = false;
                    }
                }
                else
                {
                    const std::string& characters = As<TextObject>(collection)->Text;
                    if (position < characters.size())
                    {
                        std::size_t next = NextCharacterAt(characters, position);
                        TextObject* character = Engine.TheHeap.Text(std::string_view(characters).substr(position, next - position));
                        registers[instruction.A + 2] = Value::Of(static_cast<Object*>(character));
                        registers[instruction.A + 1] = Value::Of(static_cast<double>(next));
                        done = false;
                    }
                }
                if (done)
                {
                    frame.Next = instruction.Target();
                }
                break;
            }
            case Operation::Spawn:
            {
                std::size_t slot = base + instruction.A;
                std::vector<Value> arguments(task.Stack.begin() + static_cast<std::ptrdiff_t>(slot + 1),
                    task.Stack.begin() + static_cast<std::ptrdiff_t>(slot + 1 + instruction.B));
                auto started = std::make_unique<Task>();
                started->Spawned = true;
                EASYFORGE_FAIL_IF(!Begin(*started, task.Stack[slot], arguments));
                Outcome outcome = Run(*started);
                if (outcome == Outcome::Suspended)
                {
                    Engine.Spawned.push_back(std::move(started));
                }
                EASYFORGE_FAIL_IF(outcome == Outcome::Failed && !Fatal);
                if (outcome == Outcome::Failed)
                {
                    return Outcome::Failed;
                }
                break;
            }
            case Operation::Wait:
            {
                EASYFORGE_FAIL_IF(!task.Spawned &&
                                  !Fail("wait and yield only work in a function started with spawn, such as spawn Blink()"));
                double seconds = -1.0;
                if (instruction.B)
                {
                    Value given = registers[instruction.A];
                    EASYFORGE_FAIL_IF(!given.IsNumber() &&
                                      !Fail(std::format("wait needs a number of seconds, but this is {}", Engine.Describe(given))));
                    seconds = std::max(given.Number, 0.0);
                }
                task.Waiting = true;
                task.WaitLeft = seconds;
                return Outcome::Suspended;
            }
            case Operation::TryStart:
                task.Handlers.push_back({ task.Frames.size(), instruction.Target(), base + instruction.A });
                break;
            case Operation::TryEnd:
                if (!task.Handlers.empty())
                {
                    task.Handlers.pop_back();
                }
                break;
            case Operation::Import:
            {
                std::string problem;
                ModuleObject* module = Engine.Import(text(instruction.B)->Text, function->Module, problem);
                EASYFORGE_FAIL_IF(!module && !Fail(problem));
                registers = task.Stack.data() + base;
                registers[instruction.A] = Value::Of(static_cast<Object*>(module->Globals));
                break;
            }
            case Operation::MakeRecord:
            {
                RecordObject* record = Engine.TheHeap.Make<RecordObject>();
                record->Name = text(instruction.B);
                for (int index = 0; index < instruction.C; ++index)
                {
                    record->Fields.push_back(text(static_cast<std::uint16_t>(instruction.B + 1 + index)));
                }
                registers[instruction.A] = Value::Of(static_cast<Object*>(record));
                break;
            }
            }
        }
#undef EASYFORGE_FAIL_IF
    }
}
