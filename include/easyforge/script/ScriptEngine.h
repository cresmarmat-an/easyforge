#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <easyforge/core/Result.h>
#include <easyforge/script/ScriptObject.h>
#include <easyforge/script/ScriptValue.h>

namespace easyforge
{
    namespace internal::scripting
    {
        class EngineState;
    }

    struct ScriptSettings
    {
        // The most memory the scripts' values may use, in bytes. Zero is no
        // limit. A script that needs more stops with an error.
        std::size_t MemoryLimit = 0;

        // The most instructions one Run, Call, or Update may carry out. Zero is
        // no limit. A script that loops forever stops with an error instead.
        std::uint64_t InstructionLimit = 0;

        // Where `import "name"` looks for name.script after the folder of the
        // file that imports it.
        std::vector<std::string> SearchFolders;

        // Whether scripts may import files at all.
        bool AllowImports = true;

        // Where print writes. Without one, to the standard output.
        std::function<void(std::string_view line)> Print;
    };

    // Runs scripts in the easyforge language. Each engine has its own values
    // and names; two engines share nothing.
    //
    //     ScriptEngine scripts = ScriptEngine::New({ .MemoryLimit = 64 * 1024 * 1024 });
    //     scripts.Define("Shout", [](std::string text) { return text + "!"; });
    //     Result<ScriptValue> ran = scripts.RunFile("greet.script");
    //     if (!ran)
    //     {
    //         Log(ran.Error());   // "greet.script:3:9: Shout takes 1 argument, but was given 2"
    //     }
    //     Result<ScriptValue> result = scripts.Call("SomeName", "Hello");
    //
    // Scripts can only reach what the program gives them with Define: there is
    // no file or network access of their own, apart from importing other
    // scripts when the settings allow it. An engine is used from one thread.
    class ScriptEngine
    {
    public:
        // An engine that runs nothing. Tests as false.
        ScriptEngine() = default;

        static ScriptEngine New(const ScriptSettings& settings = {});

        explicit operator bool() const { return Engine != nullptr; }

        // Runs source text as a module of its own. `name` is used in messages
        // and for finding imports. The result is the value a top-level `return`
        // gives, or nothing.
        Result<ScriptValue> Run(std::string_view source, std::string_view name = "script");

        // Reads and runs a file.
        Result<ScriptValue> RunFile(std::string_view path);

        // Reads source text and checks it, including the types written in it,
        // without running it. Returns every problem, as "name:line:column:
        // message", in the order they appear. Names nothing defines are problems
        // too, unless `unknownNames` is false: a script checked away from its
        // program uses names only the program defines.
        std::vector<std::string> Check(std::string_view source, std::string_view name = "script", bool unknownNames = true);

        // Calls a function a script defined, or one the program defined.
        template <typename... Arguments>
        Result<ScriptValue> Call(std::string_view name, Arguments&&... arguments)
        {
            return CallWith(name, { ScriptValue(std::forward<Arguments>(arguments))... });
        }
        Result<ScriptValue> CallWith(std::string_view name, const std::vector<ScriptValue>& arguments);

        // Gives scripts a name: a function, any value, or an object of your own.
        // See ScriptValue::Function for the functions it takes.
        template <typename Callable>
            requires requires { std::function(std::declval<std::decay_t<Callable>>()); }
        void Define(std::string_view name, Callable&& callable)
        {
            DefineValue(name, ScriptValue::Function(std::string(name), std::forward<Callable>(callable)));
        }
        void Define(std::string_view name, const ScriptValue& value) { DefineValue(name, value); }

        // A template, so that Define("Score", 0) is a number and not an empty object.
        template <typename Object>
            requires std::is_base_of_v<ScriptObject, Object>
        void Define(std::string_view name, std::shared_ptr<Object> object)
        {
            DefineValue(name, ScriptValue(std::shared_ptr<ScriptObject>(std::move(object))));
        }
        void DefineValue(std::string_view name, const ScriptValue& value);

        // A name scripts have defined at their top level, or the program has;
        // nothing when there is none.
        ScriptValue Get(std::string_view name) const;

        // Moves functions started with `spawn` along: those waiting for time
        // count down, and those that yielded continue. Call it once a frame.
        // Returns the first error a function met, if one did.
        Result<> Update(float deltaSeconds);

        // How many spawned functions have not finished.
        std::size_t RunningTasks() const;

        // New lists and tables to give scripts.
        ScriptValue NewList(const std::vector<ScriptValue>& items = {});
        ScriptValue NewTable(const std::vector<std::pair<std::string, ScriptValue>>& fields = {});

        // Memory the scripts' values use now, in bytes, and a collection of
        // what they no longer use. Collections also happen by themselves.
        std::size_t MemoryUsed() const;
        void Collect();

    private:
        explicit ScriptEngine(std::shared_ptr<internal::scripting::EngineState> state) : Engine(std::move(state)) {}

        std::shared_ptr<internal::scripting::EngineState> Engine;
    };
}
