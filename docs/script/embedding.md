# Embedding

```cpp
#include <easyforge/script.h>

using namespace easyforge;

int main()
{
    ScriptEngine scripts = ScriptEngine::New({
        .MemoryLimit = 16 * 1024 * 1024,
        .InstructionLimit = 1'000'000,
    });

    int coins = 0;
    scripts.Define("AddCoins", [&coins](int amount) { coins += amount; });
    scripts.Define("PlayerName", "Ari");

    Result<ScriptValue> ran = scripts.Run(R"(
        function Reward(level) returns number then
            AddCoins(level * 10)
            return level * 10
        end
        print("hello {PlayerName}")
    )", "rewards");
    if (!ran)
    {
        Log(LogLevel::Error, ran.Error());
        return 1;
    }

    Result<ScriptValue> given = scripts.Call("Reward", 3);   // coins is now 30
    Log("{} coins", given->As<int>());
}
```

## Making an engine

`ScriptEngine::New(settings)` makes an engine. Each engine has its own names and
values, and two engines share nothing. An engine is a handle like every
easyforge object: copies refer to the same engine, which lives while any copy
does. `ScriptEngine()` makes an empty one that tests as false.

### Settings

| Setting | Default | Does |
|---|---|---|
| `MemoryLimit` | `0`, no limit | the most memory the scripts' values may use, in bytes |
| `InstructionLimit` | `0`, no limit | the most instructions one `Run`, `Call`, or `Update` may carry out |
| `SearchFolders` | empty | where `import` looks after the importing file's folder |
| `AllowImports` | `true` | whether scripts may import other files at all |
| `Print` | standard output | a function that receives each line `print` writes |

```cpp
ScriptEngine scripts = ScriptEngine::New({
    .SearchFolders = { "scripts", "mods" },
    .Print = [](std::string_view line) { Log("[script] {}", line); },
});
```

## Running scripts

```cpp
Result<ScriptValue> ran = scripts.RunFile("scripts/menu.script");
Result<ScriptValue> value = scripts.Run("return 6 * 7", "sum");   // 42
```

`Run(source, name)` runs text, and `RunFile(path)` reads a file and runs it. The
result is what a `return` at the top level of the script gives, or `nothing`.
When the script has a problem, the result tests as false and `Error()` says
where, as `name:line:column: message`.

Each name given to `Run`, and each file, is a module of its own: its top-level
names belong to it. Other scripts reach them with `import`, and the program
with `Get` and `Call`, which look through every module, newest first, and then
the names the program defined. Running the same name or file again runs in the
same module, which keeps its names.

`Check(source, name)` reads and checks a script without running it and returns
every problem it finds, in order. A script checked away from its program uses
names only the program defines, so `Check(source, name, false)` leaves out
"there is no ..." for names nothing defines.

## Giving scripts functions

```cpp
scripts.Define("Shout", [](std::string text) { return text + "!"; });
scripts.Define("Distance", [](double x, double y) { return std::sqrt(x * x + y * y); });
scripts.Define("Sum", [](const std::vector<ScriptValue>& values) {
    double total = 0;
    for (const ScriptValue& value : values)
    {
        total += value.AsNumber();
    }
    return total;
});
scripts.Define("Save", [](std::string slot) -> Result<ScriptValue> {
    if (slot.empty())
    {
        return Failure("Save needs a slot name");
    }
    return ScriptValue(true);
});
```

`Define(name, function)` gives scripts a C++ function. Its parameters can be:

| Parameter | Takes |
|---|---|
| `bool` | `true` or `false` |
| `int`, `float`, `double`, any number type | a number |
| `std::string` | a string |
| `std::vector<ScriptValue>` | a list, as a copy of its items |
| `ScriptValue` | anything |

Each argument is checked before the function runs, and a script that passes the
wrong kind or count gets an error naming the function:
`Shout's first argument should be a string, but it is a number`. A function
whose only parameter is `const std::vector<ScriptValue>&` gets the arguments as
they are, however many, and checks them itself.

It can return nothing, `bool`, any number type, `std::string`, `ScriptValue`, a
`std::shared_ptr<ScriptObject>`, or `Result<ScriptValue>`. Returning a
`Failure` stops the script with that message, which `try` in the script can
catch. A list or table to return is made with `ScriptValue::List` or
`ScriptValue::Table`.

`ScriptValue::Function(name, function)` makes the same kind of function as a
value, to put in a table or return from an object.

## Giving scripts values

```cpp
scripts.Define("Score", 0);
scripts.Define("Levels", ScriptValue::List({ "forest", "caves", "tower" }));
scripts.Define("Window", ScriptValue::Table({ { "Width", 1280 }, { "Height", 720 } }));

scripts.Run("Score = Score + 5");
int score = scripts.Get("Score").As<int>();   // 5
```

`Define(name, value)` gives scripts a value. A script that assigns to a name the
program defined changes the program's value, which `Get` reads back.

