#include "EngineState.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

#include <easyforge/core/language/Syntax.h>

#include "Builtins.h"
#include "Compiler.h"

namespace easyforge::internal::scripting
{
    namespace
    {
        bool IsKind(Value value, ObjectKind kind) { return value.IsReference() && value.Pointer->Kind == kind; }

        template <typename Type>
        Type* As(Value value)
        {
            return static_cast<Type*>(value.Pointer);
        }

        std::string WithArticle(const std::string& name)
        {
            if (name == "nothing")
            {
                return name;
            }
            char first = name.empty() ? 'x' : static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
            bool vowel = first == 'a' || first == 'e' || first == 'i' || first == 'o' || first == 'u';
            return (vowel ? "an " : "a ") + name;
        }

        bool ReadWhole(const std::filesystem::path& path, std::string& text)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }
            std::ostringstream contents;
            contents << file.rdbuf();
            text = contents.str();
            return true;
        }

        std::string DescribeProblems(const std::vector<language::Problem>& problems, std::string_view name)
        {
            std::string text;
            for (const language::Problem& problem : problems)
            {
                if (!text.empty())
                {
                    text += '\n';
                }
                text += language::Describe(problem, name);
            }
            return text;
        }

        // A script calling the program, which calls a script again, nests on the
        // program's own stack, so that nesting has a smaller limit than calls
        // between script functions.
        constexpr int DeepestEntries = 32;

        // Counts the public calls into the engine, so nested calls share one limit.
        struct Entry
        {
            explicit Entry(EngineState& engine) : Engine(engine) { Engine.StartEntry(); }
            ~Entry() { Engine.FinishEntry(); }
            EngineState& Engine;
        };
    }

    Root::~Root()
    {
        if (std::shared_ptr<EngineState> engine = Engine.lock())
        {
            engine->Roots.erase(this);
        }
    }

    EngineState::EngineState(const ScriptSettings& settings) : Settings(settings)
    {
        Globals = TheHeap.Make<TableObject>();
        InstallBuiltins(*this);
    }

    EngineState::~EngineState()
    {
        for (Root* root : Roots)
        {
            root->Pointer = nullptr;
        }
        Roots.clear();
    }

    // ---- Entries and limits -----------------------------------------------------------

    void EngineState::StartEntry()
    {
        if (Entries++ == 0)
        {
            InstructionsLeft = Settings.InstructionLimit;
            TheMachine.Fatal = false;
        }
    }

    void EngineState::FinishEntry()
    {
        --Entries;
    }

    bool EngineState::CountInstruction()
    {
        if (Settings.InstructionLimit == 0)
        {
            return true;
        }
        if (InstructionsLeft == 0)
        {
            return false;
        }
        --InstructionsLeft;
        return true;
    }

    bool EngineState::BetweenInstructions(std::string& problem)
    {
        if (TheHeap.WantsCollection())
        {
            Collect();
        }
        if (Settings.MemoryLimit != 0 && TheHeap.BytesUsed() > Settings.MemoryLimit)
        {
            Collect();
            if (TheHeap.BytesUsed() > Settings.MemoryLimit)
            {
                problem = std::format("the scripts need more than {} bytes of memory", Settings.MemoryLimit);
                return false;
            }
        }
        return true;
    }

    void EngineState::Collect()
    {
        TheHeap.Mark(Globals);
        for (ModuleObject* module : Modules)
        {
            TheHeap.Mark(module);
        }
        auto markTask = [&](const Task& task) {
            for (const Value& value : task.Stack)
            {
                TheHeap.Mark(value);
            }
            for (const Frame& frame : task.Frames)
            {
                TheHeap.Mark(frame.Function);
            }
            TheHeap.Mark(task.Result);
        };
        for (const Task* task : Running)
        {
            markTask(*task);
        }
        for (const std::unique_ptr<Task>& task : Spawned)
        {
            markTask(*task);
        }
        for (const Root* root : Roots)
        {
            TheHeap.Mark(root->Pointer);
        }
        TheHeap.Trace();
        std::erase_if(Hosts, [](const auto& entry) { return !entry.second->Marked; });
        TheHeap.Sweep();
    }

    // ---- Running -------------------------------------------------------------------

    Result<ScriptValue> EngineState::RunSource(std::string_view source, std::string_view name, std::string_view path)
    {
        Entry entry(*this);
        language::SyntaxTree tree = language::Parse(source);
        CompileSettings settings;
        settings.IsDefined = [this](std::string_view defined) { return Globals->Find(TheHeap.Text(defined)) != nullptr; };
        Compiled compiled = Compile(tree, TheHeap, settings);
        if (!compiled.Problems.empty())
        {
            return Failure(DescribeProblems(compiled.Problems, name));
        }

        // Source run under a name it was run under before joins that module, so
        // its names are still there: how a prompt keeps what was typed.
        std::string key = path.empty() ? "run:" + std::string(name) : std::string(path);
        ModuleObject* module = nullptr;
        auto found = ModulesByPath.find(key);
        if (found != ModulesByPath.end())
        {
            module = found->second;
        }
        else
        {
            module = TheHeap.Make<ModuleObject>();
            module->Name = std::string(name);
            module->Path = std::string(path);
            module->Globals = TheHeap.Make<TableObject>();
            Modules.push_back(module);
            ModulesByPath.emplace(key, module);
        }
        module->Code.push_back(std::move(compiled.Main));
        return RunModule(*module);
    }

    Result<ScriptValue> EngineState::RunModule(ModuleObject& module)
    {
        FunctionObject* main = TheHeap.Make<FunctionObject>();
        main->Code = module.Code.back().get();
        main->Module = &module;
        Task task;
        if (!TheMachine.Begin(task, Value::Of(static_cast<Object*>(main)), {}))
        {
            return Failure(TheMachine.Problem);
        }
        module.Running = true;
        Outcome outcome = TheMachine.Run(task);
        module.Running = false;
        if (outcome == Outcome::Failed)
        {
            return Failure(TheMachine.Problem);
        }
        module.Ran = true;
        return ToScript(task.Result);
    }

    std::vector<std::string> EngineState::CheckSource(std::string_view source, std::string_view name, bool unknownNames)
    {
        language::SyntaxTree tree = language::Parse(source);
        CompileSettings settings;
        settings.IsDefined = [this](std::string_view defined) { return Globals->Find(TheHeap.Text(defined)) != nullptr; };
        settings.ReportUnknownNames = unknownNames;
        Compiled compiled = Compile(tree, TheHeap, settings);
        std::vector<std::string> problems;
        for (const language::Problem& problem : compiled.Problems)
        {
            problems.push_back(language::Describe(problem, name));
        }
        return problems;
    }

    Result<Value> EngineState::CallValue(Value function, std::span<const Value> arguments)
    {
        Entry entry(*this);
        if (Entries > DeepestEntries)
        {
            return Failure(std::format("the program and its scripts call each other more than {} deep", DeepestEntries));
        }
        if (IsKind(function, ObjectKind::Function))
        {
            Task task;
            if (!TheMachine.Begin(task, function, arguments))
            {
                return Failure(TheMachine.Problem);
            }
            if (TheMachine.Run(task) == Outcome::Failed)
            {
                return Failure(TheMachine.Problem);
            }
            return task.Result;
        }
        if (IsKind(function, ObjectKind::Native) || IsKind(function, ObjectKind::Record))
        {
            // Run as a call from a task of one instruction's worth: put the callee
            // and arguments in a stack and let the machine call it there.
            Task task;
            task.Stack.assign(1, function);
            task.Stack.insert(task.Stack.end(), arguments.begin(), arguments.end());
            Running.push_back(&task);
            struct Leave
            {
                EngineState& Engine;
                ~Leave() { Engine.Running.pop_back(); }
            } leave { *this };
            std::vector<Value> given(arguments.begin(), arguments.end());
            if (IsKind(function, ObjectKind::Native))
            {
                NativeObject* native = As<NativeObject>(function);
                Value result;
                if (native->Builtin)
                {
                    if (!native->Bound.IsNothing())
                    {
                        given.insert(given.begin(), native->Bound);
                    }
                    if (!native->Builtin(TheMachine, given, result))
                    {
                        return Failure(TheMachine.Problem);
                    }
                    return result;
                }
                std::vector<ScriptValue> values;
                for (const Value& argument : given)
                {
                    values.push_back(ToScript(argument));
                }
                Result<ScriptValue> returned = native->Program(values);
                if (!returned)
                {
                    return Failure(returned.Error());
                }
                return ToValue(*returned);
            }
            RecordObject* record = As<RecordObject>(function);
            TableObject* table = TheHeap.Make<TableObject>();
            table->Record = record;
            for (std::size_t index = 0; index < record->Fields.size(); ++index)
            {
                table->Set(record->Fields[index], index < given.size() ? given[index] : Value());
            }
            return Value::Of(static_cast<Object*>(table));
        }
        return Failure(std::format("{} cannot be called", Describe(function)));
    }

    Result<ScriptValue> EngineState::CallScript(Value function, const std::vector<ScriptValue>& arguments)
    {
        std::vector<Value> values;
        values.reserve(arguments.size());
        for (const ScriptValue& argument : arguments)
        {
            values.push_back(ToValue(argument));
        }
        Result<Value> result = CallValue(function, values);
        if (!result)
        {
            return Failure(result.Error());
        }
        return ToScript(*result);
    }

    Result<> EngineState::Update(double deltaSeconds)
    {
        Entry entry(*this);
        std::vector<Task*> waiting;
        for (const std::unique_ptr<Task>& task : Spawned)
        {
            waiting.push_back(task.get());
        }
        std::vector<Task*> done;
        std::string firstProblem;
        for (Task* task : waiting)
        {
            if (task->WaitLeft >= 0.0)
            {
                task->WaitLeft -= deltaSeconds;
                if (task->WaitLeft > 0.0)
                {
                    continue;
                }
            }
            task->Waiting = false;
            Outcome outcome = TheMachine.Run(*task);
            if (outcome != Outcome::Suspended)
            {
                done.push_back(task);
            }
            if (outcome == Outcome::Failed && firstProblem.empty())
            {
                firstProblem = TheMachine.Problem;
            }
            if (TheMachine.Fatal)
            {
                break;
            }
        }
        std::erase_if(Spawned, [&](const std::unique_ptr<Task>& task) {
            return std::find(done.begin(), done.end(), task.get()) != done.end();
        });
        if (!firstProblem.empty())
        {
            return Failure(firstProblem);
        }
        return {};
    }

    ModuleObject* EngineState::Import(std::string_view name, const ModuleObject* from, std::string& problem)
    {
        if (!Settings.AllowImports)
        {
            problem = "this program does not let scripts import other files";
            return nullptr;
        }
        std::string file(name);
        if (!file.ends_with(".script"))
        {
            file += ".script";
        }
        std::vector<std::filesystem::path> candidates;
        if (from && !from->Path.empty())
        {
            candidates.push_back(std::filesystem::path(from->Path).parent_path() / file);
        }
        for (const std::string& folder : Settings.SearchFolders)
        {
            candidates.push_back(std::filesystem::path(folder) / file);
        }
        if (candidates.empty())
        {
            candidates.push_back(file);
        }
        std::error_code error;
        for (const std::filesystem::path& candidate : candidates)
        {
            if (!std::filesystem::is_regular_file(candidate, error))
            {
                continue;
            }
            std::string key = std::filesystem::weakly_canonical(candidate, error).generic_string();
            auto found = ModulesByPath.find(key);
            if (found != ModulesByPath.end())
            {
                if (found->second->Running)
                {
                    problem = std::format("{} is still starting: scripts that import each other cannot both run first", file);
                    return nullptr;
                }
                if (found->second->Ran)
                {
                    return found->second;
                }
            }
            std::string source;
            if (!ReadWhole(candidate, source))
            {
                problem = std::format("cannot read {}", candidate.generic_string());
                return nullptr;
            }
            Result<ScriptValue> ran = RunSource(source, candidate.generic_string(), key);
            if (!ran)
            {
                problem = ran.Error();
                return nullptr;
            }
            return ModulesByPath[key];
        }
        problem = std::format("there is no {} to import", file);
        return nullptr;
    }

    const Value* EngineState::FindName(std::string_view name)
    {
        TextObject* text = TheHeap.Text(name);
        for (auto module = Modules.rbegin(); module != Modules.rend(); ++module)
        {
            if (const Value* found = (*module)->Globals->Find(text))
            {
                return found;
            }
        }
        return Globals->Find(text);
    }

    // ---- Values ----------------------------------------------------------------------

    std::shared_ptr<Root> EngineState::Hold(Object* object)
    {
        auto root = std::make_shared<Root>();
        root->Engine = weak_from_this();
        root->Pointer = object;
        Roots.insert(root.get());
        return root;
    }

    Value EngineState::ToValue(const ScriptValue& value)
    {
        switch (value.ValueKind)
        {
        case ScriptValueKind::Nothing: return Value();
        case ScriptValueKind::Boolean: return Value::Of(value.Number != 0.0);
        case ScriptValueKind::Number: return Value::Of(value.Number);
        case ScriptValueKind::Text: return Value::Of(static_cast<Object*>(TheHeap.Text(value.Text)));
        default: break;
        }
        if (value.Rooted)
        {
            std::shared_ptr<EngineState> owner = value.Rooted->Engine.lock();
            if (owner.get() == this && value.Rooted->Pointer)
            {
                return Value::Of(value.Rooted->Pointer);
            }
        }
        if (value.PendingItems)
        {
            ListObject* list = TheHeap.Make<ListObject>();
            for (const ScriptValue& item : *value.PendingItems)
            {
                list->Items.push_back(ToValue(item));
            }
            TheHeap.Grew(*list, list->Items.size() * sizeof(Value));
            return Value::Of(static_cast<Object*>(list));
        }
        if (value.PendingFields)
        {
            TableObject* table = TheHeap.Make<TableObject>();
            for (const auto& [name, field] : *value.PendingFields)
            {
                table->Set(TheHeap.Text(name), ToValue(field));
            }
            TheHeap.Grew(*table, table->Fields.size() * sizeof(std::pair<TextObject*, Value>) * 2);
            return Value::Of(static_cast<Object*>(table));
        }
        if (value.Native)
        {
            NativeObject* native = TheHeap.Make<NativeObject>();
            native->Name = value.Text;
            native->Program = *value.Native;
            return Value::Of(static_cast<Object*>(native));
        }
        if (value.Held)
        {
            auto found = Hosts.find(value.Held.get());
            if (found != Hosts.end())
            {
                return Value::Of(static_cast<Object*>(found->second));
            }
            HostObject* host = TheHeap.Make<HostObject>();
            host->Held = value.Held;
            Hosts.emplace(value.Held.get(), host);
            return Value::Of(static_cast<Object*>(host));
        }
        return Value();
    }

    ScriptValue EngineState::ToScript(Value value)
    {
        ScriptValue result;
        switch (value.Type)
        {
        case ValueType::Nothing: return result;
        case ValueType::Boolean: return ScriptValue(value.Boolean);
        case ValueType::Number: return ScriptValue(value.Number);
        case ValueType::Reference: break;
        }
        Object* object = value.Pointer;
        switch (object->Kind)
        {
        case ObjectKind::Text: return ScriptValue(static_cast<TextObject*>(object)->Text);
        case ObjectKind::List: result.ValueKind = ScriptValueKind::List; break;
        case ObjectKind::Table: result.ValueKind = ScriptValueKind::Table; break;
        case ObjectKind::Function:
        case ObjectKind::Native:
        case ObjectKind::Record: result.ValueKind = ScriptValueKind::Function; break;
        case ObjectKind::Host:
            result.ValueKind = ScriptValueKind::Object;
            result.Held = static_cast<HostObject*>(object)->Held;
            break;
        case ObjectKind::Box:
        case ObjectKind::Module: return result;
        }
        result.Rooted = Hold(object);
        return result;
    }

    std::string EngineState::Display(Value value, bool quoted, int depth) const
    {
        switch (value.Type)
        {
        case ValueType::Nothing: return "nothing";
        case ValueType::Boolean: return value.Boolean ? "true" : "false";
        case ValueType::Number: return NumberText(value.Number);
        case ValueType::Reference: break;
        }
        Object* object = value.Pointer;
        switch (object->Kind)
        {
        case ObjectKind::Text:
        {
            const std::string& text = static_cast<TextObject*>(object)->Text;
            return quoted ? "\"" + text + "\"" : text;
        }
        case ObjectKind::List:
        {
            const std::vector<Value>& items = static_cast<ListObject*>(object)->Items;
            if (depth > 3 && !items.empty())
            {
                return "[...]";
            }
            std::string text = "[";
            for (std::size_t index = 0; index < items.size(); ++index)
            {
                text += (index > 0 ? ", " : "") + Display(items[index], true, depth + 1);
            }
            return text + "]";
        }
        case ObjectKind::Table:
        {
            auto* table = static_cast<TableObject*>(object);
            std::string prefix = table->Record ? table->Record->Name->Text + " " : std::string();
            if (table->Fields.empty())
            {
                return prefix + "{}";
            }
            if (depth > 3)
            {
                return prefix + "{ ... }";
            }
            std::string text = prefix + "{ ";
            for (std::size_t index = 0; index < table->Fields.size(); ++index)
            {
                const auto& [name, field] = table->Fields[index];
                text += (index > 0 ? ", " : "") + name->Text + " = " + Display(field, true, depth + 1);
            }
            return text + " }";
        }
        case ObjectKind::Function:
        {
            // A function written without a name is called "a function" in errors.
            const std::string& name = static_cast<FunctionObject*>(object)->Code->Name;
            return name == "a function" ? std::string("function") : "function " + name;
        }
        case ObjectKind::Native: return "function " + static_cast<NativeObject*>(object)->Name;
        case ObjectKind::Record: return "type " + static_cast<RecordObject*>(object)->Name->Text;
        case ObjectKind::Host: return static_cast<HostObject*>(object)->Held->TypeName();
        case ObjectKind::Box: return Display(static_cast<BoxObject*>(object)->Held, quoted, depth);
        case ObjectKind::Module: return "module " + static_cast<ModuleObject*>(object)->Name;
        }
        return "nothing";
    }

    std::string EngineState::TypeName(Value value) const
    {
        switch (value.Type)
        {
        case ValueType::Nothing: return "nothing";
        case ValueType::Boolean: return "boolean";
        case ValueType::Number: return "number";
        case ValueType::Reference: break;
        }
        switch (value.Pointer->Kind)
        {
        case ObjectKind::Text: return "string";
        case ObjectKind::List: return "list";
        case ObjectKind::Table:
        {
            auto* table = static_cast<TableObject*>(value.Pointer);
            return table->Record ? table->Record->Name->Text : "table";
        }
        case ObjectKind::Function:
        case ObjectKind::Native:
        case ObjectKind::Record: return "function";
        case ObjectKind::Host: return static_cast<HostObject*>(value.Pointer)->Held->TypeName();
        case ObjectKind::Box: return TypeName(static_cast<BoxObject*>(value.Pointer)->Held);
        case ObjectKind::Module: return "table";
        }
        return "nothing";
    }

    std::string EngineState::Describe(Value value) const
    {
        return WithArticle(TypeName(value));
    }

    bool EngineState::Same(Value first, Value second) const
    {
        if (first.Type != second.Type)
        {
            return false;
        }
        switch (first.Type)
        {
        case ValueType::Nothing: return true;
        case ValueType::Boolean: return first.Boolean == second.Boolean;
        case ValueType::Number: return first.Number == second.Number;
        case ValueType::Reference: return first.Pointer == second.Pointer;
        }
        return false;
    }
}
