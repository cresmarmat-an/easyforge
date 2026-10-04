# Values

```script
constant count = 3                          -- number
constant name = "Ari"                       -- string
constant ready = true                       -- boolean
constant missing = nothing                  -- nothing
constant scores = [10, 25, 40]              -- list
constant player = { Name = "Ari", Health = 100 }   -- table
constant tint = #66CCFF                     -- a color, which is a table
constant twice = function(x) then return x * 2 end  -- function

print(Type(scores))   -- list
```

`Type(value)` gives a value's type as a string: `number`, `string`, `boolean`,
`nothing`, `list`, `table`, `function`, the name of a
[type you declared](#types-of-your-own), or the type name of an object the
program gave the script.

## Truth

Only `false` and `nothing` count as false. `0` and `""` count as true.

`and` and `or` give one of their two values rather than `true` or `false`:
`name or "unknown"` is `name` unless `name` is `nothing` or `false`. `not` always
gives a boolean.

`==` and `!=` compare any two values. Numbers, strings, and booleans are equal
when their values are. Lists, tables, records, and functions are equal only to
themselves: two lists holding the same items are different lists. `"1" == 1` is
false. `<`, `>`, `<=`, and `>=` compare two numbers, or two strings by their
bytes (so `"Z" < "a"`).

## Numbers

```script
print(10 / 4)      -- 2.5
print(2 ^ 10)      -- 1024
print(-7 % 3)      -- 2
print(1e3)         -- 1000
print(0.1 + 0.2)   -- 0.30000000000000004
```

Every number is a 64-bit floating point number. Whole numbers print without a
decimal point. `/` divides exactly, `^` raises to a power, and `%` gives the
remainder with the sign of the right side. Dividing by zero gives `infinity`
or `-infinity`, and an impossible result such as `SquareRoot(-1)` prints as
`not a number`.

## Strings

```script
constant score = 12
print("Score: {score}")              -- Score: 12
print("Next: {score + 1}")           -- Next: 13
print("Braces: {{ and }}")           -- Braces: { and }
print("Line one\nLine two")
print("Ari" + " " + "Bo")            -- Ari Bo
```

Strings are written in double quotes on one line. Braces put any expression's
value into the text; `{{` and `}}` write braces. The escapes are `\n` (a new
line), `\t` (a tab), `\"`, and `\\`.

`+` joins two strings. Joining a string and a number is an error that suggests
writing `"{first}{second}"` instead, so a mistake such as `"Total: " + count`
does not quietly do the wrong thing.

Strings are UTF-8 and counted in characters, not bytes. Their characters are
read with `Part`, not with brackets, and `for character in text` visits each
one. A string never changes; the members below give a new one.

| Member | Gives |
|---|---|
| `text.Length` | the number of characters |
| `text.Upper()`, `text.Lower()` | the text with A to Z changed |
| `text.Trim()` | the text without spaces, tabs, and line ends at either end |
| `text.Contains(part)` | `true` when `part` is in it |
| `text.Find(part)` | the position of the first `part`, counted from 1, or `nothing` |
| `text.StartsWith(part)`, `text.EndsWith(part)` | `true` or `false` |
| `text.Replace(from, to)` | the text with every `from` changed to `to` |
| `text.Split(separator)` | a list of the pieces between separators, empty ones included |
| `text.Part(first)`, `text.Part(first, count)` | the characters from position `first`, to the end or `count` of them |

```script
constant line = "  Ari, Bo,Cy "
print(line.Trim().Split(","))        -- ["Ari", " Bo", "Cy"]
print("héllo".Part(2, 3))            -- éll
```

## Lists

```script
variable items = ["sword", "shield"]
items.Add("potion")
print(items[1], items.Count)         -- sword 3
items[2] = "bow"
print(items.Remove(1), items)        -- sword ["bow", "potion"]
```

Lists hold values of any kind and count from 1. Reading or setting a position
outside the list is an error; `Add` and `Insert` make it longer. A list is
shared, not copied: giving it to a function or another name gives the same
list. `Copy` makes a new one.

| Member | Does |
|---|---|
| `list.Count` | the number of items |
| `list.First`, `list.Last` | the first or last item, or `nothing` when empty |
| `list.Add(item)` | adds an item at the end |
| `list.Insert(position, item)` | puts an item at a position from 1 to Count + 1 |
| `list.Remove(position)` | takes out the item at a position and gives it back |
| `list.Contains(item)` | `true` when an equal item is in it |
| `list.Find(item)` | the position of the first equal item, or `nothing` |
| `list.Clear()` | removes every item |
| `list.Reverse()` | turns the list around |
| `list.Copy()` | a new list with the same items |
| `list.Sort()` | puts numbers, or strings, in order; a list with both is an error |
| `list.Join()`, `list.Join(separator)` | the items as one string |

`Reverse`, `Sort`, and `Clear` change the list itself and give `nothing`.

## Tables

```script
variable player = { Name = "Ari", Health = 100 }
player.Speed = 4.5
player["Health"] -= 10

print(player.Name, player.Missing)   -- Ari nothing
print(FieldNames(player))            -- ["Name", "Health", "Speed"]
print(HasField(player, "Speed"))     -- true
RemoveField(player, "Speed")
```

A table holds named fields, kept in the order they were added. A field is read
with a dot or with a string in brackets; a field that is not there reads as
`nothing`, and setting one adds it. Like lists, tables are shared rather than
copied. `for name in table` visits the field names in order.

`FieldNames(table)` gives the names as a list, `HasField(table, name)` tells
whether a field is there (even one set to `nothing`), and
`RemoveField(table, name)` takes one out and gives `true` when it was there.

## Colors

```script
constant sky = #66CCFF
constant glass = #FFFFFF80
print(sky.Red, glass.Alpha)          -- 0.4 0.5019607843137255
```

`#RRGGBB` and `#RRGGBBAA` make a table with `Red`, `Green`, `Blue`, and
`Alpha`, each from 0 to 1. This is the shape the `ui` and `data`
[bridges](interfaces-and-tables.md) take and give for colors.

## Types of your own

```script
type Point
    X: number
    Y: number
end

constant start = Point(1, 2)
start.X = 5
print(start, Type(start))            -- Point { X = 5, Y = 2 } Point

function Length(point: Point) returns number then
    return SquareRoot(point.X ^ 2 + point.Y ^ 2)
end
```

`type` declares the fields a kind of table has. Its name becomes a function that
makes one, taking the fields in order (fields left out start as `nothing`), and
a type that can be written after names and parameters. The checker reports
values of the wrong kind for a field, too many values, and fields the type does
not have. Fields can be changed but not added, also when the checker could not
see the type. Field types are optional, as everywhere.

## Built-in functions

| Function | Gives |
|---|---|
| `print(values...)` | writes the values on one line, separated by spaces |
| `Type(value)` | the value's type as a string |
| `ToText(value)` | the value as it would print |
| `ToNumber(value)` | a number from a string such as `" 42 "`, or `nothing` when it is not one |
| `Fail(message)` | stops with an error that `try` can catch |
| `SquareRoot`, `Absolute`, `Floor`, `Ceiling`, `Round` | the usual; `Round` takes halves away from zero |
| `Sine`, `Cosine`, `Tangent` | of an angle in radians |
| `Radians(degrees)`, `Degrees(radians)` | converts between them |
| `Pi` | the number, not a function |
| `Min(numbers...)`, `Max(numbers...)` | the smallest or largest of one or more numbers |
| `Clamp(value, low, high)` | `value` kept between `low` and `high` |
| `Random()` | a number from 0 up to, but not including, 1 |
| `RandomWhole(low, high)` | a whole number from `low` to `high`, both included |
| `FieldNames`, `HasField`, `RemoveField` | see [Tables](#tables) |

`print` writes to the program's standard output unless the program
[sends it somewhere else](embedding.md#settings).

## Limitations

- There are no tables keyed by numbers or other values, and no sets.
- Strings cannot be written across lines; join them or use `\n`.
- `Sort` has no custom order yet.
- `Upper` and `Lower` leave letters outside A to Z alone.
