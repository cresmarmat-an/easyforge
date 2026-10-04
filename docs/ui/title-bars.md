# Title bars

```cpp
window.TitleBar = ui::TitleBar({
    .Height = 40,
    .Background = Color::Hex("#15151A"),
    .Children = {
        ui::Image("icon.png", { .Width = 18, .Height = 18 }),
        ui::Label("Notes"),
        ui::Button("File", { .Style = ui::ButtonStyle::Subtle }),
        ui::Spacer(),
        ui::WindowButtons(),
    },
});
```

Assigning `window.TitleBar` hides the system's title bar and shows yours at the
top of the window, with the content below it. The bar still behaves like the
system's:

- Dragging any part of the bar that is not a control moves the window, and
  double-clicking there maximizes or restores it.
- The window's edges, including the top edge, still resize it.
- `ui::WindowButtons()` draws the minimize, maximize, and close buttons in the
  theme's colors, and the window acts on them as on its own. On Windows 11,
  resting the pointer on the maximize button shows the snap layouts.

A title bar is a [row](layout.md#containers). It takes `TitleBarSettings`, which
has the same fields as `ContainerSettings` with defaults of its own: 36 points
tall and as wide as the window, with its children centered from top to bottom,
8 points apart, and 12 points of padding at the left. Any of these can be given,
including values such as `.Gap = 0`. Its background is the theme's `TitleBar`
color.

Changing the bar's height, or hiding it, after the window is shown moves the
content to match before the next frame. A [menu](menus-and-dialogs.md) in the
bar opens below it, over the content.

Controls in the bar, such as buttons and text fields, work as anywhere else; only
the empty parts drag. Labels and images do not count as controls, so dragging
the title text moves the window.

## Window buttons

| WindowButtonsSettings | Default | Meaning |
|---|---|---|
| `Name` | empty | |
| `ButtonWidth` | 46 | Each button's width; their height is the bar's |
| `Minimize`, `Maximize`, `Close` | true | Which buttons show |
| `Visible` | true | |

Put the buttons last in the bar. The maximize button shows the restore symbol
while the window is maximized. Hovering the close button turns it red, as the
system's does.

## Limitations

- On Windows only, for now. On macOS the red, yellow, and green buttons will
  stay where macOS users expect them; on phones and the web there is no title
  bar, so it will not be shown.
- In full screen the title bar is hidden.
