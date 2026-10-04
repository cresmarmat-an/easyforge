# Displays

```cpp
ui::Displays pages;
pages = ui::Displays({
    .Start = "Menu",
    .Children = {
        ui::Display("Menu", {
            .Children = {
                ui::Button("Play", { .OnClick = [&pages] { pages.Show("Game", ui::Transition::Fade(0.4f)); } }),
                ui::Button("Settings", { .OnClick = [&pages] { pages.Show("Settings", ui::Transition::Slide(0.3f)); } }),
            },
        }),
        ui::Display("Settings", { .Children = { /* ... */ ui::Button("Back", { .OnClick = [&pages] { pages.Back(); } }) } }),
        ui::Display("Game", { .Children = { /* ... */ } }),
    },
});
window.Content = pages;
```

A `ui::Displays` holds several displays in one place and shows one at a time:
the screens of a game, or the pages on the right of a settings screen. It is an
ordinary element, so it can fill the window or just one area of it. Without a
size it fills.

A `ui::Display` is a [column](layout.md#containers) with a name:
`ui::Display("Settings", { .Padding = 24, .Children = { ... } })`.

## Showing a display

| Written | Does |
|---|---|
| `pages.Show("Settings")` | Shows the display with that name, with the default transition |
| `pages.Show("Settings", ui::Transition::Slide(0.3f))` | Shows it with a transition |
| `pages.Back()` | Goes back to the display shown before, with the transition that led here reversed |
| `pages.CanGoBack()` | Whether there is anything to go back to |
| `pages.Current` | The name of the display shown; assigning it shows that display |
| `pages.Transition` | The default transition, `Cut` unless set |

`Show` returns false, and logs a warning, when there is no display with the
name. `Back` returns false when there is nothing to go back to.

The display shown first is `.Start`, or the first child without one.

## Transitions

| Transition | The new display |
|---|---|
| `ui::Transition::Cut()` | Appears at once |
| `ui::Transition::Fade(seconds)` | Fades in over the old one |
| `ui::Transition::Slide(seconds, ui::SlideFrom::Right)` | Pushes the old one out, from the right, left, bottom, or top |
| `ui::Transition::Scale(seconds)` | Grows into place as the old one shrinks a little and fades |
| `ui::Transition::Shader(shader, seconds)` | Is drawn over the old one through your shader |

Going back reverses a slide, so the old display comes back from the side it
left by. Transitions ease in and out; `transition.Easing` changes that.

A shader transition gets `value Progress: number`, going from 0 to 1, and reads
the new display as `input.Content`. At 0 nothing of it should show, and at 1 all
of it:

```
-- wipe.shader: reveals the new display from left to right.
value Progress: number = 0

function Pixel(input: PixelInput) returns color then
    constant edge = Progress * 1.05
    variable shown = Sample(input.Content, input.Coordinates)
    shown.Alpha = shown.Alpha * (1 - SmoothStep(edge - 0.05, edge, input.Coordinates.X))
    return shown
end
```

## What happens during a transition

Both displays are drawn while it runs; only the new one gets events. A display
keeps its state while hidden: where it is scrolled to, what was typed, which
switches are on. Showing another display while one is still coming in finishes
the first one at once.

## Callbacks

| Callback | Called |
|---|---|
| `DisplaysSettings::OnChange` | With the name of the display being shown, as it starts to show |
| `display.OnShown` | When that display starts to be shown |
| `display.OnHidden` | When that display has gone, after its transition |

If the element with the keyboard is in the display being hidden, it loses the
keyboard.

## Tabs

```cpp
ui::Tabs settings({
    .Children = {
        ui::Display("General", { .Padding = 16, .Children = { /* ... */ } }),
        ui::Display("Advanced", { .Padding = 16, .Children = { /* ... */ } }),
    },
});
settings.Current = "Advanced";
```

`ui::Tabs` shows a strip of tabs above its pages, one page at a time, without a
transition. Each page's name is its tab's text, and the page shown has an
accent line under its tab.

| TabsSettings | Default | Meaning |
|---|---|---|
| `Width`, `Height` | `Fill` | |
| `Padding` | none | Around the page, below the strip |
| `Start` | the first page | The page shown first |
| `FontSize`, `Color` | the theme's | The tabs' text; tabs not shown use the muted text color |
| `OnChange` | none | Called with the name of the page shown, when it changes |
| `Children` | none | The pages, usually `ui::Display` |

Clicking a tab shows its page and gives the tabs the keyboard. While the tabs
have the keyboard, Left and Right move to the tab before or after, and Home and
End to the first and last; arrows pressed inside a page stay with the page.
Pages keep their state while hidden, and a page's `OnShown` and `OnHidden` are
called as for displays. `tabs.Current` is the page shown, and assigning it shows
another. Tabs is as large as its largest page, so it keeps its size between
pages. A page with `Visible` set to false has no tab.

## Limitations

- During a fade or scaling, a frosted glass effect inside a display blurs only
  what is in the display, not what is behind the displays, until the transition
  ends.
- A fade draws each display at part strength, so in the middle of a fade between
  two displays with backgrounds, a little of what is behind both shows through.
- Tabs that do not fit across the strip are cut off; there is no scrolling
  through them.
- `Back` goes back through the displays shown with `Show` and `Current`; it keeps
  no history across separate `ui::Displays`.
