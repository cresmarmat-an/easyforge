# data

`data` will hold any data as a tree that is stored like one big table: each node
is a row and each property is a column. Every change is recorded, so the same
table can be undone, saved, shown in an interface, and shared over a network.

> [!NOTE] Not available yet
> `data` is step 6 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- `Table::New()`, `table.Add("Player")`, and `player["Health"] = 100` to build
  and change a tree of nodes with any properties.
- Types with default values, so a new "Enemy" starts with the right health.
- A list of every change, which `ui` uses to update only what changed.
- Undo and redo, grouped into named edits.
- A readable `.tree` file format that shows differences well in version control.
- Sharing a table between machines one way, from a server to its clients, or two
  ways, where each node is changed only by its owner.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#data).
