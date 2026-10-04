# Elements

```cpp
ui::Column({
    .Padding = 24,
    .Gap = 12,
    .Children = {
        ui::Label("Settings", { .FontSize = 28 }),
        ui::TextField({ .Placeholder = "Your name" }),
        ui::Toggle("Dark theme", { .OnChange = [](bool on) { /* ... */ } }),
        ui::Slider({ .Value = 0.7f, .OnChange = [](float value) { /* ... */ } }),
        ui::Button("Save", { .Style = ui::ButtonStyle::Accent, .OnClick = [] { /* ... */ } }),
    },
})
```

Every element's settings struct starts with the fields every element has, in the
same order as [`ContainerSettings`](layout.md#containers) (`Name`, sizes,
`Margin`, `Padding`), then the element's own fields, then the box, effects,
`Visible`, `Enabled`, `Tooltip`, and `Cursor`, then its events. Only the
element's own fields are listed below.

## Labels

```cpp
ui::Label("Hello", { .FontSize = 32 })
ui::Label(longText, { .Width = 400, .Wrap = true })
ui::Label(ui::Bind(player["Health"], "Health: {}"))
```

| LabelSettings | Default | Meaning |
|---|---|---|
| `FontSize` | the theme's | In points |
| `Color` | the theme's text color | |
| `TextAlignment` | `Start` | Where lines sit: `Start`, `Center`, or `End` |
| `Wrap` | false | Breaks lines between words to fit the width the label is given |
| `Font` | the theme's | Any `Font` |

A new line starts at each `"\n"`. A word wider than a whole line is broken
between characters. A label draws its text centered from top to bottom in its
box, so it lines up with buttons beside it in a row. Following a value from a
data table is described in [live data](live-data.md).

## Images

```cpp
ui::Image("icon.png", { .Width = 20, .Height = 20 })
ui::Image(texture, { .Width = ui::Fill, .Fit = ui::ImageFit::Cover })
```

| ImageSettings | Default | Meaning |
|---|---|---|
| `Fit` | `Contain` | How the picture fills the box: `Stretch`, `Contain`, `Cover`, or `Center` |
| `Tint` | white | Multiplies every pixel; lower its alpha to fade the picture |
| `Slice` | 0 | Keeps this many pixels at each edge from stretching, with `Stretch` |

Without a size, an image is one point for each pixel of the picture. With only
its width or only its height given, the other side keeps the picture's shape.
`Contain` shows the whole picture inside the box, `Cover` fills the box and cuts
off what does not fit, and `Center` shows it at its own size in the middle.
`image.Source = "other.png"` loads another file. `CornerRadius` rounds the
picture's corners.

## Buttons

```cpp
ui::Button("Save", { .Style = ui::ButtonStyle::Accent, .OnClick = [] { Save(); } })
ui::Button("", { .Children = { ui::Image("add.png"), ui::Label("Add") } })
```

| ButtonSettings | Default | Meaning |
|---|---|---|
| `Padding` | 14 at the sides, 6 above and below | |
| `Style` | `Normal` | `Normal` (the theme's control color, with a border), `Accent` (the theme's accent, for the one action a screen is about), or `Subtle` (no box until the pointer is on it) |
| `FontSize`, `Color` | the theme's | The text's size and color |
| `TextAlignment` | `Center` | `Start` suits buttons in a list |
| `OnClick` | none | Called when the button is clicked, or pressed with Enter or Space while it has the keyboard |
| `Children` | none | Shown side by side in place of the text |

A click counts when the pointer is pressed and released over the button. The
button eases to its hovered and pressed colors over the theme's transition time.
`button.Click()` calls `OnClick` as a click would. With a `Background` of your
own, hovering and pressing darken or lighten it slightly.

## Checkboxes and toggles

```cpp
ui::Checkbox("Remember me", { .Checked = true })
ui::Toggle("Full screen", { .OnChange = [window](bool on) { window.Fullscreen = on; } })
```

Both take `ui::CheckSettings`:

| CheckSettings | Default | Meaning |
|---|---|---|
| `Checked` | false | Whether it is checked, or for a toggle, on |
| `FontSize`, `Color` | the theme's | The text's size and color |
| `OnChange` | none | Called with the new state whenever it changes |

Clicking the box, the switch, or the text changes it, and so does Space or Enter
while it has the keyboard. `OnChange` is also called when the program assigns
`Checked`, but not when the value stays the same.

## Sliders and progress bars

```cpp
ui::Slider({ .Value = 5, .Maximum = 10, .Step = 1, .OnChange = [](float value) { /* ... */ } })
ui::ProgressBar({ .Value = 0.4f })
```

Both take `ui::RangeSettings`:

| RangeSettings | Default | Meaning |
|---|---|---|
| `Width` | 200 | |
| `Value` | 0 | |
| `Minimum`, `Maximum` | 0, 1 | |
| `Step` | 0 | A slider stops only at multiples of the step from `Minimum`; 0 stops anywhere |
| `Color` | the theme's accent | The filled part |
| `OnChange` | none | A slider calls it with the new value whenever it changes |

A slider follows the pointer while pressed, and the arrow keys move it by a step
(or a hundredth of the range without one), Page Up and Page Down by ten steps,
and Home and End to either end. The mouse wheel moves it while it has the
keyboard. Values assigned by the program are kept in range and on a step too.
A progress bar's `Value` can move gradually: `bar.Value.AnimateTo(0.8f)`.

## Text fields

```cpp
ui::TextField name({ .Placeholder = "Your name", .OnSubmit = [](const std::string& text) { /* ... */ } });
ui::TextField password({ .Password = true, .MaximumLength = 64 });
```

