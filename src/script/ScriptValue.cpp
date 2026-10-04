#include <easyforge/script/ScriptValue.h>

#include <easyforge/script/ScriptObject.h>

#include "Builtins.h"
#include "EngineState.h"

namespace easyforge
{
    using internal::scripting::EngineState;
    using internal::scripting::ListObject;
    using internal::scripting::ObjectKind;
    using internal::scripting::TableObject;
    using internal::scripting::Value;

    namespace internal
    {
        std::string_view OrdinalWord(std::size_t index)
        {
            static constexpr std::string_view words[] = { "first", "second", "third", "fourth", "fifth", "sixth",
                "seventh", "eighth", "ninth", "tenth" };
            return index < std::size(words) ? words[index] : "next";
        }

        std::string ScriptKindWord(const ScriptValue& value)
        {
            std::string name = value.TypeName();
            if (name == "nothing")
            {
                return name;
            }
            char first = name.empty() ? 'x' : name[0];
            bool vowel = first == 'a' || first == 'e' || first == 'i' || first == 'o' || first == 'u' || first == 'A' ||
                         first == 'E' || first == 'I' || first == 'O' || first == 'U';
            return (vowel ? "an " : "a ") + name;
        }

        bool IsScriptNumber(const ScriptValue& value) { return value.Kind() == ScriptValueKind::Number; }
        bool IsScriptBoolean(const ScriptValue& value) { return value.Kind() == ScriptValueKind::Boolean; }
        bool IsScriptText(const ScriptValue& value) { return value.Kind() == ScriptValueKind::Text; }
        bool IsScriptList(const ScriptValue& value) { return value.Kind() == ScriptValueKind::List; }

        Result<ScriptValue> NothingResult()
        {
            return ScriptValue();
        }
    }

    namespace
    {
        // The engine a value's object lives in, and the object, when both are still there.
        std::shared_ptr<EngineState> EngineOf(const std::shared_ptr<internal::scripting::Root>& root)
        {
            return root && root->Pointer ? root->Engine.lock() : nullptr;
        }
    }

    ScriptValue::ScriptValue(bool value) : ValueKind(ScriptValueKind::Boolean), Number(value ? 1.0 : 0.0) {}
    ScriptValue::ScriptValue(double value) : ValueKind(ScriptValueKind::Number), Number(value) {}
    ScriptValue::ScriptValue(float value) : ValueKind(ScriptValueKind::Number), Number(value) {}
    ScriptValue::ScriptValue(int value) : ValueKind(ScriptValueKind::Number), Number(value) {}
    ScriptValue::ScriptValue(long long value) : ValueKind(ScriptValueKind::Number), Number(static_cast<double>(value)) {}
    ScriptValue::ScriptValue(std::string value) : ValueKind(ScriptValueKind::Text), Text(std::move(value)) {}
    ScriptValue::ScriptValue(std::string_view value) : ValueKind(ScriptValueKind::Text), Text(value) {}
    ScriptValue::ScriptValue(const char* value) : ValueKind(ScriptValueKind::Text), Text(value ? value : "") {}

    ScriptValue::ScriptValue(std::shared_ptr<ScriptObject> object) : Held(std::move(object))
    {
        ValueKind = Held ? ScriptValueKind::Object : ScriptValueKind::Nothing;
    }

    ScriptValue ScriptValue::FromFunction(std::string name, ScriptFunction function)
    {
        ScriptValue value;
        value.ValueKind = ScriptValueKind::Function;
        value.Text = std::move(name);
        value.Native = std::make_shared<ScriptFunction>(std::move(function));
        return value;
    }

    ScriptValue ScriptValue::List(std::vector<ScriptValue> items)
    {
        ScriptValue value;
        value.ValueKind = ScriptValueKind::List;
        value.PendingItems = std::make_shared<std::vector<ScriptValue>>(std::move(items));
        return value;
    }

