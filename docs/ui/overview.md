# ui

`ui` builds interfaces: rows and columns of labels, buttons, fields, and
pictures, custom title bars, screens that switch with transitions, themes,
effects, and shaders of your own. You write the interface as a tree of elements
and assign it to a window; changing an element later changes what is on screen.

```cmake
target_link_libraries(my_program PRIVATE easyforge::window easyforge::ui)
```

```cpp
#include <easyforge/ui.h>
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "Notes", .Icon = "icon.png", .Width = 960, .Height = 600 });

    ui::Label status("Ready");
    window.Content = ui::Column({
        .Padding = 24,
        .Gap = 12,
        .Children = {
            ui::Label("Hello", { .FontSize = 32 }),
            ui::Button("Save", { .Style = ui::ButtonStyle::Accent, .OnClick = [status] { status.Text = "Saved"; } }),
            status,
        },
    });

    window.Run();
}
```

`ui` depends on `core`, `assets`, `data`, and `graphics`. It does not depend on
`window`: a window shows anything that implements `View` from `core`, and every
element converts to one. Without a window, a [root](without-a-window.md) takes
events and draws into any canvas.

## Pages

- [Layout](layout.md): rows, columns, stacks, grids, scrolling, sizes, padding,
  margins, and alignment.
- [Elements](elements.md): labels, images, buttons, checkboxes, toggles,
  sliders, progress bars, text fields, text areas, drawing areas, and 3D scenes.
- [Menus, dropdowns, and dialogs](menus-and-dialogs.md): lists that open over
  the interface, and boxes that take the keyboard until they close.
- [Lists and trees](lists-and-trees.md): rows to choose from, and the nodes of a
  data table.
- [Title bars](title-bars.md): drawing a window's title bar yourself, with the
  window's own buttons and snap layouts.
- [Displays](displays.md): switching between screens with transitions, going
  back, and tabs.
- [Effects and shaders](effects-and-shaders.md): backgrounds, shadows, glows,
  outlines, gradients, frosted glass, color changes, masks, and your own
  shaders.
- [Themes](themes.md): colors, fonts, and shapes, light and dark, switched at
  once or gradually, and loaded from files.
- [Properties and animation](properties-and-animation.md): changing elements
  while the program runs, finding them by name, and animating them.
- [Events and focus](events-and-focus.md): clicks, the keyboard, focus,
  tooltips, drag and drop, and which events reach the rest of your program.
- [Live data](live-data.md): text that follows a value in a `data` table.
- [Without a window](without-a-window.md): using `ui` with your own window or
  engine.

## How it fits together

Each window has one **root**, which holds its theme, the element that has the
keyboard, and the renderer the interface draws with. Everything assigned to a
window's `Content` and `TitleBar` shares that root:

```cpp
ui::Root root = ui::Root::Of(window);
root.Theme = ui::Theme::Dark();
ui::Button save = root.Find<ui::Button>("Save");
```

The root draws once the window's frame is done: after `OnFrame`, so changes made
there show in the same frame. It only lays elements out again in frames where
something changed.

Elements are stored as rows of a [data table](../data/overview.md): each element
is a node whose type is its kind, such as `Button`, and each setting you chose
is a property of the node. `root.Data()` is that table, so tools and scripts can
read the whole interface and follow every change to it.

## Limitations

- Drawing needs the `graphics` library, so `ui` runs where `graphics` does:
  Windows, for now.
- The interface takes the window's renderer. A window that shows `ui` cannot
  also have a `Renderer` of your own; draw inside `ui::DrawingArea` or show a
  scene with `ui::SceneView` instead.
- Text is laid out with kerning but not shaped: scripts that need shaping, such
  as Arabic, and color emoji do not display correctly yet.
- Menus, dropdowns, and dialogs are drawn inside the window; there are no system
  menus or file dialogs.
