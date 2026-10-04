#pragma once

// How a C++ function becomes one scripts can call: each argument is checked and
// converted from a ScriptValue, and the result converted back. Used by
// ScriptValue::Function and ScriptEngine::Define.

#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <easyforge/core/Result.h>

namespace easyforge
{
    class ScriptValue;

    // What every function scripts can call comes down to: the arguments in, a
    // value or a failure out.
    using ScriptFunction = std::function<Result<ScriptValue>(const std::vector<ScriptValue>& arguments)>;

    namespace internal
    {
        std::string_view OrdinalWord(std::size_t index);
        std::string ScriptKindWord(const ScriptValue& value);
        bool IsScriptNumber(const ScriptValue& value);
        bool IsScriptBoolean(const ScriptValue& value);
        bool IsScriptText(const ScriptValue& value);
        bool IsScriptList(const ScriptValue& value);
        Result<ScriptValue> NothingResult();

        template <typename Signature>
        struct FunctionParts;

        template <typename Returned, typename... Arguments>
        struct FunctionParts<std::function<Returned(Arguments...)>>
        {
            using ReturnType = Returned;
            using ArgumentTypes = std::tuple<std::remove_cvref_t<Arguments>...>;
            static constexpr std::size_t Count = sizeof...(Arguments);
        };

        // Whether a function takes the arguments as one list, however many.
        template <typename ArgumentTypes>
        constexpr bool TakesArgumentList()
        {
            if constexpr (std::tuple_size_v<ArgumentTypes> == 1)
            {
                return std::is_same_v<std::tuple_element_t<0, ArgumentTypes>, std::vector<ScriptValue>>;
            }
            else
            {
                return false;
            }
        }

        template <typename Argument>
        constexpr std::string_view ExpectedWord()
        {
            if constexpr (std::is_same_v<Argument, bool>)
            {
                return "true or false";
            }
            else if constexpr (std::is_arithmetic_v<Argument>)
            {
                return "a number";
            }
            else if constexpr (std::is_same_v<Argument, std::string>)
            {
                return "a string";
            }
            else if constexpr (std::is_same_v<Argument, std::vector<ScriptValue>>)
            {
                return "a list";
            }
            else
            {
                return "a value";
            }
        }

        template <typename Argument>
        bool Accepts(const ScriptValue& value)
        {
            if constexpr (std::is_same_v<Argument, bool>)
            {
                return IsScriptBoolean(value);
            }
            else if constexpr (std::is_arithmetic_v<Argument>)
            {
                return IsScriptNumber(value);
            }
            else if constexpr (std::is_same_v<Argument, std::string>)
            {
                return IsScriptText(value);
            }
            else if constexpr (std::is_same_v<Argument, std::vector<ScriptValue>>)
            {
                return IsScriptList(value);
            }
            else
            {
                static_assert(std::is_same_v<Argument, ScriptValue>,
                    "a function for scripts takes bool, number types, std::string, std::vector<ScriptValue>, or ScriptValue");
                return true;
            }
        }

        template <typename Argument>
        Argument Convert(const ScriptValue& value);

        template <typename Returned>
        Result<ScriptValue> Wrap(Returned&& returned);

        // A C++ callable as a ScriptFunction, checking the arguments it is given.
        // Defined in ScriptValue.h, after ScriptValue.
        template <typename Callable>
        ScriptFunction MakeScriptFunction(std::string name, Callable&& callable);
    }
}
