# graphics

`graphics` draws 2D and 3D into windows and into images in memory. A `Canvas`
draws rectangles, circles, lines, images, and text; a `Scene` holds 3D models, a
camera, and a sun; and shaders written in the easyforge shader language change
how anything looks, pixel by pixel. Textures, fonts, and models load anywhere, before any
renderer exists, and are sent to the GPU the first time something draws them.

```cmake
target_link_libraries(my_program PRIVATE easyforge::window easyforge::graphics)
```

```cpp
#include <easyforge/graphics.h>
#include <easyforge/window.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "Drawing", .Width = 800, .Height = 600 });
    Renderer renderer = Renderer::New(window);
    if (!renderer)
    {
        Log(LogLevel::Error, renderer.Error());
        return 1;
    }

    Texture logo = Texture::Load("logo.png");
    Font font = Font::Load("Inter.ttf");

    window.OnFrame = [&](float deltaSeconds) {
        Canvas canvas = renderer.BeginFrame(Color::Hex("#1A1A20"));
        canvas.Rectangle({ .Position = { 40, 40 }, .Size = { 200, 80 }, .Color = Color::Hex("#2E5AAC"), .CornerRadius = 12 });
        canvas.Image(logo, { .Position = { 280, 40 } });
        canvas.Text(font, "Hello", { .Position = { 40, 160 }, .Size = 32 });
        renderer.EndFrame();
    };

    window.Run();
}
```

`graphics` depends on `core` and `assets`. It draws with Direct3D 12 on Windows;
Vulkan, WebGPU, and Metal arrive with the later platforms, behind the same
interface.

## Renderers

A renderer draws into a window, or into an image of a size you choose:

```cpp
Renderer onScreen = Renderer::New(window);
Renderer inMemory = Renderer::New({ .Width = 512, .Height = 512 });
```

| Written | Result |
|---|---|
| `renderer.BeginFrame(clear)` | Starts a frame cleared to a color, and gives the `Canvas` to draw it with |
| `renderer.EndFrame()` | Sends the frame to the GPU and shows it |
| `renderer.Capture()` | The last finished frame as an `ImageData`, read back from the GPU |
| `renderer.Size()`, `PixelSize()`, `Scale()` | The size in points and pixels, and pixels per point |
| `renderer.Description()` | The API and GPU, such as `"Direct3D 12 on Intel(R) HD Graphics 620"` |
| `if (!renderer)`, `renderer.Error()` | Whether making it failed, and why |

`RendererSettings` holds the rest:

| Setting | Default | Meaning |
|---|---|---|
| `Adapter` | `HighPerformance` | Which GPU: `HighPerformance`, `LowPower`, or `Software` |
| `Width`, `Height` | 0 | For a renderer without a window: the image's size in pixels |
| `Scale` | 1 | For a renderer without a window: pixels per point |

`GraphicsAdapter::Software` draws on the processor with Windows' own software
renderer. It is slow, and it draws exactly the same pixels on every computer,
which is what tests that compare pictures want. easyforge's own tests use it.

A window has one renderer at a time; asking for a second fails with an error
that says so. Renderers share one GPU device for each adapter, so a texture
drawn in two windows is only sent to the GPU once.

A window's renderer follows the window: each frame is the window's current size,
in its current scaling, and waits for the screen when the window's
`VerticalSync` is on. A window made with `Transparent` gets a renderer that
draws with alpha, and what is drawn with alpha below 1 shows what is behind the
window.

## What there is

- [Drawing with a canvas](canvas.md): shapes, images, text, clipping, and
  transforms.
- [Textures and fonts](textures-and-fonts.md): loading, updating, and how text
  is laid out.
- [Scenes](scenes.md): 3D models, the camera, and the sun.
- [Shaders](shaders.md): the easyforge shader language, and layers drawn
  through shaders.

## Colors

Colors are sRGB, as in `Color::Hex`, and 2D drawing blends them the way web
browsers do, so a design picked in a design tool looks the same. 3D lighting is
worked out in linear light, which is what makes it look physically right, and
the result goes back to sRGB.

## Limitations

- Direct3D 12 only for now, on Windows 10 with a GPU that supports feature level
  11.0; other computers fall back to the software renderer.
- The explicit GPU layer underneath the renderer is not public yet. It will be,
  as `GraphicsDevice`, once Vulkan has proven its shape.
- Drawing belongs to the thread that made the renderer. Textures, fonts, and
  models can be loaded on any thread, since loading does not touch the GPU.
