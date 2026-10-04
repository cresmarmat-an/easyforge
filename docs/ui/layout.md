# Layout

```cpp
window.Content = ui::Row({
    .Children = {
        ui::Panel({ .Width = 220, .Padding = 12, .Gap = 4, .Children = { /* the sidebar */ } }),
        ui::Column({
            .Width = ui::Fill,
            .Padding = 24,
            .Gap = 12,
            .Children = {
                ui::Label("Shopping", { .FontSize = 28 }),
                ui::TextField({ .Width = ui::Fill, .Placeholder = "Add an item" }),
                ui::Scroll({ .Height = ui::Fill, .Children = { /* the items */ } }),
            },
        }),
    },
});
```

## Containers

| Element | Places its children |
|---|---|
| `ui::Column` | One under another, top to bottom |
| `ui::Row` | Side by side, left to right |
| `ui::Stack` | On top of each other, the last on top |
| `ui::Panel` | As a column, in a box with the theme's surface color and rounded corners |
| `ui::Grid` | In rows of equal columns, left to right |
| `ui::Scroll` | As a column (or a row) that can be longer than the scroll itself |

`ui::Display` and `ui::TitleBar` are containers too; see [displays](displays.md)
and [title bars](title-bars.md).

Row, Column, Stack, and Panel take `ui::ContainerSettings`. Every field is
optional, and those you write must come in this order:

| ContainerSettings | Default | Meaning |
|---|---|---|
| `Name` | empty | The name [Find](properties-and-animation.md#finding-elements) looks for |
| `Width`, `Height` | `ui::Fit` | See [sizes](#sizes) |
| `MinimumWidth`, `MinimumHeight` | 0 | The smallest the element gets |
| `MaximumWidth`, `MaximumHeight` | `ui::Unlimited` | The largest the element gets |
| `Margin` | 0 | Space kept free around the element |
| `Padding` | 0 | Space inside the element, around its children |
| `Gap` | 0 | Space between children |
| `Alignment` | `Stretch` | Where children sit across a row or column, or inside a stack |
| `Distribution` | `Start` | How a row or column spreads its children along itself |
| `CornerRadius`, `Background`, `BorderWidth`, `BorderColor` | none | The box behind the children; see [effects](effects-and-shaders.md#backgrounds-and-borders) |
| `Opacity`, `Offset`, `Scale` | 1, 0, 1 | How it is drawn; see [animation](properties-and-animation.md#drawn-but-not-moved) |
| `Effects`, `Shader`, `ShaderValues` | none | See [effects and shaders](effects-and-shaders.md) |
| `Visible`, `Enabled` | true | A hidden element takes no space; a disabled one does not respond |
| `Tooltip`, `Cursor` | none | See [events](events-and-focus.md) |
| `Children` | none | The elements inside |

## Sizes

A width or height is one of:

| Written | Size |
|---|---|
| `200` | 200 points |
| `ui::Percent(50)` | Half of the parent's size inside its padding |
| `ui::Fill` | The space the parent has left |
| `ui::Fit` | As small as the content allows; the default for most elements |

In a row, the children that fill share the width the others leave, equally; in
a column, the height. A child held back by its maximum keeps that size, and the
others share what it leaves. Across a row or column, a child that fills is as
tall or wide as the row or column. When a row or column works out its own size
from its children (it fits its content), each child that fills gets as much as
the widest of them needs. A wrapped label that fills is measured at the width it
ends up with, so the row is as tall as its lines.

A size left out of a settings struct is the element's own: `ui::Fit` for most,
and a size of its kind's for some, such as 200 points for a text field.
`ui::Fit` given on purpose is always Fit, and assigning `ui::Size {}` to a
property goes back to the element's own size.

`MinimumWidth`, `MaximumWidth`, `MinimumHeight`, and `MaximumHeight` limit
whatever size an element would otherwise take.

The element a window shows fills the window unless it has a size of its own, so
a column assigned to `window.Content` covers the whole window and its background
does too. Its minimum and maximum sizes still apply: a column with
`.MaximumWidth = 720` stays 720 points wide in a wider window.

A point is a pixel at 100% scaling; on a screen scaled to 150% a point is 1.5
pixels. Every size and position in `ui` is in points.

## Padding and margins

```cpp
.Padding = 16              // every side
.Padding = { 16, 8 }       // left and right, then top and bottom
.Margin = { 4, 8, 4, 0 }   // left, top, right, bottom
```

Padding is inside the element, between its edge and its content, and its
background covers it. A margin is outside, and nothing is drawn there.

## Alignment and distribution

`Alignment` places children across a row or column: `Start` (left in a column,
top in a row), `Center`, `End`, or `Stretch`, the default, which makes children
that fit their content as wide as the column or as tall as the row. A child with
a size of its own keeps it.

`Distribution` spreads children along a row or column when none of them fills:

| Distribution | Spare space goes |
|---|---|
| `Start` | After the last child |
| `Center` | Half before the first child, half after the last |
| `End` | Before the first child |
| `SpaceBetween` | Between the children, none at the ends |
| `SpaceAround` | Around each child, so the ends get half as much as between |
| `SpaceEvenly` | Equally between the children and at both ends |

In a stack, `Alignment` places each child on both axes: `Center` puts it in the
middle, and `Stretch` makes children that fit their content fill the stack.

## Grids

```cpp
ui::Grid({ .Width = 480, .Columns = 3, .ColumnGap = 12, .RowGap = 12, .Children = cards })
```

The columns share the grid's width equally. Each row is as tall as its tallest
cell, and `Alignment` places each child in its cell. `GridSettings` has the same
fields as `ContainerSettings`, with `Columns`, `ColumnGap`, and `RowGap` in place
of `Gap` and `Distribution`.

## Scrolling

```cpp
ui::Scroll list({ .Height = 300, .Gap = 6, .Children = rows });
list.ScrollTo(0);
list.ScrollIntoView(rows.back());
```

A scroll lays its children out as a column (or as a row, with
`.Direction = ui::ScrollDirection::Horizontal`) and shows the part that fits in
its own box. The mouse wheel scrolls it smoothly, dragging the bar at its edge
moves it, a finger drags it and it keeps going for a moment after the finger
lets go, and Page Up, Page Down, Control with Home, and Control with End work
while something inside it has the keyboard. `list.Position` is how far it is
scrolled, in points. The bar can be grabbed even where a child lies under it.

`ScrollTo` and `ScrollIntoView` work right after children are added: the scroll
finishes after the next layout, when the new children have their places. A
position set before the scroll is first shown is kept. Percent sizes along a
scroll are shares of its view, so pages of `ui::Percent(100)` each fill it.

A scroll that fits its content never needs to scroll; give it a height, or
`ui::Fill`, to make it smaller than what it holds.

## Spacers

`ui::Spacer()` fills: in a row it pushes everything after it to the right end,
and in a column to the bottom. `ui::Spacer(16)` is 16 points of space in both
directions.

## Adding and removing children later

```cpp
items.Add(ui::Checkbox("Milk"));
items.Insert(0, ui::Label("First"));
item.Remove();
items.Clear();
```

A child added to a container leaves the one it was in. A removed element keeps
its settings and can be added again. Layout runs again in the next frame.

## Limitations

- Sizes are points, shares, fill, or fit; there is no aspect ratio setting, apart
  from images, which keep their shape when only one side is given.
- Fill children share space equally; there are no weights.
- A grid's columns are equal; there are no column widths of their own.
- Scrolling goes one way at a time: down, or across.
