# data

`data` holds information as a tree of named nodes, each with any properties you
give it, stored like one big table: every node is a row and every property is a
column. Every change is recorded, so the same table can be undone, saved to a
readable file, and watched by anything that needs to follow it, such as an
interface.

```cmake
target_link_libraries(my_game PRIVATE easyforge::data)
```

```cpp
#include <easyforge/data.h>

using namespace easyforge;

int main()
{
    Table game = Table::New();

    Node player = game.Add("Player");
    player["Health"] = 100;
    player["Position"] = Vector2 { 10, 20 };

    Node sword = player.Add("Sword");
    sword["Damage"] = 12;

    game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });
    Node goblin = game.Add("Goblin", "Enemy");

    game.BeginEdit("Hit");
    goblin["Health"] -= sword["Damage"].As<int>();
    game.EndEdit();

    int health = goblin["Health"];   // 88
    game.Undo();                     // 100 again

    Result<> saved = game.Save("save.tree");
    if (!saved)
    {
        Log(LogLevel::Error, "{}", saved.Error());
    }
}
```

The saved file:

```
type Enemy
    Health = 100
    Speed = 4.5

Player
    Health = 100
    Position = 10, 20
    Sword
        Damage = 12
Goblin : Enemy
```

`data` depends only on `core`.

## The same data as a table

The tree above is stored like this:

| Node | Parent | Health | Position | Damage |
|---|---|---|---|---|
| Player | | 100 | 10, 20 | |
| Sword | Player | | | 12 |
| Goblin | | | | |

Each property is one array of values with one entry per node, so empty cells
hold nothing, and visiting every node with `Health` is one pass over one array.
Tree links are row numbers rather than pointers. The goblin has no health of its
own: it reads 100 from its type until it is given a value.

## Pages

- [Nodes and properties](nodes-and-properties.md): building the tree, reading
  and changing values, finding nodes, moving and removing them.
- [Values](values.md): the nine kinds of value a property can hold, and how they
  convert.
- [Types](types.md): default values shared by many nodes.
- [Changes and undo](changes-and-undo.md): the list of every change, versions,
  and edits that undo as one.
- [Tree files](tree-files.md): saving, loading, and the `.tree` format.

## Handles

`Table` and `Node` are handles. Copying one is cheap, and every copy refers to
the same table or node. A table lives as long as any handle to it or to one of
its nodes. A node handle becomes empty when the node is removed and works again
if the removal is undone; it never reaches a different node that later takes the
same row.

## Limitations

- A table is not safe to use from two threads at once. Use it from one thread,
  or guard it with a lock of your own.
- Saving writes the whole table each time. Undo history and the change list are
  not saved.
- Types give default values only. A type cannot extend another type, and a
  node's type cannot be changed after it is added.
- Sharing a table over a network arrives with the `network` library, as a bridge
  that uses the change list.
