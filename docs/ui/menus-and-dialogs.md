# Menus, dropdowns, and dialogs

```cpp
ui::Dialog confirm({
    .Title = "Delete the note?",
    .Children = { ui::Label("This cannot be undone.") },
    .Buttons = {
        ui::Button("Cancel", { .OnClick = [&] { confirm.Close(); } }),
        ui::Button("Delete", { .Style = ui::ButtonStyle::Accent, .OnClick = [&] { DeleteNote(); confirm.Close(); } }),
    },
});

window.TitleBar = ui::TitleBar({
    .Children = {
        ui::Menu("File", {
            .Items = {
                { .Text = "New", .OnClick = [] { NewNote(); }, .Shortcut = "Ctrl+N" },
                { .Separator = true },
                { .Text = "Delete", .OnClick = [&] { confirm.Open(ui::Root::Of(window)); } },
            },
        }),
        ui::Dropdown({ .Options = { "Small", "Medium", "Large" }, .Selected = 1 }),
        ui::Spacer(),
        ui::WindowButtons(),
    },
});
```

Menus and dropdowns open a list below themselves, over the rest of the
interface; a dialog opens over everything, in the middle. All three are drawn by
the root they belong to, above its content and title bar, so they are never cut
off by a scroll area or a small panel.

## Dropdowns

| DropdownSettings | Default | Meaning |
|---|---|---|
| `Width` | 200 | |
| `Padding` | 10 at the left, 30 at the right for the arrow, 6 above and below | |
| `Options` | none | The texts to choose from |
| `Selected` | -1 | The option chosen, counting from 0; -1 is none |
| `Placeholder` | empty | Shown while nothing is chosen |
| `FontSize`, `Color` | the theme's | |
| `OnChange` | none | Called with the option chosen, when a person chooses one or the program assigns `Selected` |

Clicking a dropdown, or pressing Enter or Space while it has the keyboard, opens
its list with the chosen option highlighted. In the list, the arrow keys, Page
Up, Page Down, Home, and End move, Enter or Space chooses, and Escape, Tab, or a
click elsewhere closes it without choosing. While the list is closed, Up and
Down choose the option before or after, and Alt with Down opens the list.
`dropdown.SelectedText()` is the chosen option's text. The list is at least as
wide as the dropdown, at most 320 points tall, and scrolls past that. It opens
above the dropdown when there is no room below.

## Menus

| MenuSettings | Default | Meaning |
|---|---|---|
| `Padding` | 10 at the sides, 6 above and below | |
| `Items` | none | The entries, in order |
| `Style` | `Subtle` | The button's look, as for a [button](elements.md#buttons) |
| `FontSize`, `Color` | the theme's | |

Each `ui::MenuItem` has:

| MenuItem | Default | Meaning |
|---|---|---|
| `Text` | empty | |
| `OnClick` | none | Called after the menu has closed |
| `Shortcut` | empty | Text shown at the right, such as "Ctrl+S" |
| `Enabled` | true | A disabled entry is dimmed and cannot be chosen |
| `Separator` | false | A line between groups; `Text` and `OnClick` are not used |

A menu is a button that opens its entries below it. The pointer and the arrow
keys move through them, skipping separators and disabled entries, and Enter,
Space, or a click chooses one. Down opens the menu with its first entry
highlighted. `menu.Open()`, `menu.Close()`, and `menu.IsOpen()` do the same from
code. The shortcut text is only shown: the program handles its own shortcuts,
for example in the window's `OnEvent`.

## Dialogs

| DialogSettings | Default | Meaning |
|---|---|---|
| `Width` | 440 | |
| `Padding` | 24 | |
| `Gap` | 16 | Between the title, the children, and the buttons |
| `Title` | empty | Shown at the top in a larger size |
| `ClosesOnOutsideClick` | false | Whether a click outside the dialog closes it |
| `OnClosed` | none | Called when the dialog has closed, however it was closed |
| `Children` | none | Laid out in a column, as in a [Column](layout.md#containers) |
| `Buttons` | none | Laid out in a row at the bottom right |

`dialog.Open(root)` shows the dialog over the root's interface and dims
everything behind it. The keyboard moves to the first element in the dialog that
takes it, and Tab and Shift with Tab stay inside the dialog. Clicks outside it do
nothing unless `ClosesOnOutsideClick` is set. Escape closes it, as does
`dialog.Close()`, and the keyboard goes back to where it was before the dialog
opened. Without a background of its own, a dialog has the theme's surface color,
rounder corners than a panel, and a shadow. `dialog.IsOpen()` says whether it is
showing; a dialog can be opened again after it closes.

## Limitations

- Menus have one level: an entry cannot open a menu of its own.
- A dialog is drawn inside its window. There are no system dialogs for opening
  or saving files.
- Dropdowns hold text options only.