    ScriptValue ScriptValue::Table(std::vector<std::pair<std::string, ScriptValue>> fields)
    {
        ScriptValue value;
        value.ValueKind = ScriptValueKind::Table;
        value.PendingFields = std::make_shared<std::vector<std::pair<std::string, ScriptValue>>>(std::move(fields));
        return value;
    }

    bool ScriptValue::IsTrue() const
    {
        if (ValueKind == ScriptValueKind::Boolean)
        {
            return Number != 0.0;
        }
        return ValueKind != ScriptValueKind::Nothing;
    }

    double ScriptValue::AsNumber() const
    {
        return ValueKind == ScriptValueKind::Number ? Number : 0.0;
    }

    bool ScriptValue::AsBoolean() const
    {
        return ValueKind == ScriptValueKind::Boolean && Number != 0.0;
    }

    std::string ScriptValue::AsText() const
    {
        switch (ValueKind)
        {
        case ScriptValueKind::Nothing: return "nothing";
        case ScriptValueKind::Boolean: return Number != 0.0 ? "true" : "false";
        case ScriptValueKind::Number: return internal::scripting::NumberText(Number);
        case ScriptValueKind::Text: return Text;
        default: break;
        }
        if (std::shared_ptr<EngineState> engine = EngineOf(Rooted))
        {
            return engine->Display(Value::Of(Rooted->Pointer));
        }
        if (Held)
        {
            return Held->TypeName();
        }
        if (Native)
        {
            return "function " + Text;
        }
        if (PendingItems)
        {
            std::string text = "[";
            for (std::size_t index = 0; index < PendingItems->size(); ++index)
            {
                const ScriptValue& item = (*PendingItems)[index];
                text += (index > 0 ? ", " : "") + (item.Kind() == ScriptValueKind::Text ? "\"" + item.Text + "\"" : item.AsText());
            }
            return text + "]";
        }
        if (PendingFields)
        {
            if (PendingFields->empty())
            {
                return "{}";
            }
            std::string text = "{ ";
            for (std::size_t index = 0; index < PendingFields->size(); ++index)
            {
                const auto& [name, field] = (*PendingFields)[index];
                text += (index > 0 ? ", " : "") + name + " = " +
                        (field.Kind() == ScriptValueKind::Text ? "\"" + field.Text + "\"" : field.AsText());
            }
            return text + " }";
        }
        return "nothing";
    }

    std::vector<ScriptValue> ScriptValue::Items() const
    {
        if (PendingItems)
        {
            return *PendingItems;
        }
        std::vector<ScriptValue> items;
        std::shared_ptr<EngineState> engine = EngineOf(Rooted);
        if (!engine || Rooted->Pointer->Kind != ObjectKind::List)
        {
            return items;
        }
        for (const Value& item : static_cast<ListObject*>(Rooted->Pointer)->Items)
        {
            items.push_back(engine->ToScript(item));
        }
        return items;
    }

    std::size_t ScriptValue::Count() const
    {
        if (PendingItems)
        {
            return PendingItems->size();
        }
        if (PendingFields)
        {
            return PendingFields->size();
        }
        if (!EngineOf(Rooted))
        {
            return 0;
        }
        if (Rooted->Pointer->Kind == ObjectKind::List)
        {
            return static_cast<ListObject*>(Rooted->Pointer)->Items.size();
        }
        if (Rooted->Pointer->Kind == ObjectKind::Table)
        {
            return static_cast<TableObject*>(Rooted->Pointer)->Fields.size();
        }
        return 0;
    }

    ScriptValue ScriptValue::Field(std::string_view name) const
    {
        if (Held)
        {
            return Held->Get(name);
        }
        if (PendingFields)
        {
            for (const auto& [field, value] : *PendingFields)
            {
                if (field == name)
                {
                    return value;
                }
            }
            return {};
        }
        std::shared_ptr<EngineState> engine = EngineOf(Rooted);
        if (!engine || Rooted->Pointer->Kind != ObjectKind::Table)
        {
            return {};
        }
        const Value* found = static_cast<TableObject*>(Rooted->Pointer)->Find(engine->TheHeap.Text(name));
        return found ? engine->ToScript(*found) : ScriptValue();
    }

