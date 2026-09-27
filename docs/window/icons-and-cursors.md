# Icons and cursors

## The window's icon

```cpp
Window window = Window::New({ .Title = "Notes", .Icon = "icon.png" });
```

`Icon` names an image file, read through [Files](../assets/files-and-packs.md)
like any other, in any format `assets` reads. The window scales it to the sizes
its title bar and the taskbar use, and again when the window moves to a screen
with different scaling. A square image of at least 256 by 256 pixels looks best;
an image that is not square is fitted and centered.

The icon can be changed at any time, from a file or from an image in memory:

```cpp
window.Icon = "icons/unsaved.png";

ImageData badge = ImageData::Load("icon.png");
badge.SetColorAt(0, 0, Color::Hex("#FF4040"));
window.SetIcon(badge);
```

If the file cannot be read, the window keeps its current icon and logs a
warning.

## The program's icon

The icon Explorer shows for the program file, and the one on shortcuts, is part
of the program itself and is set when it is built.
[`easyforge_app_icon`](../installation/app-icons.md) does that from the same
image:

```cmake
easyforge_app_icon(notes icon.png)
```

A window without an `Icon` of its own uses the program's icon. So with
`easyforge_app_icon`, the `Icon` setting is only needed to show something else
while the program runs.

## Cursors

```cpp
window.Cursor = Cursor::Hand;
```

| Cursor | Looks like |
|---|---|
| `Arrow` | The usual pointer |
| `Text` | The I-beam for text |
| `Hand` | A pointing hand, for links |
| `Crosshair` | A cross, for precise picking |
| `Move` | Arrows in four directions |
| `ResizeHorizontal`, `ResizeVertical` | A double arrow across or up and down |
| `ResizeDiagonal`, `ResizeAntiDiagonal` | A double arrow from top left to bottom right, or top right to bottom left |
| `NotAllowed` | A circle with a line through it |
| `Wait` | Busy |
| `Progress` | Busy, but still accepting input |
| `Hidden` | Nothing |

The cursor applies over the window's content. Over the frame and the edges, the
system's own resize cursors show as usual.

A cursor can also be an image, with a hot spot in pixels of the image that
marks where it clicks:

```cpp
ImageData pointer = ImageData::Load("cursor.png");
window.SetCursorImage(pointer, { 2, 2 });
```

The image is used at its own size in pixels. Assigning `window.Cursor` again goes
back to a standard cursor.

## Limitations

- The icon Explorer shows for the program file only comes from
  `easyforge_app_icon`; it cannot be changed while the program runs.
- Image cursors are not scaled for the screen, so provide one sized for the
  screens you expect.
- Animated cursors are not supported.
