# Custom title bars

A window can draw its own title bar in place of the system's, with its own
colors, icon, tabs, or search box, and still behave like a title bar: dragging
it moves the window, double-clicking maximizes it, the edges resize it, and on
Windows 11 hovering the maximize button shows the snap layouts.

With `ui`, a title bar is an interface like any other:

```cpp
window.TitleBar = ui::TitleBar({
    .Height = 40,
    .Background = Color::Hex("#15151A"),
    .Children = {
        ui::Image("icon.png", { .Width = 20, .Height = 20 }),
        ui::Label("Notes"),
        ui::Spacer(),
        ui::WindowButtons(),
    },
});
```

`ui` arrives in a later step. This page explains what the window does with any
title bar view, which is also how to write one without `ui`.

## What the window does

Assigning a view to `window.TitleBar`:

- hides the system's title bar, so the content reaches the top edge of the
  window. The left, right, and bottom edges and the shadow stay the system's.
- keeps the content's size. The window gets shorter by the height of the
  system's title bar, and the title bar view is drawn inside the content.
- asks the view how tall it is, with `PreferredSize`, and places it across the
  top. The `Content` view is placed below it.
- asks the view what each point of it is, with `HitTest`, whenever the mouse is
  over it.

Assigning `nullptr` brings the system's title bar back.

In full screen the title bar view is hidden: it is placed with a height of zero,
gets no events or frames, and the content fills the screen.

## What each point is

`HitTest` returns a `HitArea` for a point, in points from the top left of the
window:

| HitArea | The window |
|---|---|
| `Content` | Treats the point as ordinary content: the view gets the mouse as usual |
| `Caption` | Moves the window when dragged, maximizes it on a double click, shows the window menu on a right click |
| `MinimizeButton` | Minimizes the window when clicked |
| `MaximizeButton` | Maximizes or restores it when clicked; on Windows 11, shows the snap layouts on hover |
| `CloseButton` | Asks to close it when clicked, through `OnCloseRequested` |

The window acts on the buttons itself when the mouse is pressed and released on
the same button. The view still receives `MouseMoved`, `MouseButtonPressed`,
and `MouseButtonReleased` over its buttons, so it can draw them hovered and
pressed. Moving off a button while holding it cancels the click, as with the
system's buttons.

Points marked `Content` inside the title bar, such as a search box or tabs,
behave like the rest of the content.

The top few pixels of the window resize it, as the system's title bar would,
unless the window is maximized or not resizable.

## Writing a title bar view

A title bar view is a [`View`](../core/events-and-views.md#views) that reports
its height and its areas. This one leaves drawing aside and has a caption and
three buttons at the right, each 46 points wide:

```cpp
class PlainTitleBar final : public View
{
public:
    void Attach(Host&) override {}
    void Detach() override {}
    void Place(easyforge::Rectangle area) override { Area = area; }
    void HandleEvent(Event&) override {}
    void Frame(float) override {}   // draw here

    Vector2 PreferredSize(Vector2 available) const override { return { available.X, 40 }; }

    HitArea HitTest(Vector2 point) const override
    {
        float fromRight = Area.Width - point.X;
        if (fromRight < 46) return HitArea::CloseButton;
        if (fromRight < 92) return HitArea::MaximizeButton;
        if (fromRight < 138) return HitArea::MinimizeButton;
        return HitArea::Caption;
    }

private:
    easyforge::Rectangle Area;
};

window.TitleBar = std::make_shared<PlainTitleBar>();
```

To know whether to draw the maximize button as "restore", ask the host in
`Attach` for its `Mode()`, which is `WindowMode::Maximized` while the window is
maximized.

## Limitations

- Windows only for now. On macOS the window will keep its red, yellow, and
  green buttons where people expect them and leave room for them; on Linux,
  Wayland and X11 each have their own rules. On phones and the web there is no
  window frame, so a title bar view is never shown.
- On Windows 10 the thin line the system draws along the top of a window is
  gone with the system's title bar. Windows 11 draws its border and rounded
  corners as usual.
