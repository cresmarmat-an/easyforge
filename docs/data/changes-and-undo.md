# Changes and undo

Every change to a table is recorded in one list, oldest first. Anything that
needs to follow the table asks for the changes since it last looked, so it only
has to deal with what is different. The same record is what undo uses.

```cpp
std::uint64_t seen = game.Version();

// ... the game runs, and anything changes the table ...

for (const Change& change : game.ChangesSince(seen))
{
    if (change.Kind == ChangeKind::PropertySet && change.Property == "Health")
    {
        Log("{} health went from {} to {}", change.Target.Name(), change.Old, change.New);
    }
}
seen = game.Version();
```

## Versions

`game.Version()` starts at 0 and goes up by one with every change. A change that
sets a property to the value it already has is not a change, and does not move
the version.

`game.ChangesSince(version)` gives every change after that version. Each has:

| Member | Meaning |
|---|---|
| `Version` | The table's version once this change was made |
| `Kind` | What happened, below |
| `Target` | The node that changed |
| `Parent` | Where it was added, removed from, or moved to; no node for the top of the table |
| `OldParent` | Where a moved node came from |
| `Property` | The property that was set, or the name of the type that was defined |
| `Old`, `New` | The property's values before and after, or the node's names for a rename |

| `ChangeKind` | What happened |
|---|---|
| `Added` | `Target` was added under `Parent` |
| `Removed` | `Target`, and everything under it, was removed from `Parent` |
| `PropertySet` | `Property` of `Target` went from `Old` to `New` |
| `Renamed` | `Target`'s name went from `Old` to `New` |
| `Moved` | `Target` moved from `OldParent` to `Parent` |
| `TypeDefined` | The type named `Property` got new values |

`Old` is nothing for a property that had no value of its own, and `New` is
nothing for a property that was cleared. A `Target` that has been removed since
tests as false; the change still says where it was.

Adding a node with starting values records the add and then one `PropertySet`
for each value. Removing a node records one change for the whole tree under it.

## How far back

The list keeps the last `ChangeLimit` changes (100000 unless set in
`Table::New`). `game.OldestVersion()` is the version of the oldest change still
kept. A reader whose last look was before `OldestVersion() - 1` has missed
changes and should read the whole table again:

```cpp
if (seen + 1 < game.OldestVersion())
{
    RebuildEverything();
}
```

A `ChangeLimit` of 0 keeps no list at all; the version still counts.

## Edits and undo

Changes are undone in edits. `BeginEdit(name)` starts one, `EndEdit()` finishes
it, and `Undo` reverses everything in it at once:

```cpp
game.BeginEdit("Move the chest");
chest["Position"] = Vector2 { 4, 8 };
chest["Locked"] = false;
game.EndEdit();

game.Undo();   // both back
game.Redo();   // both again
```

| Written | Meaning |
|---|---|
| `game.Undo()`, `game.Redo()` | Reverses or repeats one edit; false when there is none |
| `game.CanUndo()`, `game.CanRedo()` | Whether there is one to reverse or repeat |
| `game.UndoName()`, `game.RedoName()` | Its name, for a menu such as "Undo Move the chest" |

- Edits nest. `BeginEdit` inside an open edit adds to it, and only the outermost
  `EndEdit` finishes it, so a function that makes its own edit can be called
  from inside a bigger one.
- Nothing can be undone or redone while an edit is open.
- An edit with no changes is not kept.
- Making a new edit forgets everything that could be redone.
- `Undo` goes back at most `UndoLimit` edits (100 unless set in `Table::New`).
- Changes made outside an edit are recorded in the change list but cannot be
  undone. Undoing an edit puts back the values from before that edit, even when
  a property was changed again outside an edit afterwards.

Undoing and redoing are changes too: they appear in the change list like any
other, in the order they are applied, so anything following the table sees them.

Nodes removed in an edit keep their row while undo or redo can bring them back,
so their handles work again after `Undo`. The row is used for a new node only
once no edit refers to it.
