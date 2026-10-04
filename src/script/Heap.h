#pragma once

// The values scripts work with, and the heap their lists, tables, functions,
// and text live in, which collects what nothing can reach any more.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <easyforge/core/language/Tokens.h>
#include <easyforge/script/ScriptFunctions.h>
#include <easyforge/script/ScriptObject.h>

namespace easyforge::internal::scripting
{
    struct Object;
    struct TextObject;
    class Machine;

    enum class ValueType : std::uint8_t
    {
        Nothing,
        Boolean,
        Number,
        Reference,
    };

    // A value in a register: nothing, a boolean, a number, or a reference to an
    // object on the heap.
    struct Value
    {
        ValueType Type = ValueType::Nothing;
        union
        {
            bool Boolean;
            double Number;
            Object* Pointer;
        };

        Value() : Number(0.0) {}
        static Value Of(bool boolean)
        {
            Value value;
            value.Type = ValueType::Boolean;
            value.Boolean = boolean;
            return value;
        }
        static Value Of(double number)
        {
            Value value;
            value.Type = ValueType::Number;
            value.Number = number;
            return value;
        }
        static Value Of(Object* object)
        {
            Value value;
            if (object)
            {
                value.Type = ValueType::Reference;
                value.Pointer = object;
            }
            return value;
        }

        bool IsNothing() const { return Type == ValueType::Nothing; }
        bool IsNumber() const { return Type == ValueType::Number; }
        bool IsBoolean() const { return Type == ValueType::Boolean; }
        bool IsReference() const { return Type == ValueType::Reference; }

        // False for nothing and false, true for everything else.
        bool IsTrue() const { return Type == ValueType::Boolean ? Boolean : Type != ValueType::Nothing; }
    };

    enum class ObjectKind : std::uint8_t
    {
        Text,
        List,
        Table,
        Function,
        Native,
        Host,
        Record,
        Box,
        Module,
    };

    struct Object
    {
        explicit Object(ObjectKind kind) : Kind(kind) {}
        virtual ~Object() = default;

        ObjectKind Kind;
        bool Marked = false;
        Object* Next = nullptr;

        // About how many bytes the object holds, for the memory limit.
        std::size_t Bytes = 0;
    };

    struct TextObject final : Object
    {
        TextObject() : Object(ObjectKind::Text) {}
        std::string Text;
    };

    struct ListObject final : Object
    {
        ListObject() : Object(ObjectKind::List) {}
        std::vector<Value> Items;
    };

    struct RecordObject;

    // Fields keyed by interned text, so comparing names compares pointers, kept
    // in the order they were added.
    struct TableObject final : Object
    {
        TableObject() : Object(ObjectKind::Table) {}

        const Value* Find(TextObject* name) const;
        void Set(TextObject* name, Value value);
        bool Remove(TextObject* name);

        std::vector<std::pair<TextObject*, Value>> Fields;
        std::unordered_map<TextObject*, std::size_t> Positions;

        // The record type that made it, for tables made by a type's constructor.
        RecordObject* Record = nullptr;
    };

    // ---- Code ----------------------------------------------------------------

    enum class Operation : std::uint8_t
    {
        LoadNothing,      // A = nothing
        LoadBoolean,      // A = B != 0
        LoadConstant,     // A = constant B
        Move,             // A = B
        GetGlobal,        // A = the global named by constant B
        SetGlobal,        // the global named by constant B = A, in the module or the program
        DefineGlobal,     // the module's own global named by constant B = A
        GetCapture,       // A = capture B
        SetCapture,       // capture B = A
        MakeBox,          // A = a box holding A
        GetBox,           // A = what the box in B holds
        SetBox,           // the box in A holds B
        Closure,          // A = a function made from child prototype B
        Negate,           // A = -B
        Not,              // A = not B
        Add,              // A = B + C
        Subtract,
        Multiply,
        Divide,
        Remainder,
        Power,
        Equal,            // A = B == C
        NotEqual,
        Less,
        LessEqual,
        Jump,             // to B | C << 16
        JumpIfFalse,      // to B | C << 16 when A is false
        JumpIfTrue,
        Call,             // A = call A with B arguments in A + 1 onward
        Invoke,           // A = call member constant C of A with B arguments in A + 1 onward
        Return,           // return A when B, nothing otherwise
        NewList,          // A = a list of C values from B
        NewTable,         // A = an empty table
        GetField,         // A = field constant C of B
        SetField,         // field constant B of A = C
        GetIndex,         // A = B[C]
        SetIndex,         // A[B] = C
        BuildText,        // A = the C values from B as text, joined
        RangeCheck,       // when A > A + 1, to B | C << 16; else A + 2 = A
        RangeStep,        // A += 1, then to B | C << 16
        EachStart,        // checks A can be gone through; A + 1 = 0
        EachNext,         // A + 2 = next item of A, or to B | C << 16 when done
        Spawn,            // starts A with B arguments in A + 1 onward as a task
        Wait,             // waits A seconds when B, until the next update otherwise
        TryStart,         // a failure jumps to B | C << 16 with its message in A
        TryEnd,           // the innermost try is over
        Import,           // A = the module named by constant B
        MakeRecord,       // A = a record type named by constant B, with C field names after it
    };