`ScriptValue::List` and `ScriptValue::Table` make lists and tables without an
engine; the engine copies them in when a script receives them.
`scripts.NewList(items)` and `scripts.NewTable(fields)` make them inside the
engine, so a script that receives one and changes it changes the same list
the program holds.

## Objects of your own

```cpp
class Player : public ScriptObject
{
public:
    std::string TypeName() const override { return "Player"; }

    ScriptValue Get(std::string_view name) override
    {
        if (name == "Health")
        {
            return Health;
        }
        if (name == "Heal")
        {
            return ScriptValue::Function("Heal", [this](int amount) { Health += amount; });
        }
        return {};
    }

    bool Set(std::string_view name, const ScriptValue& value) override
    {
        if (name != "Health")
        {
            return false;
        }
        Health = value.As<int>();
        return true;
    }

    int Health = 100;
};

auto player = std::make_shared<Player>();
scripts.Define("player", player);
```

```script
player.Health -= 30
player.Heal(5)
print(player.Health, Type(player))   -- 75 Player
```

A `ScriptObject` lets scripts read and change a C++ object through its members.
`Get` gives a member's value, or `nothing` when there is no such member; a
member that is a function is returned as a function value. `Set` changes a
member and returns false when it cannot, which the script sees as
`Mana cannot be changed on a Player`. `TypeName` is the name scripts see, and
`MemberNames` may list the members. Scripts hold the object through the
`shared_ptr`, so it lives as long as either side uses it.

## Values that come back

```cpp
Result<ScriptValue> made = scripts.Run(R"(
    function Greet(name) then
        return "hello {name}"
    end
    return { Items = [1, 2], Greeter = Greet }
)", "made");

ScriptValue table = *made;
std::size_t count = table.Field("Items").Count();                    // 2
Result<ScriptValue> greeting = table.Field("Greeter").Call({ "Ari" });  // "hello Ari"
```

A `ScriptValue` is nothing, a boolean, a number, a string, or a list, table,
function, or object that lives in an engine. Holding one keeps what it refers
to alive, even after the script lets go of it, until the engine itself is
gone.

| Member | Gives |
|---|---|
| `Kind()` | `ScriptValueKind::Nothing`, `Boolean`, `Number`, `Text`, `List`, `Table`, `Function`, or `Object` |
| `IsNothing()`, `IsTrue()` | whether it is nothing; whether an `if` would take it as true |
| `AsNumber()`, `AsBoolean()` | the number or boolean, or 0 and false for other kinds |
| `AsText()` | a string as it is, anything else as `print` shows it |
| `As<T>()` | the value as `bool`, a number type, `std::string`, or `ScriptValue` |
| `Items()`, `Count()` | a list's items; how many items or fields there are |
| `Field(name)`, `FieldNames()`, `SetField(name, value)` | a table's fields, or an object's members |
| `Add(item)` | adds an item to a list |
| `Object()` | the `ScriptObject` for an object |
| `Call(arguments)` | calls a function value, such as a callback a script gave the program |
| `TypeName()` | the type's name as scripts see it |

`==` compares the way the language does: by value for nothing, booleans,
numbers, and strings, and by identity for the rest.

## Functions that run across frames

```cpp
window.OnFrame = [&](float deltaSeconds) {
    Result<> updated = scripts.Update(deltaSeconds);
    if (!updated)
    {
        Log(LogLevel::Error, updated.Error());
    }
};
```

Scripts start tasks with `spawn` (see [the language](language.md#work-that-goes-on-across-frames)).
`Update(deltaSeconds)` moves them along: tasks waiting for time count down, and
tasks that yielded continue. Call it once a frame. It returns the first error a
task met; that task stops, and the others carry on. `RunningTasks()` counts the
tasks that have not finished.

## Limits

| Limit | When it is reached |
|---|---|
| `MemoryLimit` | `the scripts need more than 16777216 bytes of memory` |
| `InstructionLimit` | `the script ran more than 1000000 instructions` |
| 1000 calls inside each other | `the calls go more than 1000 deep; a function may be calling itself without end` |
| 32 calls between the program and scripts inside each other | `the program and its scripts call each other more than 32 deep` |

Running out of memory or instructions cannot be caught by `try`; the script
stops, the call returns the error, and the engine works again for the next
call. The instruction limit counts from the outermost call into the engine, so
a script that calls the program, which calls the script again, shares one
count.

Memory is collected by itself as scripts run. `MemoryUsed()` gives what the
values take now, and `Collect()` collects at once, between runs.

## Limitations

- An engine is used from one thread. Functions and objects you define are called
  on that thread.
- C++ functions take at most the parameter kinds listed above; anything else is
  passed as `ScriptValue` or through a `ScriptObject`.
- A `ScriptValue` from an engine that is gone is empty: lists have no items,
  and calling a function gives an error.
