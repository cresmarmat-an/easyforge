#pragma once

// The functions every script has, such as print and SquareRoot, and the members
// of lists and strings, such as items.Add(3) and name.Length.

#include <span>
#include <string>

#include "Heap.h"

namespace easyforge::internal::scripting
{
    class EngineState;
    class Machine;

    // Defines the built-in functions in the engine's names.
    void InstallBuiltins(EngineState& engine);

    enum class MemberResult
    {
        Found,
        Missing,
        Failed,
    };

    // Calls a member function of a list or a string: `arguments` starts with the
    // list or string itself.
    MemberResult CallMember(Machine& machine, TextObject* name, std::span<Value> arguments, Value& result);

    // `object.name`, read and assigned.
    bool GetMember(Machine& machine, Value object, TextObject* name, Value& result);
    bool SetMember(Machine& machine, Value object, TextObject* name, Value value);

    // `object[index]`, read and assigned.
    bool GetItem(Machine& machine, Value object, Value index, Value& result);
    bool SetItem(Machine& machine, Value object, Value index, Value value);

    // A number as scripts show it: 3 for a whole number, 0.5 otherwise.
    std::string NumberText(double number);
}
