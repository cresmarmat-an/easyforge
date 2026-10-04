# Nodes and properties

```cpp
Table game = Table::New();

Node player = game.Add("Player", { { "Name", "Ari" }, { "Health", 100 } });
Node sword = player.Add("Sword");
sword["Damage"] = 12;

player["Health"] -= 10;
std::string name = player["Name"];
if (player["Health"].As<int>() <= 0)
{
    player.Remove();
}
```

## Making a table

`Table::New()` makes an empty table. Settings go in braces, and both have
defaults:

| Setting | Default | Meaning |
|---|---|---|
| `ChangeLimit` | `100000` | How many changes the [change list](changes-and-undo.md) remembers |
| `UndoLimit` | `100` | How many edits `Undo` can go back |

```cpp
Table history = Table::New({ .ChangeLimit = 1000, .UndoLimit = 20 });
```

A default-constructed `Table` is no table: it tests as false and adding to it
does nothing. [Loading](tree-files.md) a file that cannot be read also gives a
table that tests as false, with `Error()` saying why.

## Adding nodes

`table.Add(name)` adds a node at the top of the table, and `node.Add(name)` adds
a child at the end of that node's children. Both take an optional
[type](types.md) and starting values:

```cpp
Node camera = game.Add("Camera");
Node goblin = game.Add("Goblin", "Enemy");
Node chest = game.Add("Chest", { { "Locked", true } });
Node troll = game.Add("Troll", "Enemy", { { "Health", 300 } });
```

A name is any text. Names do not have to be unique; finding by name gives the
first match. A name with `/` in it cannot be found with `Find`, which uses `/`
to separate levels, but `Child` still finds it.

## Reading and changing properties

`node["Health"]` is a cell: it reads and assigns like a variable.

| Written | What happens |
|---|---|
| `node["Health"] = 100` | Sets the node's own value |
| `int health = node["Health"]` | Reads it as an `int` |
| `node["Health"].As<int>()` | Reads it as an `int`, for use inside an expression |
| `node["Health"].Get()` | Reads it as a [`DataValue`](values.md) |
| `node["Health"] += 5`, `-=` | Adds to a whole number, number, or vector, keeping its type |
| `node["Best"] = node["Score"]` | Copies one value into another property |
| `node["Health"].Clear()` | Removes the node's own value |
| `node["Health"].Exists()`, `node.Has("Health")` | True when the node or its type has the property |
| `node.Get("Health")`, `node.Set("Health", 100)` | Reads or sets a `DataValue` directly, without making a cell |

Write the type you read into. `auto health = node["Health"];` keeps the cell
itself, which reads the current value each time it is used.

Reading a property gives the node's own value, or else its type's value, or else
nothing. Nothing reads as the empty value of whatever type you ask for: `0`,
`false`, `""`, or a zero vector. Assigning nothing (`node["Health"] =
DataValue()`) is the same as `Clear()`.

Adding to a property that has no value starts from zero of the amount's type, so
`node["Score"] += 10` works on a new node. Adding to text, booleans, or colors
leaves them as they are.

`node.Properties()` lists the node's own properties, in the order they were
first set. Properties that come from its type are not included.

## Finding nodes

| Written | Gives |
|---|---|
| `game.TopNodes()` | The nodes at the top of the table, in order |
| `node.Children()`, `node.ChildCount()` | The node's children, in order |
| `node.Child("Sword")` | The first child with that name, or no node |
| `node.Parent()` | The node above, or no node at the top |
| `game.Find("Player/Sword")` | A node by its path of names, or no node |
| `node.Path()` | The node's path, such as `"Player/Sword"` |
| `game.Nodes()` | Every node, parents before children, in order |
| `game.NodeCount()` | How many nodes there are |
| `game.NodesWith("Health")` | Every node that has the property, its own or its type's |
| `game.NodesOfType("Enemy")` | Every node of a type |
| `node.Identifier()`, `game.FindByIdentifier(number)` | A number no other node of the table has had, and the node with it |

`NodesWith` and `NodesOfType` go through the table's rows rather than the tree,
so their order is not the tree's order. Use `Nodes()` when order matters.

"No node" is a `Node` that tests as false. Every function on it does nothing and
gives an empty result, so a chain like `game.Find("Player").Child("Sword")` is
safe when the player does not exist.

## Moving, renaming, and removing

```cpp
sword.MoveTo(chest);     // to the end of chest's children, with everything under it
sword.MoveTo(Node());    // to the top of the table
sword.Rename("Blade");
sword.Remove();          // with everything under it
```

A node cannot be moved inside itself or its own children; `MoveTo` logs a
warning and leaves it where it was.

After `Remove`, handles to the node and to everything under it test as false,
and reading or changing through them does nothing. If the removal was part of an
[edit](changes-and-undo.md#edits-and-undo), `Undo` brings the nodes back with
their properties, and the same handles work again.

`node == other` is true when both handles refer to the same node.
