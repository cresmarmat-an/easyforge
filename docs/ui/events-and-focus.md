# Events and focus

The interface gets every event its window receives, before the window's
listeners (such as [input](../input/overview.md)'s controls) and `OnEvent`. An
event the interface used is marked handled, and those later in line can skip
it: clicking a button does not also fire the player's weapon, and typing in a
field does not also move the player.

## The pointer

A press goes to the deepest element under the pointer that answers it: a button,
a checkbox, a slider, a field, or a scroll. A press on something inside it that
does not answer, such as a picture in a button or a card in a scroll, goes to it
too. That element holds the pointer until it is released, so a slider follows a
drag even when the pointer leaves it. A click counts when the press and the
release are both over the element. A second finger pressing elsewhere lets go of
the first element without a click.

Clicks on an element with a visible background, such as a panel, stay with the
interface even though nothing answers them. Clicks on empty parts of rows and
columns without backgrounds, and on labels, images, and scene views, pass
through to the rest of the program.

The wheel goes to the scroll under the pointer. A touch works as the left
button does, and a finger dragging a scroll moves it.

`element.IsHovered()` is true while the pointer is over an element that answers
it or has a tooltip. Buttons ease into their hovered look.

## The keyboard

One element at a time has the keyboard. Clicking an element that takes the
keyboard gives it the keyboard; clicking anywhere else takes it away. Tab moves
it to the next element that takes the keyboard, and Shift with Tab to the one
before, in the order they are laid out. Escape takes it away. `element.Focus()`
gives it the keyboard from the program, and `root.Focused()` is the element that
has it.

When the keyboard moved the focus, a ring in the theme's `Focus` color shows
around the element; after a click, it does not. Text fields show focus with
their border instead.

A key press goes to the element with the keyboard first, then to the elements
around it, so a scroll can answer Page Down while a button inside it has the
keyboard.

| Element | Keys |
|---|---|
| Button | Enter or Space clicks |
| Checkbox, Toggle | Space or Enter changes it |
| Slider | Arrows, Page Up, Page Down, Home, End |
| Text field | See [text fields](elements.md#text-fields) |
| Text area | See [text areas](elements.md#text-areas) |
| Dropdown, Menu | See [menus and dropdowns](menus-and-dialogs.md) |
| List, Tree | See [lists and trees](lists-and-trees.md) |
| Tabs | Left, Right, Home, End |
| Scroll | Page Up, Page Down, Control with Home or End |

Disabled elements, and those inside disabled or hidden elements, never get the
keyboard or the pointer. An element that has the keyboard and is then hidden,
disabled, or taken out of the interface loses it. When a key moves the keyboard
somewhere else, as Space does on a button that focuses a field, the character
the key types is not given to the new element.

## Tooltips

```cpp
ui::Button("Export", { .Tooltip = "Saves a copy as PNG" })
```

Resting the pointer on an element with a `Tooltip` for a moment shows the text
next to the pointer, in the theme's tooltip colors. Moving away or pressing
hides it.

## Drag and drop

```cpp
ui::Label card("Buy milk");
card.DragText = "task-12";

ui::Panel done({ .Width = 200, .Height = 300 });
done.OnDrop = [](const std::string& text) { MarkDone(text); };
done.OnFilesDropped = [](const std::vector<std::string>& paths) { Import(paths); };
```

Any element with `DragText` can be picked up: press on it and move the pointer
more than four points. A faded copy of the element follows the pointer, and the
element under the pointer that has `OnDrop` is outlined in the theme's accent
color. Letting go there calls its `OnDrop` with the drag text; letting go
anywhere else, or pressing Escape, does nothing. A button picked up this way is
not clicked. The deepest element with `OnDrop` under the pointer takes the drop,
and an element cannot be dropped onto itself or onto anything inside it.

`OnFilesDropped` is called with the paths of files dragged in from the system
and dropped on the element, when the window reports them.

## The pointer's shape

`.Cursor = Cursor::Hand` sets the pointer's shape over an element and what is
in it. Without one, text fields show the text cursor and everything else the
arrow.

## Limitations

- Only the left button and touches click; right clicks and the middle button do
  nothing to elements yet.
- Several fingers at once are not told apart; each touch acts as the pointer.
- Drag and drop carries text, within one window; dragging to other programs is
  not available.
