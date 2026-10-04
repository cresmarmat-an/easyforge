#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <easyforge/core/Result.h>
#include <easyforge/script/ScriptEngine.h>

#include "Heap.h"
#include "Machine.h"

namespace easyforge::internal::scripting
{
    class EngineState;

    // What keeps an engine's list, table, function, or object alive while a
    // ScriptValue holds it.
    struct Root
    {
        std::weak_ptr<EngineState> Engine;
        Object* Pointer = nullptr;
        ~Root();
    };

    class EngineState : public std::enable_shared_from_this<EngineState>
    {
    public:
        explicit EngineState(const ScriptSettings& settings);
        ~EngineState();

        // ---- Running ----------------------------------------------------------

        // Reads, checks, and compiles source as the module named `name`, then runs it.
        Result<ScriptValue> RunSource(std::string_view source, std::string_view name, std::string_view path);
        std::vector<std::string> CheckSource(std::string_view source, std::string_view name, bool unknownNames);

        // Calls a function value to the end, in a task of its own.
        Result<Value> CallValue(Value function, std::span<const Value> arguments);
        Result<ScriptValue> CallScript(Value function, const std::vector<ScriptValue>& arguments);

        Result<> Update(double deltaSeconds);

        // The module a script imports, run the first time.
        ModuleObject* Import(std::string_view name, const ModuleObject* from, std::string& problem);

        // A name the program defined, or any module defined at its top level.
        const Value* FindName(std::string_view name);

        // Counts an instruction toward the limit; false once it is used up.
        bool CountInstruction();
        void StartEntry();
        void FinishEntry();

        // Collects when the heap asks for it, and checks the memory limit.
        bool BetweenInstructions(std::string& problem);
        void Collect();

        // ---- Values ------------------------------------------------------------

        Value ToValue(const ScriptValue& value);
        ScriptValue ToScript(Value value);
        std::shared_ptr<Root> Hold(Object* object);

        // As print shows it.
        std::string Display(Value value, bool quoted = false, int depth = 0) const;

        // "a number", "a string", "nothing", "a Point".
        std::string Describe(Value value) const;
        std::string TypeName(Value value) const;

        bool Same(Value first, Value second) const;

        ScriptSettings Settings;
        Heap TheHeap;
        Machine TheMachine { *this };

        // The names the program and the built-ins define.
        TableObject* Globals = nullptr;

        std::vector<ModuleObject*> Modules;
        std::unordered_map<std::string, ModuleObject*> ModulesByPath;

        std::vector<std::unique_ptr<Task>> Spawned;
        std::vector<Task*> Running;
        std::unordered_set<Root*> Roots;
        std::unordered_map<ScriptObject*, HostObject*> Hosts;

        std::uint64_t InstructionsLeft = 0;
        int Entries = 0;

    private:
        Result<ScriptValue> RunModule(ModuleObject& module);
    };

}
