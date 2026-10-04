# The C interface

```c
#include <easyforge/script/CInterface.h>

#include <stdio.h>

static void Shout(easyforge_script_arguments* call, void* data)
{
    char buffer[128];
    (void)data;
    if (easyforge_script_argument_kind(call, 0) != EASYFORGE_SCRIPT_TEXT)
    {
        easyforge_script_fail(call, "Shout needs a string");
        return;
    }
    snprintf(buffer, sizeof buffer, "%s!", easyforge_script_argument_text(call, 0));
    easyforge_script_return_text(call, buffer);
}

int main(void)
{
    easyforge_script* script = easyforge_script_new_limited(16 * 1024 * 1024, 1000000);
    easyforge_script_define(script, "Shout", Shout, NULL);

    if (!easyforge_script_run_file(script, "greet.script"))
    {
        puts(easyforge_script_error(script));
    }

    easyforge_script_push_text(script, "Hello");
    if (easyforge_script_call(script, "SomeName"))
    {
        puts(easyforge_script_result_text(script));   /* Output: Hello */
    }

    easyforge_script_free(script);
    return 0;
}
```

`<easyforge/script/CInterface.h>` is a plain C header over the same engine, for
C programs and for any language that can call C functions. Link
`easyforge::script` as usual; the program that links it needs a C++ standard
library, which a C++ compiler or linker brings.

Functions that can fail return 1 when they worked and 0 when they did not, and
`easyforge_script_error` then says why. Strings go in and out as UTF-8.

## Making an engine

| Function | Does |
|---|---|
| `easyforge_script_new()` | an engine without limits |
| `easyforge_script_new_limited(memory, instructions)` | an engine with a memory limit in bytes and an instruction limit for each run or call; 0 is no limit |
| `easyforge_script_free(script)` | frees it |

## Running and calling

| Function | Does |
|---|---|
| `easyforge_script_run(script, source, name)` | runs source text; `name` is used in messages |
| `easyforge_script_run_file(script, path)` | reads and runs a file |
| `easyforge_script_update(script, delta_seconds)` | moves spawned functions along; call it once a frame |
| `easyforge_script_error(script)` | why the last call that failed did, as `name:line:column: message` |
| `easyforge_script_push_nothing`, `_boolean`, `_number`, `_text` | adds an argument for the next call |
| `easyforge_script_call(script, function)` | calls a function by name with the pushed arguments |

Arguments are pushed in order, then `easyforge_script_call` uses and clears
them.

## Results

After a run or call:

| Function | Gives |
|---|---|
| `easyforge_script_result_kind(script)` | `EASYFORGE_SCRIPT_NOTHING`, `_BOOLEAN`, `_NUMBER`, `_TEXT`, or `_OTHER` |
| `easyforge_script_result_boolean(script)` | 1 or 0 |
| `easyforge_script_result_number(script)` | the number, or 0 |
| `easyforge_script_result_text(script)` | any result as text, as `print` shows it |

Lists, tables, functions, and objects are `OTHER`, and are read as text.

## Functions of your own

```c
static void Clamp(easyforge_script_arguments* call, void* data)
{
    double value, low, high;
    (void)data;
    if (easyforge_script_argument_count(call) != 3)
    {
        easyforge_script_fail(call, "Clamp takes 3 numbers");
        return;
    }
    value = easyforge_script_argument_number(call, 0);
    low = easyforge_script_argument_number(call, 1);
    high = easyforge_script_argument_number(call, 2);
    easyforge_script_return_number(call, value < low ? low : value > high ? high : value);
}

easyforge_script_define(script, "Clamp", Clamp, NULL);
easyforge_script_define_number(script, "Limit", 3);
easyforge_script_define_text(script, "PlayerName", "Ari");
```

`easyforge_script_define(script, name, function, data)` gives scripts a C
function. `data` is passed back to it on every call, for whatever the function
needs. Inside it:

| Function | Does |
|---|---|
| `easyforge_script_argument_count(call)` | how many arguments the script passed |
| `easyforge_script_argument_kind(call, index)` | the kind of one, counting from 0 |
| `easyforge_script_argument_boolean`, `_number`, `_text` | one argument's value |
| `easyforge_script_return_boolean`, `_number`, `_text` | what the function gives back |
| `easyforge_script_fail(call, message)` | stops the script with an error that `try` can catch |

A function that neither returns nor fails gives back `nothing`. The C interface
does not check arguments for you; check their count and kinds before using
them. `easyforge_script_define_number` and `easyforge_script_define_text`
give scripts plain values.

## How long strings last

- Text from `easyforge_script_result_text` and `easyforge_script_error` stays
  valid until the next call on the same engine.
- Text from `easyforge_script_argument_text` stays valid until your function
  returns.
- Strings you pass in are copied, so they can be freed or changed right after.

## Limitations

- The C interface passes nothing, booleans, numbers, and strings. Lists,
  tables, and objects of your own need the [C++ interface](embedding.md).
- An engine is used from one thread.
