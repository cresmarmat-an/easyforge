# Live data

```cpp
Table game = Table::New();
Node player = game.Add("Player", { { "Name", "Ari" }, { "Health", 100 } });

window.Content = ui::Column({
    .Padding = 16,
    .Children = {
        ui::Label(ui::Bind(player["Name"]), { .FontSize = 24 }),
        ui::Label(ui::Bind(player["Health"], "Health: {}")),
    },
});

player["Health"] = 90;   // the label shows "Health: 90" in the next frame
```

`ui::Bind(cell, format)` makes a binding to one property of a node in a
[data table](../data/overview.md). A label made with a binding follows the
value by itself: whenever it changes, wherever the change comes from (your code,
an undo, a script, or later the network), the label shows the new value in the
next frame. Nothing needs to tell the label to update.

`format` is a `std::format` string with one `{}` for the value; without one the
value is shown as it is. Text is shown without quotes, and numbers, vectors, and
colors as a [.tree file](../data/tree-files.md) writes them.

| Written | Does |
|---|---|
| `label.Binding = ui::Bind(cell, "Score: {}")` | Starts following another value |
| `label.Text = "Gone"` | Shows fixed text, and stops following |
| `binding.Text()`, `binding.Value()` | The text for the current value, and the value |

## Following changes yourself

A label follows one value. For anything else, such as a bar that follows
health or a list that follows a table, read the table's list of changes once a
frame:

```cpp
std::uint64_t seen = game.Version();
window.OnFrame = [&](float) {
    for (const Change& change : game.ChangesSince(seen))
    {
        if (change.Kind == ChangeKind::PropertySet && change.Property == "Health")
        {
            healthBar.Value.AnimateTo(change.New.As<float>());
        }
    }
    seen = game.Version();
};
```

[Changes and undo](../data/changes-and-undo.md) describes the list. Example 05
in [easyforge-examples](https://github.com/cresmarmat-an/easyforge-examples)
does both, over a 3D scene.

## Limitations

- Only labels follow a binding. Other elements follow data through the change
  list, as above.
- A binding checks its value once a frame while its label is in an interface
  that is drawing.
