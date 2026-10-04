# Themes

```cpp
ui::Root root = ui::Root::Of(window);
root.Theme = ui::Theme::Dark();

ui::Theme theme = ui::Theme::Light();
theme.Accent = Color::Hex("#E4572E");
theme.CornerRadius = 10;
root.Theme.AnimateTo(theme, { .Duration = 0.3f });
```

A theme holds the colors, font, and shapes an interface uses wherever an element
has no value of its own. A window's interface starts with the system's light or
dark theme and switches when the system does. Assigning a theme stops that; the
interface then keeps the theme it was given.

## What a theme holds

| Color | Used for |
|---|---|
| `Background` | Behind everything: the window's own color |
| `Surface` | Panels, cards, and a slider's knob |
| `Control`, `ControlHovered`, `ControlPressed` | Buttons, fields, and other controls |
| `Text` | Text |
| `MutedText` | Placeholders, hints, scroll bars, and a toggle's knob when off |
| `Accent`, `AccentHovered`, `AccentPressed` | Accent buttons, checked boxes, sliders, and focused fields |
| `AccentText` | Text and marks on the accent color |
| `Border` | Lines around fields and buttons, and the empty part of sliders and progress bars |
| `Focus` | The ring around whatever the keyboard moved to |
| `Selection` | Behind selected text |
| `TitleBar`, `TitleBarText`, `TitleButtonHovered`, `CloseButtonHovered` | Title bars and their buttons |
| `DisabledText` | Controls that cannot be used right now |
| `Tooltip`, `TooltipText` | Tooltips |

| Value | Default | Meaning |
|---|---|---|
| `Font` | the system's | Without one, Segoe UI on Windows |
| `FontSize` | 15 | In points |
| `CornerRadius` | 6 | For buttons, panels, and fields |
| `BorderWidth` | 1 | For buttons and fields |
| `Transition` | 0.12 | Seconds that hover and press changes take |

`ui::Theme::Light()` and `ui::Theme::Dark()` are the two built-in themes, and
`ui::Theme::For(ColorScheme::Dark)` picks one.

## Switching gradually

`root.Theme.AnimateTo(theme, settings)` moves every color and number from the
current theme to the new one over the animation's duration. The font switches
halfway.

## Themes in files

```cpp
Result<ui::Theme> loaded = ui::Theme::Load("sunset.tree");
if (loaded)
{
    root.Theme = *loaded;
}
```

A theme file is a [.tree file](../data/tree-files.md) with a node named
`Theme` and a property for each value it changes:

```
Theme
    Base = "Dark"
    Accent = #E4572E
    AccentHovered = #EE6A44
    CornerRadius = 10
    Font = "Inter.ttf"
```

Values the file leaves out come from the light theme, or from the dark one with
`Base = "Dark"`. Colors are written as `#RRGGBB` or `#RRGGBBAA`. A value of the
wrong kind, such as a number for a color, fails with the file and the name.
`theme.Save(path)` writes every color and number; it does not write the font,
which a theme holds as a loaded font rather than a file name.

## Limitations

- A theme is one set of values; styles per kind of element, such as all
  buttons with square corners, are set on each element or with a function that
  makes them.
- The font is one font, used at different sizes; there is no bold or italic
  variant in the theme yet.
