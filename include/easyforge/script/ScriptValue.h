#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <easyforge/core/Result.h>
#include <easyforge/script/ScriptFunctions.h>

namespace easyforge
{
    class ScriptEngine;
    class ScriptObject;

    namespace internal::scripting
    {
        class EngineState;
        struct Root;
    }

    enum class ScriptValueKind
    {
        Nothing,
        Boolean,
        Number,
        Text,
        List,
        Table,
        Function,
        Object,
    };

    // A value from a script, or one to give it: nothing, a boolean, a number,
    // text, or a list, table, function, or object that lives in an engine.
    //
    //     ScriptValue score = scripts.Get("Score");
    //     int points = score.As<int>();
    //
    // Lists, tables, functions, and objects stay alive while a ScriptValue
    // holds them, even if the script lets go of them. They belong to the engine
    // they came from and are empty once it is gone.
    class ScriptValue
    {
    public:
        // Nothing.
        ScriptValue() = default;

        ScriptValue(bool value);
        ScriptValue(double value);
        ScriptValue(float value);
        ScriptValue(int value);
        ScriptValue(long long value);
        ScriptValue(std::string value);
        ScriptValue(std::string_view value);
        ScriptValue(const char* value);

        // An object of your own, which scripts read and change through its
        // members.
        ScriptValue(std::shared_ptr<ScriptObject> object);

        // A C++ function scripts can call. Its arguments can be bool, any
        // number type, std::string, std::vector<ScriptValue> for a list, or
        // ScriptValue; each is checked before the call, and a script that passes
        // the wrong kind gets an error naming the function. It can return
        // nothing, any of those, or a Result<ScriptValue> to fail on purpose. A
        // function taking only `const std::vector<ScriptValue>&` gets the
        // arguments as they are, however many.
        //
        //     ScriptValue shout = ScriptValue::Function("Shout", [](std::string text) { return text + "!"; });
        template <typename Callable>
        static ScriptValue Function(std::string name, Callable&& callable)
        {
            return FromFunction(name, internal::MakeScriptFunction(name, std::forward<Callable>(callable)));
        }
        static ScriptValue FromFunction(std::string name, ScriptFunction function);

        // A list or table made without an engine, such as one an object of your
        // own returns from Get. The engine copies it in when a script receives it.
        static ScriptValue List(std::vector<ScriptValue> items = {});
        static ScriptValue Table(std::vector<std::pair<std::string, ScriptValue>> fields = {});

        ScriptValueKind Kind() const { return ValueKind; }
        bool IsNothing() const { return ValueKind == ScriptValueKind::Nothing; }

        // False for nothing and for false, true for everything else, as an if
        // in a script decides.
        bool IsTrue() const;

        double AsNumber() const;
        bool AsBoolean() const;

        // Text as it is. Other values as print shows them: 3 for a whole
        // number, true, nothing, [1, 2], { X = 1 }.
        std::string AsText() const;

        // The value as a C++ type: bool, any number type, std::string, or
        // ScriptValue. A value of another kind gives the type's empty value.
        template <typename Wanted>
        Wanted As() const;

        // A list's items, in order.
        std::vector<ScriptValue> Items() const;

        // How many items a list has, or fields a table has.
        std::size_t Count() const;

        // A table's field, or an object's member; nothing when it has none.
        ScriptValue Field(std::string_view name) const;

        // A table's field names, in the order they were added.
        std::vector<std::string> FieldNames() const;

        // Sets a table's field or an object's member. Returns false when the
        // value is neither, or the object refuses.
        bool SetField(std::string_view name, const ScriptValue& value) const;

        // Adds an item at the end of a list.
        bool Add(const ScriptValue& item) const;

        // The object given to the engine, for values of kind Object.
        std::shared_ptr<ScriptObject> Object() const;

        // Calls a function value: one from a script, such as a callback it
        // assigned, or one made with Function.
        Result<ScriptValue> Call(const std::vector<ScriptValue>& arguments = {}) const;

        // The name of the value's type as scripts see it: "number", "string",
        // "Point", and so on.
        std::string TypeName() const;

        // Equal as == in a script decides: by value for nothing, booleans,
        // numbers, and text, and by identity for the rest.
        bool operator==(const ScriptValue& other) const;

