# Types

A type is a set of default values shared by every node that names it. A node
reads its type's value for any property it has no value of its own for.

```cpp
game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });

Node goblin = game.Add("Goblin", "Enemy");
Node troll = game.Add("Troll", "Enemy", { { "Health", 300 } });

int goblinHealth = goblin["Health"];   // 100, from the type
int trollHealth = troll["Health"];     // 300, its own
float speed = troll["Speed"];          // 4.5, from the type

goblin["Health"] = 50;                 // the goblin's own value now
goblin["Health"].Clear();              // reads 100 again
```

The values are not copied into the nodes. A node of a type stores only the
values it was given itself, so a thousand enemies that use the defaults cost
nothing for those properties, and `node.Properties()` lists only the node's own.

## Defining and redefining

`table.DefineType(name, values)` defines a type, or replaces one that already
exists. Replacing a type changes what every node of that type reads, right away:

```cpp
game.DefineType("Enemy", { { "Health", 120 } });   // goblins now read 120, and have no Speed
```

A node can name a type that is not defined yet. It reads nothing from it until
the type is defined.

| Written | Gives |
|---|---|
| `game.TypeNames()` | The names of every defined type, in alphabetical order |
| `game.TypeValues("Enemy")` | The type's names and values, in the order given |
| `node.TypeName()` | The type named when the node was added, or empty |
| `game.NodesOfType("Enemy")` | Every node of the type |
| `game.NodesWith("Speed")` | Every node that has the property, including through its type |

Defining a type is a change like any other: it is in the
[change list](changes-and-undo.md) as `ChangeKind::TypeDefined`, and inside an
edit, `Undo` puts back the values the type had before, or removes a type that
did not exist.

## Limitations

- A node's type is set when the node is added and cannot be changed.
- A type cannot extend another type.
- Types are for default values. They do not limit which properties a node can
  have or what kind of value a property holds.
