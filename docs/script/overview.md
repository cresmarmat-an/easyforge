# script

`script` is the easyforge scripting language: a small language any C or C++
program can embed, with or without the rest of easyforge. Script files end in
`.script`.

```script
-- greet.script
function SomeName(parameter) returns string then
    print("Output: {parameter}")
    return "Output: {parameter}"
end

SomeName("Hello")
```

```cpp
#include <easyforge/script.h>

using namespace easyforge;

int main()
{
    ScriptEngine scripts = ScriptEngine::New({ .MemoryLimit = 64 * 1024 * 1024 });
    scripts.Define("Shout", [](std::string text) { return text + "!"; });

    Result<ScriptValue> ran = scripts.RunFile("greet.script");   // prints "Output: Hello"
    if (!ran)
    {
        Log(LogLevel::Error, ran.Error());   // such as "greet.script:3:5: there is no Name"
        return 1;
    }

    Result<ScriptValue> result = scripts.Call("SomeName", "again");
    if (result)
    {
        Log(result->AsText());   // "Output: again"
    }
}
```

Link `easyforge::script`. It needs only `core`.

## Pages

- [The language](language.md): names, blocks, types, functions, errors,
  modules, and functions that run across frames.
- [Values](values.md): numbers, strings, lists, tables, colors, types of your
  own, and the built-in functions.
- [Embedding](embedding.md): running scripts from C++, giving them functions and
  objects, limits, and the values that pass between them.
- [The C interface](c-interface.md): the same from C, and from languages that
  can call C.
- [Interfaces and tables](interfaces-and-tables.md): letting scripts drive a
  `ui` interface or change a `data` table.
- [Scripts in the build](../installation/scripts.md): checking scripts when a
  program is built, and the `easyforge-script` tool.

[Example 06](https://github.com/cresmarmat-an/easyforge-examples/tree/main/06-scripted-interface)
runs a whole menu from a script.

## How it works

A script is read with `core`'s [language reader](../core/language.md), checked
(names, constants, and the types written in it), compiled to instructions for a
register machine, and run. Values live in the engine's own memory, which a
collector clears of what nothing uses any more. Each engine is separate: two
engines share no names and no values.

Scripts reach only what the program gives them with `Define`. There are no
built-in functions for files, the network, or the system. The one way a script
reaches outside itself is importing another script, which the program can turn
off. With a memory limit and an instruction limit, a script from someone you do
not trust cannot take the program over.

## Limitations

- An engine is used from one thread at a time.
- Numbers are 64-bit floating point; there is no separate whole number type.
- Strings are UTF-8. Upper and Lower change only the letters A to Z.
- Lists count from 1. Tables are keyed by names; there are no tables keyed by
  numbers or other values.
- There is no debugger yet; errors give the file, line, and column.
