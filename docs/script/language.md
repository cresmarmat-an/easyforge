# The language

```script
-- A comment runs to the end of the line.

--[[
    A comment that spans
    several lines.
]]

variable count = 0
constant limit = 10

while count < limit then
    count += 1
end

if count == limit then
    print("full")
else if count > limit then
    print("over")
else
    print("room left")
end
```

Every block starts with a header and ends with `end`. Headers with a condition, a
loop, a function's parameters, or a caught error end in `then`; `else`, `try`,
and `type` need nothing after them. Keywords are whole words:

`function` `returns` `then` `end` `variable` `constant` `if` `else` `while` `for`
`in` `to` `return` `break` `continue` `and` `or` `not` `true` `false` `nothing`
`type` `import` `try` `catch` `spawn` `wait` `yield`

`value` is reserved too, for the [shader language](../graphics/shaders.md),
which shares this syntax. None of these can be used as names.

## Names

`variable` declares a name whose value can change and `constant` one whose value
cannot; changing a constant is an error found before the script runs. A
variable without a value starts as `nothing`.

Names declared at the top of a file belong to the file, and functions anywhere
in it can use them. Names declared inside a block or a function belong to that
block. A name nothing has declared yet is an error when the line runs:
`there is no Score`.

Assigning to a name nothing declared is an error found before the script runs,
unless the program defined the name; then the script changes the program's
value, which `ScriptEngine::Get` reads.

`+=`, `-=`, `*=`, and `/=` change a name, a field, or an item in place.

## Types written in the source

```script
variable health: number = 100
constant names: list of string = ["Ari", "Bo"]

function Heal(amount: number) returns number then
    return health + amount
end
```

A type can follow any name and any parameter, and `returns` gives a function's.
They are optional. What is written is checked before the script runs: a value
of the wrong type, a call with the wrong number or kind of arguments, a return
that does not match. Types are `number`, `string`, `boolean`, `list`, `table`,
`function`, `nothing`, `any`, `list of` another type, and
[types you declare](values.md#types-of-your-own). Where a type is not written,
or cannot be known before the script runs, anything goes, and the check at the
line itself (such as adding a string to a number) happens when it runs.

The problems found before running look like this:

```
game.script:3:1: limit is a constant and cannot be changed
game.script:9:6: Heal's first argument should be a number, but this is a string
```

## Loops

```script
for index in 1 to 10 then
    print(index)          -- 1 to 10, both included
end

for item in ["a", "b"] then
    print(item)
end

for character in "héllo" then
    print(character)      -- each character, as a string
end

for name in { X = 1, Y = 2 } then
    print(name)           -- each field's name: X, then Y
end
```

`break` leaves the innermost loop and `continue` starts its next turn. `for ...
to` counts up by one.

## Functions

```script
function Counter() returns function then
    variable count = 0
    return function() then
        count += 1
        return count
    end
end

constant next = Counter()
next()
print(next())   -- 2
```

A function is a value: it can be kept in a variable, put in a list, passed to
another function, or returned. A function written inside another keeps the names
around it, as `count` above. A function is defined when the line that writes it
runs, so a function can call one written below it, but the top of the file
cannot call a function before reaching it. A call must give as many arguments
as the function has parameters. A function without `return` gives `nothing`.

## Errors

```script
try
    Risky()
catch problem then
    print("failed: {problem}")
end

Fail("the save file is damaged")
```

A mistake while running, such as reading an item past the end of a list, or a
call of `Fail`, stops the script with a message that names the file, line, and
column. `try` catches it, and the catch block gets the message as a string,
without the place. Running out of memory or instructions cannot be caught.

## Modules

```script
-- game.script
import "enemies"

constant goblin = enemies.Make("goblin")
```

One file is one module. `import "enemies"` runs `enemies.script` the first time
any script imports it and gives its top-level names as a table named
`enemies`. It is looked for next to the importing file, then in the engine's
search folders. The table is the module's own, so when the module changes one
of its names later, the change shows through it. Two modules that import each
other cannot both start: the second import fails with an error that says so.

## Work that goes on across frames

```script
function Blink(lamp, times) then
    for index in 1 to times then
        lamp.Visible = not lamp.Visible
        wait 0.5
    end
end

spawn Blink(ui.Find("Lamp"), 6)
```

`spawn` starts a function as a task: it runs at once until it waits, and the
program's `ScriptEngine::Update` carries it on, once a frame. `wait 0.5` pauses
it for half a second; `yield`, or `wait` without a number, pauses it until the
next update. Only functions started with `spawn` can wait. A task that fails
stops, and Update returns the error.

## Limitations

- `for ... to` has no step; count down with `while`.
- There is no `switch`, no multiple assignment, and no default values for
  parameters.
- Types are checked before running only where they can be known from the
  source; values from the program are checked when they are used.