| TextFieldSettings | Default | Meaning |
|---|---|---|
| `Width` | 200 | |
| `Padding` | 8 at the sides, 6 above and below | |
| `Text` | empty | |
| `Placeholder` | empty | Shown in the theme's muted color while the field is empty |
| `FontSize`, `Color` | the theme's | |
| `Password` | false | Shows a dot for each character, and does not copy |
| `MaximumLength` | 0 | The most characters the field takes; 0 is no limit |
| `OnChange` | none | Called after every change to the text, typed or assigned |
| `OnSubmit` | none | Called when Enter is pressed in the field |

The field edits one line of text:

| Keys | Do |
|---|---|
| Left, Right | Move by a character; with Control, by a word |
| Home, End | Move to the start or end |
| Shift with any of those | Selects as it moves |
| Backspace, Delete | Remove a character, or with Control, a word, or the selection |
| Control with A, C, X, V | Select all, copy, cut, paste |
| Control with Z; Control with Y, or Control, Shift, and Z | Undo; redo |
| Enter | Calls `OnSubmit` |

Clicking places the caret, dragging selects, a double click selects a word, and
a triple click selects everything. Typing in a row undoes as one step. While the
field has the keyboard, the window's input method is on, so Chinese, Japanese,
and Korean text can be typed; what the input method is composing shows at the
caret, underlined, and the keys belong to the input method until it is done.
Keys pressed in the field do not reach the rest of the program, so typing a W
does not also move a player. Control together with Alt is AltGr on many
keyboards, which types characters such as @, so it is not taken as a shortcut.

The caret moves between characters as UTF-8 encodes them (code points).
Characters built from several code points, such as some emoji, take more than
one press of an arrow key to cross.

## Text areas

```cpp
ui::TextArea notes({ .Width = ui::Fill, .Height = 240, .Placeholder = "Write something" });
ui::TextArea log({ .Wrap = false, .OnChange = [](const std::string& text) { /* ... */ } });
```

| TextAreaSettings | Default | Meaning |
|---|---|---|
| `Width`, `Height` | 300, 120 | With `.Height = ui::Fit`, it grows with its text |
| `Padding` | 8 at the sides, 6 above and below | |
| `Text`, `Placeholder` | empty | |
| `FontSize`, `Color` | the theme's | |
| `Wrap` | true | Breaks lines between words to fit; without it, long lines scroll sideways |
| `MaximumLength` | 0 | The most characters the area takes; 0 is no limit |
| `OnChange` | none | Called after every change to the text |

A text area edits several lines with the same keys, selection, undo, and input
methods as a text field, plus these:

| Keys | Do |
|---|---|
| Enter | Starts a new line |
| Up, Down | Move a line, keeping the column through shorter lines |
| Page Up, Page Down | Move a page of lines |
| Home, End | Move to the start or end of the line |
| Control with Home or End | Move to the start or end of the text |

The wheel scrolls the text, and the view follows the caret as it moves. A bar at
the right shows where the view is while there is more text than fits. A triple
click selects a line. Pasted text keeps its line breaks, without carriage
returns.

## Drawing areas

```cpp
ui::DrawingArea({
    .Width = ui::Fill,
    .Height = 200,
    .OnDraw = [](Canvas& canvas, Vector2 size) {
        canvas.Line({ 0, 0 }, size, { .Color = Color::White, .Width = 2 });
    },
})
```

`OnDraw` is called every frame the area is on screen, with the same
[Canvas](../graphics/canvas.md) a renderer gives. Points start at the area's top
left inside its padding, and drawing is cut to that box. `OnDraw` can take just
the canvas, or the canvas and the area's size in points.

```cpp
std::vector<Vector2> dots;
ui::DrawingArea board({
    .Width = ui::Fill,
    .Height = ui::Fill,
    .OnDraw = [&dots](Canvas& canvas) {
        for (Vector2 dot : dots)
        {
            canvas.Circle(dot, 6, { .Color = Color::Hex("#6FC3FF") });
        }
    },
    .OnPress = [&dots](Vector2 point) { dots.push_back(point); },
});
```

`OnPress`, `OnMove`, and `OnRelease` follow the pointer in the same points as
`OnDraw`. `OnPress` comes when the area is pressed with the left button or a
finger. `OnMove` comes when the pointer moves over the area, and also outside
it while the area is held, so a drag can be followed to its end. `OnRelease`
comes when the press ends, wherever it ends. An area with any of them takes the
presses on it, so they do not reach the program's own controls; without them,
presses pass through, as they do on a scene view.

## 3D scenes

```cpp
ui::Stack({
    .Children = {
        ui::SceneView(scene),
        ui::Column({ .Padding = 16, .Children = { ui::Label(ui::Bind(player["Health"], "Health: {}")) } }),
    },
})
```

A scene view draws a [scene](../graphics/scenes.md) through its camera, filling
the space it has unless given a size. It does not use the pointer, so clicks on
the game reach the program's own controls, while clicks on the interface over
it do not.

More elements have pages of their own: [menus, dropdowns, and
dialogs](menus-and-dialogs.md), [lists and trees](lists-and-trees.md), and
[tabs](displays.md#tabs).

## Limitations

- Buttons, checkboxes, and toggles are clicked with the left button or a touch;
  the other buttons do nothing to them.
- A text area lays out all of its text each time the text or its width changes,
  which suits notes and messages more than books.
- Tab in a text area moves the keyboard to the next element; it does not type a
  tab.