    std::vector<std::string> ScriptValue::FieldNames() const
    {
        std::vector<std::string> names;
        if (Held)
        {
            return Held->MemberNames();
        }
        if (PendingFields)
        {
            for (const auto& [name, value] : *PendingFields)
            {
                names.push_back(name);
            }
            return names;
        }
        if (!EngineOf(Rooted) || Rooted->Pointer->Kind != ObjectKind::Table)
        {
            return names;
        }
        for (const auto& [name, value] : static_cast<TableObject*>(Rooted->Pointer)->Fields)
        {
            names.push_back(name->Text);
        }
        return names;
    }

    bool ScriptValue::SetField(std::string_view name, const ScriptValue& value) const
    {
        if (Held)
        {
            return Held->Set(name, value);
        }
        if (PendingFields)
        {
            for (auto& [field, held] : *PendingFields)
            {
                if (field == name)
                {
                    held = value;
                    return true;
                }
            }
            PendingFields->emplace_back(std::string(name), value);
            return true;
        }
        std::shared_ptr<EngineState> engine = EngineOf(Rooted);
        if (!engine || Rooted->Pointer->Kind != ObjectKind::Table)
        {
            return false;
        }
        static_cast<TableObject*>(Rooted->Pointer)->Set(engine->TheHeap.Text(name), engine->ToValue(value));
        return true;
    }

    bool ScriptValue::Add(const ScriptValue& item) const
    {
        if (PendingItems)
        {
            PendingItems->push_back(item);
            return true;
        }
        std::shared_ptr<EngineState> engine = EngineOf(Rooted);
        if (!engine || Rooted->Pointer->Kind != ObjectKind::List)
        {
            return false;
        }
        auto* list = static_cast<ListObject*>(Rooted->Pointer);
        list->Items.push_back(engine->ToValue(item));
        engine->TheHeap.Grew(*list, sizeof(Value));
        return true;
    }

    std::shared_ptr<ScriptObject> ScriptValue::Object() const
    {
        return Held;
    }

    Result<ScriptValue> ScriptValue::Call(const std::vector<ScriptValue>& arguments) const
    {
        if (Native)
        {
            return (*Native)(arguments);
        }
        std::shared_ptr<EngineState> engine = EngineOf(Rooted);
        if (!engine)
        {
            return Failure(ValueKind == ScriptValueKind::Function ? "the engine this function came from is gone"
                                                                  : "this value is not a function");
        }
        return engine->CallScript(Value::Of(Rooted->Pointer), arguments);
    }

    std::string ScriptValue::TypeName() const
    {
        switch (ValueKind)
        {
        case ScriptValueKind::Nothing: return "nothing";
        case ScriptValueKind::Boolean: return "boolean";
        case ScriptValueKind::Number: return "number";
        case ScriptValueKind::Text: return "string";
        case ScriptValueKind::List: return "list";
        case ScriptValueKind::Function: return "function";
        default: break;
        }
        if (Held)
        {
            return Held->TypeName();
        }
        if (std::shared_ptr<EngineState> engine = EngineOf(Rooted))
        {
            return engine->TypeName(Value::Of(Rooted->Pointer));
        }
        return "table";
    }

    bool ScriptValue::operator==(const ScriptValue& other) const
    {
        if (ValueKind != other.ValueKind)
        {
            return false;
        }
        switch (ValueKind)
        {
        case ScriptValueKind::Nothing: return true;
        case ScriptValueKind::Boolean:
        case ScriptValueKind::Number: return Number == other.Number;
        case ScriptValueKind::Text: return Text == other.Text;
        default: break;
        }
        if (Held || other.Held)
        {
            return Held == other.Held;
        }
        if (Native || other.Native)
        {
            return Native == other.Native;
        }
        if (PendingItems || other.PendingItems || PendingFields || other.PendingFields)
        {
            return PendingItems == other.PendingItems && PendingFields == other.PendingFields;
        }
        return Rooted && other.Rooted && Rooted->Pointer == other.Rooted->Pointer;
    }
}
