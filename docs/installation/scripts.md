# Scripts in the build

`easyforge_add_scripts` checks a program's scripts every time they change and
copies them next to the program, where `ScriptEngine::RunFile` finds them:

```cmake
add_executable(game main.cpp)
target_link_libraries(game PRIVATE easyforge::script)
easyforge_add_scripts(game menu.script scripts/enemies.script)
```

A script with a problem stops the build, with its file, line, and column:

```
menu.script:9:6: ShowVolume's first argument should be a number, but this is a string
```

The check reads each script, checks its names and constants, and checks the
types written in it, without running it. Names the program gives scripts with
`Define` are not known at build time, so the build does not report names that
nothing in the script defines; those are found when the script runs. Relative
paths are relative to the folder of the `CMakeLists.txt` that calls the
function, and every script is copied into the program's own folder, without
the folders it came from.

## The tool

The check is done by `easyforge-script`, a small program built from
easyforge's `tools` folder whenever `script` is built. When easyforge is
fetched, it is only compiled if a program uses `easyforge_add_scripts`. It also
runs scripts and opens a prompt:

```
easyforge-script game.script
easyforge-script check menu.script enemies.script
easyforge-script check --names menu.script
easyforge-script
```

| Command | Does |
|---|---|
| `easyforge-script <file>` | runs a script, then keeps updating its spawned functions, about 60 times a second, until they finish |
| `easyforge-script check <files>` | checks scripts without running them; prints every problem and exits with 1 if there were any |
| `easyforge-script check --names <files>` | also reports names nothing defines, for scripts that use no names from a program |
| `easyforge-script` | opens a prompt |

At the prompt, each line runs when it is complete; a line that opens a block
waits for its `end`. A line that is a value on its own, such as `1 + 2` or
`Twice(21)`, shows the value. What earlier lines defined stays defined. End the
input (Ctrl+Z then Enter on Windows, Ctrl+D elsewhere) to leave.

```
> function Twice(x) then
...     return x * 2
... end
> Twice(21)
42
```

The tool's engine has only the language's own functions, so a script that
needs functions from its program fails at the first one it calls.

## Limitations

- Scripts are copied as source; there is no packing or compiling ahead of time.
- The copy goes into the program's folder, not into a pack.