    struct Instruction
    {
        Operation Code = Operation::LoadNothing;
        std::uint8_t A = 0;
        std::uint16_t B = 0;
        std::uint16_t C = 0;

        std::uint32_t Target() const { return static_cast<std::uint32_t>(B) | (static_cast<std::uint32_t>(C) << 16); }
    };

    struct ModuleObject;

    // Where a function's capture comes from when the function is made: a box in a
    // register of the function around it, or a capture of that function.
    struct CaptureSource
    {
        bool FromRegister = true;
        std::uint16_t Index = 0;
    };

    // A function as compiled: its instructions, constants, and the functions
    // written inside it.
    struct Prototype
    {
        std::string Name;
        int ParameterCount = 0;
        int RegisterCount = 0;
        std::vector<Instruction> Code;
        std::vector<language::Location> Places;
        std::vector<Value> Constants;
        std::vector<std::unique_ptr<Prototype>> Children;
        std::vector<CaptureSource> Captures;
    };

    struct BoxObject final : Object
    {
        BoxObject() : Object(ObjectKind::Box) {}
        Value Held;
    };

    struct FunctionObject final : Object
    {
        FunctionObject() : Object(ObjectKind::Function) {}
        const Prototype* Code = nullptr;
        ModuleObject* Module = nullptr;
        std::vector<BoxObject*> Captures;
    };

    // A function written in C++. Built-in functions work on values directly;
    // the program's own go through ScriptValue.
    using BuiltinFunction = std::function<bool(Machine& machine, std::span<Value> arguments, Value& result)>;

    struct NativeObject final : Object
    {
        NativeObject() : Object(ObjectKind::Native) {}
        std::string Name;
        BuiltinFunction Builtin;
        ScriptFunction Program;

        // A member function bound to the value it was read from, such as a
        // list's Add.
        Value Bound;
    };

    struct HostObject final : Object
    {
        HostObject() : Object(ObjectKind::Host) {}
        std::shared_ptr<ScriptObject> Held;
    };

    // A type declared with `type`: calling it makes a table with its fields.
    struct RecordObject final : Object
    {
        RecordObject() : Object(ObjectKind::Record) {}
        TextObject* Name = nullptr;
        std::vector<TextObject*> Fields;
    };

    // One file of script: its top-level names and the code that defines them.
    struct ModuleObject final : Object
    {
        ModuleObject() : Object(ObjectKind::Module) {}
        std::string Name;
        std::string Path;
        TableObject* Globals = nullptr;

        // Every piece of source run as the module, oldest first. Functions made
        // by earlier pieces still use their code.
        std::vector<std::unique_ptr<Prototype>> Code;
        bool Ran = false;
        bool Running = false;
    };

    // ---- The heap --------------------------------------------------------------

    class Heap
    {
    public:
        Heap() = default;
        ~Heap();

        Heap(const Heap&) = delete;
        Heap& operator=(const Heap&) = delete;

        template <typename Type>
        Type* Make()
        {
            auto* object = new Type();
            object->Bytes = sizeof(Type);
            Track(object);
            return object;
        }

        // Text is interned: the same text is always the same object.
        TextObject* Text(std::string_view text);

        // Records memory an object took on after it was made, such as a list
        // growing.
        void Grew(Object& object, std::size_t bytes);

        std::size_t BytesUsed() const { return Used; }

        // Set when enough was made since the last collection that another one
        // is worth it. Collections only happen between instructions.
        bool WantsCollection() const { return Used >= NextCollection; }

        // Marks a value and everything it reaches; call for every root, then Sweep.
        void Mark(Value value);
        void Mark(Object* object);

        // Marks everything the marked objects reach. Sweep traces first too.
        void Trace();
        void Sweep();

    private:
        void Track(Object* object);

        Object* First = nullptr;
        std::size_t Used = 0;
        std::size_t NextCollection = 1024 * 1024;
        std::unordered_map<std::string_view, TextObject*> Interned;
        std::vector<Object*> Gray;
    };
}
