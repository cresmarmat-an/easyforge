#include <easyforge/script/ScriptEngine.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

#include "EngineState.h"

namespace easyforge
{
    using internal::scripting::EngineState;
    using internal::scripting::ListObject;
    using internal::scripting::Object;
    using internal::scripting::TableObject;
    using internal::scripting::Value;

    namespace
    {
        Result<ScriptValue> NoEngine()
        {
            return Failure("this script engine was never made; make one with ScriptEngine::New");
        }
    }

    ScriptEngine ScriptEngine::New(const ScriptSettings& settings)
    {
        return ScriptEngine(std::make_shared<EngineState>(settings));
    }

    Result<ScriptValue> ScriptEngine::Run(std::string_view source, std::string_view name)
    {
        if (!Engine)
        {
            return NoEngine();
        }
        return Engine->RunSource(source, name, {});
    }

    Result<ScriptValue> ScriptEngine::RunFile(std::string_view path)
    {
        if (!Engine)
        {
            return NoEngine();
        }
        std::ifstream file(std::filesystem::path(std::string(path)), std::ios::binary);
        if (!file)
        {
            return Failure(std::format("cannot read {}", path));
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        std::error_code error;
        std::string key = std::filesystem::weakly_canonical(std::filesystem::path(std::string(path)), error).generic_string();
        return Engine->RunSource(contents.str(), path, key.empty() ? std::string(path) : key);
    }

    std::vector<std::string> ScriptEngine::Check(std::string_view source, std::string_view name, bool unknownNames)
    {
        if (!Engine)
        {
            return { "this script engine was never made; make one with ScriptEngine::New" };
        }
        return Engine->CheckSource(source, name, unknownNames);
    }

    Result<ScriptValue> ScriptEngine::CallWith(std::string_view name, const std::vector<ScriptValue>& arguments)
    {
        if (!Engine)
        {
            return NoEngine();
        }
        const Value* function = Engine->FindName(name);
        if (!function)
        {
            return Failure(std::format("there is no {} to call", name));
        }
        return Engine->CallScript(*function, arguments);
    }

    void ScriptEngine::DefineValue(std::string_view name, const ScriptValue& value)
    {
        if (Engine)
        {
            Engine->Globals->Set(Engine->TheHeap.Text(name), Engine->ToValue(value));
        }
    }

    ScriptValue ScriptEngine::Get(std::string_view name) const
    {
        if (!Engine)
        {
            return {};
        }
        const Value* found = Engine->FindName(name);
        return found ? Engine->ToScript(*found) : ScriptValue();
    }

    Result<> ScriptEngine::Update(float deltaSeconds)
    {
        if (!Engine)
        {
            return Failure("this script engine was never made; make one with ScriptEngine::New");
        }
        return Engine->Update(deltaSeconds);
    }

    std::size_t ScriptEngine::RunningTasks() const
    {
        return Engine ? Engine->Spawned.size() : 0;
    }

    ScriptValue ScriptEngine::NewList(const std::vector<ScriptValue>& items)
    {
        if (!Engine)
        {
            return {};
        }
        ListObject* list = Engine->TheHeap.Make<ListObject>();
        ScriptValue held = Engine->ToScript(Value::Of(static_cast<Object*>(list)));
        for (const ScriptValue& item : items)
        {
            list->Items.push_back(Engine->ToValue(item));
        }
        Engine->TheHeap.Grew(*list, list->Items.size() * sizeof(Value));
        return held;
    }

    ScriptValue ScriptEngine::NewTable(const std::vector<std::pair<std::string, ScriptValue>>& fields)
    {
        if (!Engine)
        {
            return {};
        }
        TableObject* table = Engine->TheHeap.Make<TableObject>();
        ScriptValue held = Engine->ToScript(Value::Of(static_cast<Object*>(table)));
        for (const auto& [name, value] : fields)
        {
            table->Set(Engine->TheHeap.Text(name), Engine->ToValue(value));
        }
        return held;
    }

    std::size_t ScriptEngine::MemoryUsed() const
    {
        return Engine ? Engine->TheHeap.BytesUsed() : 0;
    }

    void ScriptEngine::Collect()
    {
        if (Engine && Engine->Running.empty())
        {
            Engine->Collect();
        }
    }
}
