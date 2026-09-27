# App icons

The icon Explorer, the taskbar, and shortcuts show for a program is part of the
program file, so it is set when the program is built. `easyforge_app_icon` makes
it from one image:

```cmake
add_executable(notes main.cpp)
target_link_libraries(notes PRIVATE easyforge::window)
easyforge_app_icon(notes icon.png)
```

The function is available once easyforge is added to your build, whether it was
fetched with `FetchContent` or found with `find_package`.

## The image

Any format the [assets](../assets/images.md) library reads works. Use a square
image of at least 256 by 256 pixels with a transparent background. An image that
is not square is fitted and centered. A relative path is relative to the folder
of the `CMakeLists.txt` that calls the function.

## What it makes

| Platform | Result |
|---|---|
| Windows | An `.ico` file with the image at 16, 20, 24, 32, 40, 48, 64, 96, 128, and 256 pixels, each scaled from the original, built into the program as its first icon |
| Other platforms | Nothing yet; the icon files for macOS, iOS, Android, and the web arrive with those platforms |

The icon is made by `easyforge-icon`, a small program built from easyforge's
`tools` folder. When easyforge is fetched, it is only compiled when a program
uses `easyforge_app_icon`. It can also be run by hand:

```
easyforge-icon icon.png notes.ico
```

A window whose `Icon` setting is empty shows the program's icon, so with
`easyforge_app_icon` most programs do not need the `Icon` setting at all. See
[icons and cursors](../window/icons-and-cursors.md).

## Limitations

- One icon per program.
- The `.ico` stores every size as an uncompressed bitmap, which makes the
  program file about 400 KB larger.
