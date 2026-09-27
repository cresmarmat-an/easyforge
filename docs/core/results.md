# Results

Nothing in easyforge throws exceptions. A function that can fail returns a
`Result`, which holds either a value or the reason there is none.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Result<int> ParseDigit(char character)
{
    if (character < '0' || character > '9')
    {
        return Failure(std::format("'{}' is not a digit", character));
    }
    return character - '0';
}

Result<int> digit = ParseDigit('x');
if (!digit)
{
    Log(digit.Error());             // 'x' is not a digit
}
```

## Result with a value

| Written | Result |
|---|---|
| `return value;` | A result that succeeded |
| `return Failure("why");` | A result that failed, with a message a person can read |
| `if (result)`, `result.Succeeded()` | True when it has a value |
| `result.Error()` | The message, or an empty string when it succeeded |
| `result.Get()`, `*result` | The value |
| `result->Member` | A member of the value |
| `result.GetOr(fallback)` | The value, or `fallback` when it failed |
| `std::move(result).Get()` | Moves the value out, for types that can only be moved |

Reading the value of a failed result is a mistake in the program, not a
situation to handle. It logs the error and stops the program, so the mistake is
found where it happened. Test the result first, or use `GetOr`.

## Result without a value

`Result<>` is for functions that either succeed or fail, with nothing to return:

```cpp
Result<> SaveSettings(const Settings& settings)
{
    if (!WriteSettingsFile("settings.tree", settings))
    {
        return Failure("could not write settings.tree");
    }
    return {};
}

if (Result<> saved = SaveSettings(settings); !saved)
{
    Log(LogLevel::Warning, saved.Error());
}
```

It has `if (result)`, `Succeeded()`, and `Error()`, and no value.

## Writing good messages

A message is for the person reading the log. Say what was being done and what
went wrong, with the names involved: `"ship.fbx: unsupported FBX version 6100"`
helps more than `"load failed"`.
