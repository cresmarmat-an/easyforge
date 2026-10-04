#include <easyforge/script/CInterface.h>

#include <deque>
#include <string>
#include <vector>

#include <easyforge/script/ScriptEngine.h>

using easyforge::Failure;
using easyforge::Result;
using easyforge::ScriptEngine;
using easyforge::ScriptValue;
using easyforge::ScriptValueKind;

struct easyforge_script
{
    ScriptEngine Engine;
    std::string Error;
    std::vector<ScriptValue> Pushed;
    ScriptValue Result;
    std::string ResultText;
};

struct easyforge_script_arguments
{
    const std::vector<ScriptValue>* Arguments = nullptr;

    // Text handed out for arguments, kept until the call ends.
    std::deque<std::string> Texts;
    ScriptValue Result;
    std::string Problem;
    bool Failed = false;
};

namespace
{
    int KindOf(const ScriptValue& value)
    {
        switch (value.Kind())
        {
        case ScriptValueKind::Nothing: return EASYFORGE_SCRIPT_NOTHING;
        case ScriptValueKind::Boolean: return EASYFORGE_SCRIPT_BOOLEAN;
        case ScriptValueKind::Number: return EASYFORGE_SCRIPT_NUMBER;
        case ScriptValueKind::Text: return EASYFORGE_SCRIPT_TEXT;
        default: return EASYFORGE_SCRIPT_OTHER;
        }
    }

    int Keep(easyforge_script* script, Result<ScriptValue> result)
    {
        script->Pushed.clear();
        if (!result)
        {
            script->Error = result.Error();
            script->Result = ScriptValue();
            return 0;
        }
        script->Result = *result;
        return 1;
    }

    const ScriptValue* Argument(const easyforge_script_arguments* call, int index)
    {
        if (!call || index < 0 || static_cast<std::size_t>(index) >= call->Arguments->size())
        {
            return nullptr;
        }
        return &(*call->Arguments)[static_cast<std::size_t>(index)];
    }
}

extern "C"
{
    easyforge_script* easyforge_script_new(void)
    {
        return easyforge_script_new_limited(0, 0);
    }

    easyforge_script* easyforge_script_new_limited(size_t memory_limit, unsigned long long instruction_limit)
    {
        auto* script = new easyforge_script();
        script->Engine = ScriptEngine::New({ .MemoryLimit = memory_limit, .InstructionLimit = instruction_limit });
        return script;
    }

    void easyforge_script_free(easyforge_script* script)
    {
        delete script;
    }

    int easyforge_script_run(easyforge_script* script, const char* source, const char* name)
    {
        return script ? Keep(script, script->Engine.Run(source ? source : "", name ? name : "script")) : 0;
    }

    int easyforge_script_run_file(easyforge_script* script, const char* path)
    {
        return script ? Keep(script, script->Engine.RunFile(path ? path : "")) : 0;
    }

    int easyforge_script_update(easyforge_script* script, float delta_seconds)
    {
        if (!script)
        {
            return 0;
        }
        Result<> updated = script->Engine.Update(delta_seconds);
        if (!updated)
        {
            script->Error = updated.Error();
            return 0;
        }
        return 1;
    }

    const char* easyforge_script_error(const easyforge_script* script)
    {
        return script ? script->Error.c_str() : "there is no engine";
    }

    void easyforge_script_push_nothing(easyforge_script* script)
    {
        if (script)
        {
            script->Pushed.emplace_back();
        }
    }

    void easyforge_script_push_boolean(easyforge_script* script, int value)
    {
        if (script)
        {
            script->Pushed.emplace_back(value != 0);
        }
    }

    void easyforge_script_push_number(easyforge_script* script, double value)
    {
        if (script)
        {
            script->Pushed.emplace_back(value);
        }
    }

    void easyforge_script_push_text(easyforge_script* script, const char* value)
    {
        if (script)
        {
            script->Pushed.emplace_back(value ? value : "");
        }
    }

    int easyforge_script_call(easyforge_script* script, const char* function)
    {
        if (!script)
        {
            return 0;
        }
        std::vector<ScriptValue> arguments = std::move(script->Pushed);
        return Keep(script, script->Engine.CallWith(function ? function : "", arguments));
    }

    int easyforge_script_result_kind(const easyforge_script* script)
    {
        return script ? KindOf(script->Result) : EASYFORGE_SCRIPT_NOTHING;
    }

    int easyforge_script_result_boolean(const easyforge_script* script)
    {
        return script && script->Result.AsBoolean() ? 1 : 0;
    }

    double easyforge_script_result_number(const easyforge_script* script)
    {
        return script ? script->Result.AsNumber() : 0.0;
    }

    const char* easyforge_script_result_text(easyforge_script* script)
    {
        if (!script)
        {
            return "";
        }
        script->ResultText = script->Result.AsText();
        return script->ResultText.c_str();
    }

    void easyforge_script_define(easyforge_script* script, const char* name, easyforge_script_function function, void* data)
    {
        if (!script || !name || !function)
        {
            return;
        }
        script->Engine.DefineValue(name,
            ScriptValue::FromFunction(name, [function, data](const std::vector<ScriptValue>& arguments) -> Result<ScriptValue> {
                easyforge_script_arguments call;
                call.Arguments = &arguments;
                function(&call, data);
                if (call.Failed)
                {
                    return Failure(call.Problem);
                }
                return call.Result;
            }));
    }

    void easyforge_script_define_number(easyforge_script* script, const char* name, double value)
    {
        if (script && name)
        {
            script->Engine.DefineValue(name, ScriptValue(value));
        }
    }

    void easyforge_script_define_text(easyforge_script* script, const char* name, const char* value)
    {
        if (script && name)
        {
            script->Engine.DefineValue(name, ScriptValue(value ? value : ""));
        }
    }

    int easyforge_script_argument_count(const easyforge_script_arguments* call)
    {
        return call ? static_cast<int>(call->Arguments->size()) : 0;
    }

    int easyforge_script_argument_kind(const easyforge_script_arguments* call, int index)
    {
        const ScriptValue* argument = Argument(call, index);
        return argument ? KindOf(*argument) : EASYFORGE_SCRIPT_NOTHING;
    }

    int easyforge_script_argument_boolean(const easyforge_script_arguments* call, int index)
    {
        const ScriptValue* argument = Argument(call, index);
        return argument && argument->AsBoolean() ? 1 : 0;
    }

    double easyforge_script_argument_number(const easyforge_script_arguments* call, int index)
    {
        const ScriptValue* argument = Argument(call, index);
        return argument ? argument->AsNumber() : 0.0;
    }

    const char* easyforge_script_argument_text(easyforge_script_arguments* call, int index)
    {
        const ScriptValue* argument = Argument(call, index);
        if (!argument)
        {
            return "";
        }
        call->Texts.push_back(argument->AsText());
        return call->Texts.back().c_str();
    }

    void easyforge_script_return_boolean(easyforge_script_arguments* call, int value)
    {
        if (call)
        {
            call->Result = ScriptValue(value != 0);
        }
    }

    void easyforge_script_return_number(easyforge_script_arguments* call, double value)
    {
        if (call)
        {
            call->Result = ScriptValue(value);
        }
    }

    void easyforge_script_return_text(easyforge_script_arguments* call, const char* value)
    {
        if (call)
        {
            call->Result = ScriptValue(value ? value : "");
        }
    }

    void easyforge_script_fail(easyforge_script_arguments* call, const char* message)
    {
        if (call)
        {
            call->Failed = true;
            call->Problem = message ? message : "the function failed";
        }
    }
}