    private:
        ScriptValueKind ValueKind = ScriptValueKind::Nothing;
        double Number = 0.0;
        std::string Text;
        // Keeps a list, table, function, or object alive in its engine.
        std::shared_ptr<internal::scripting::Root> Rooted;
        std::shared_ptr<ScriptObject> Held;

        // A C++ function, list, or table not yet given to an engine.
        std::shared_ptr<ScriptFunction> Native;
        std::shared_ptr<std::vector<ScriptValue>> PendingItems;
        std::shared_ptr<std::vector<std::pair<std::string, ScriptValue>>> PendingFields;

        friend class internal::scripting::EngineState;
    };

    template <typename Wanted>
    Wanted ScriptValue::As() const
    {
        if constexpr (std::is_same_v<Wanted, ScriptValue>)
        {
            return *this;
        }
        else if constexpr (std::is_same_v<Wanted, bool>)
        {
            return AsBoolean();
        }
        else if constexpr (std::is_arithmetic_v<Wanted>)
        {
            return static_cast<Wanted>(AsNumber());
        }
        else if constexpr (std::is_same_v<Wanted, std::string>)
        {
            return ValueKind == ScriptValueKind::Text ? Text : std::string();
        }
        else
        {
            static_assert(sizeof(Wanted) == 0, "ScriptValue::As takes bool, a number type, std::string, or ScriptValue");
        }
    }

    namespace internal
    {
        template <typename Argument>
        Argument Convert(const ScriptValue& value)
        {
            if constexpr (std::is_same_v<Argument, std::vector<ScriptValue>>)
            {
                return value.Items();
            }
            else
            {
                return value.As<Argument>();
            }
        }

        template <typename Returned>
        Result<ScriptValue> Wrap(Returned&& returned)
        {
            using Plain = std::remove_cvref_t<Returned>;
            if constexpr (std::is_same_v<Plain, Result<ScriptValue>>)
            {
                return std::forward<Returned>(returned);
            }
            else if constexpr (std::is_arithmetic_v<Plain> && !std::is_same_v<Plain, bool>)
            {
                return ScriptValue(static_cast<double>(returned));
            }
            else
            {
                return ScriptValue(std::forward<Returned>(returned));
            }
        }

        template <typename Callable>
        ScriptFunction MakeScriptFunction(std::string name, Callable&& callable)
        {
            using Signature = decltype(std::function(std::declval<std::decay_t<Callable>>()));
            using Parts = FunctionParts<Signature>;
            using ArgumentTypes = typename Parts::ArgumentTypes;

            if constexpr (TakesArgumentList<ArgumentTypes>())
            {
                // Takes the arguments as they are, however many there are, and checks
                // them itself.
                static_cast<void>(name);
                return [callable = std::forward<Callable>(callable)](const std::vector<ScriptValue>& arguments) {
                    return Wrap(callable(arguments));
                };
            }
            else
            {
                return [name = std::move(name), callable = std::forward<Callable>(callable)](
                           const std::vector<ScriptValue>& arguments) -> Result<ScriptValue> {
                    if (arguments.size() != Parts::Count)
                    {
                        return Failure(std::format("{} takes {} argument{}, but was given {}", name, Parts::Count,
                            Parts::Count == 1 ? "" : "s", arguments.size()));
                    }
                    std::string problem;
                    auto check = [&]<std::size_t... Index>(std::index_sequence<Index...>) {
                        auto one = [&]<std::size_t At>() {
                            using Argument = std::tuple_element_t<At, ArgumentTypes>;
                            if (problem.empty() && !Accepts<Argument>(arguments[At]))
                            {
                                problem = std::format("{}'s {} argument should be {}, but it is {}", name, OrdinalWord(At),
                                    ExpectedWord<Argument>(), ScriptKindWord(arguments[At]));
                            }
                        };
                        (one.template operator()<Index>(), ...);
                    };
                    check(std::make_index_sequence<Parts::Count>());
                    if (!problem.empty())
                    {
                        return Failure(problem);
                    }
                    auto call = [&]<std::size_t... Index>(std::index_sequence<Index...>) {
                        return callable(Convert<std::tuple_element_t<Index, ArgumentTypes>>(arguments[Index])...);
                    };
                    if constexpr (std::is_void_v<typename Parts::ReturnType>)
                    {
                        call(std::make_index_sequence<Parts::Count>());
                        return NothingResult();
                    }
                    else
                    {
                        return Wrap(call(std::make_index_sequence<Parts::Count>()));
                    }
                };
            }
        }
    }
}
