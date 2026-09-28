# Tree files

A table saves to a `.tree` file, a text format that is easy to read and shows
differences well in version control.

```cpp
Result<> saved = game.Save("save.tree");

Table loaded = Table::Load("save.tree");
if (!loaded)
{
    Log(LogLevel::Error, "{}", loaded.Error());   // "save.tree:12: '1 2' is not a value: ..."
}
```

`table.ToText()` gives the same text as a string, and
`Table::FromText(text, name)` reads it, using `name` in error messages.

## The format

```
-- A comment runs to the end of the line.
type Enemy
    Health = 100
    Speed = 4.5

Player
    Health = 100
    Position = 10, 20
    Name = "Ari"
    Sword
        Damage = 12
        Glow = #66CCFF
Goblin : Enemy
    Health = 90
```

- A line with `=` sets a property of the node above it: the name, `=`, and a
  [value](values.md) as `ToText` writes it.
- Any other line is a node. A type follows its name after a colon.
- Each level is four spaces deeper. Properties sit one level deeper than their
  node, and so do its children. A tab counts as four spaces.
- `type Name` starts a type, with its values one level deeper. Types are written
  first, in alphabetical order, each followed by a blank line.
- `--` starts a comment, unless it is inside quotes. Blank lines are skipped.
- A name is written in double quotes when it is empty, contains `=`, `:`, `"`,
  `--`, a tab, or a line break, starts or ends with a space, or could be read as
  a type line. Otherwise names are written as they are.

## Reading

A loaded table starts at version 0, with an empty change list and nothing to
undo: what was read is where the table starts, not a list of changes to it. The
nodes get new [identifiers](nodes-and-properties.md#finding-nodes).

Anything the reader does not understand stops it, and the table tests as false.
The error names the file and the line:

| Problem | Error |
|---|---|
| Indented by a number of spaces that is not a multiple of four | `each level is four spaces deeper, but this line is indented by 2` |
| A value that cannot be read | `'1 2' is not a value: write a number, text in quotes, ...` |
| A property with no node above it | `the property Health is not under a node` |
| A node more than one level below the node above it | `this node is indented deeper than the node above it allows` |
| `= 5` with no name | `a property needs a name before the =` |
| `type` with no name | `a type needs a name: type Enemy` |
| A file that cannot be opened | `save.tree: the file could not be opened` |

## What is saved

The nodes, their names, types, and own properties, in tree order, and the types.
The change list, the undo history, node identifiers, and the table's settings
are not saved.

`Save` writes the whole table each time, replacing the file.
