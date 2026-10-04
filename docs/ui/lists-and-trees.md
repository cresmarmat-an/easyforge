# Lists and trees

```cpp
ui::List files({
    .Width = ui::Fill,
    .Items = { "notes.txt", "todo.txt", "ideas.txt" },
    .OnSelect = [](int row) { /* ... */ },
    .OnActivate = [](int row) { /* open it */ },
});

ui::Tree outline({ .Source = game, .OnSelect = [](Node node) { /* ... */ } });
```

A list shows rows of text and a tree shows the nodes of a
[data table](../data/overview.md). Both draw their rows themselves instead of
making an element for each, so a list of thousands of rows costs about as much
to lay out as a list of ten.

## Lists

| ListSettings | Default | Meaning |
|---|---|---|
| `Width`, `Height` | 240, 200 | |
| `Padding` | 4 | |
| `Items` | none | The rows' texts |
| `Selected` | -1 | The row chosen, counting from 0; -1 is none |
| `FontSize`, `Color` | the theme's | |
| `BorderWidth` | the theme's | |
| `OnSelect` | none | Called with the row chosen, by a click, the keyboard, or the program assigning `Selected` |
| `OnActivate` | none | Called with a row double-clicked, or with the chosen row when Enter is pressed |

Clicking a row chooses it and gives the list the keyboard. Up and Down choose the
row before or after, Page Up and Page Down move a page, and Home and End go to
the first and last row; the list scrolls to keep the chosen row in view. The
wheel scrolls without choosing. `list.Items` can be assigned at any time, and
`list.SelectedText()` is the chosen row's text.

## Trees

| TreeSettings | Default | Meaning |
|---|---|---|
| `Width`, `Height` | 240, 300 | |
| `Padding` | 4 | |
| `Source` | none | The table whose nodes are shown |
| `OpenAtStart` | true | Whether the top nodes start open |
| `FontSize`, `Color` | the theme's | |
| `OnSelect` | none | Called with the node chosen |
| `OnActivate` | none | Called with a node double-clicked, or with the chosen node when Enter is pressed |

Each row is a node's name, indented by its depth, with an arrow before nodes that
have children. Clicking the arrow opens or closes a node; double-clicking a row
does too. With the keyboard, Right opens the chosen node and then moves into it,
Left closes it and then moves to its parent, and the other keys work as in a
list.

The tree follows its table: nodes added, removed, renamed, or moved show up the
next time the tree is drawn, wherever the change came from. `tree.Selected` is
the chosen node, and assigning it chooses one. `tree.OpenNode(node)` opens a
node and every node above it, so it shows; `tree.CloseNode(node)` and
`tree.IsOpen(node)` work as their names say.

## Limitations

- Rows are one line of text each; rows built from other elements, such as an
  icon and two lines, are not available yet.
- One row is chosen at a time; there is no choosing several with Shift or
  Control.
- Rows cannot be reordered by dragging, though [drag and
  drop](events-and-focus.md#drag-and-drop) works between other elements.
