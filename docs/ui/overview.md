# ui

`ui` will build interfaces: layout, buttons, text fields, lists, dialogs, themes,
effects, custom shaders, and displays that the program switches between.

> [!NOTE] Not available yet
> `ui` is step 7 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- Elements written directly in a tree: `ui::Column({ .Gap = 12, .Children = { ui::Label("Hello"), ui::Button("Save") } })`.
- `window.Content = ...` and `window.TitleBar = ...` to fill a window and give it
  a custom title bar.
- Rows, columns, grids, layers, and scrolling, with sizes in pixels, as a
  percentage of the parent, or filling the space left.
- Shadows, background blur, glows, outlines, gradients, images that stretch
  without distorting their corners, and custom shaders on any element.
- Themes that follow the system's light or dark setting, and can be switched
  and animated.
- `ui::Displays` for switching between screens, with fades and slides.
- Labels that show live values from a `data` table and update by themselves.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#window-and-ui-the-usage).
