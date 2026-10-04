# Drawing with a canvas

A `Canvas` draws 2D shapes, images, text, and 3D scenes into a frame. It comes
from `Renderer::BeginFrame` and works until `EndFrame`.

```cpp
Canvas canvas = renderer.BeginFrame(Color::Hex("#1A1A20"));

canvas.Rectangle({ .Position = { 20, 20 }, .Size = { 200, 80 }, .Color = Color::Hex("#2E5AAC"), .CornerRadius = 12 });
canvas.Circle({ 300, 60 }, 40, { .Color = Color::Hex("#3BB273") });
canvas.Line({ 360, 20 }, { 500, 100 }, { .Color = Color::White, .Width = 3 });
canvas.Image(logo, { .Position = { 20, 120 }, .Size = { 96, 96 } });
canvas.Text(font, "Hello", { .Position = { 140, 140 }, .Size = 32 });

renderer.EndFrame();
```

Positions and sizes are in points from the top left, with Y growing downward.
Things drawn later cover things drawn earlier. Every edge is anti-aliased, and
a shape whose edge falls exactly on a pixel boundary fills whole pixels, so
rectangles at whole-point positions come out crisp.

## Shapes

### Rectangles

| RectangleStyle | Default | Meaning |
|---|---|---|
| `Position` | 0, 0 | The top left corner |
| `Size` | 0, 0 | Width and height; nothing is drawn if either is zero |
| `Color` | white | The fill |
| `CornerRadius` | 0 | Rounds all four corners; it is limited to half the shorter side, so a large radius makes a pill |
| `BorderWidth` | 0 | A border inside the edge |
| `BorderColor` | transparent | The border's color |
| `Gradient` | none | A `LinearGradient` to fill with instead of `Color` |
| `Blur` | 0 | Softens the edge over this many points, half inside and half outside |

```cpp
canvas.Rectangle({ .Position = { 240, 20 }, .Size = { 200, 80 }, .Color = Color::White, .CornerRadius = 40,
    .BorderWidth = 4, .BorderColor = Color::Hex("#E8553B") });
```

### Gradients

```cpp
canvas.Rectangle({ .Position = { 20, 20 }, .Size = { 300, 120 }, .CornerRadius = 12,
    .Gradient = LinearGradient { .From = Color::Hex("#243B55"), .To = Color::Hex("#141E30"), .Angle = 90 } });
```

A `LinearGradient` goes from `From` to `To` along `Angle` degrees: 0 from left
to right, 90 from top to bottom, 180 from right to left. The two colors sit on
the shape's farthest corners, so the whole shape is covered from the first
color to the last, whatever its size. Colors are mixed with their alpha
applied, so a gradient to a transparent color fades without a dark fringe.

### Soft edges, shadows, and glows

```cpp
// A shadow: the card's box, moved down, darkened, and softened.
canvas.Rectangle({ .Position = { 40, 46 }, .Size = { 240, 140 }, .Color = Color::Black.WithAlpha(0.35f),
    .CornerRadius = 12, .Blur = 18 });
canvas.Rectangle({ .Position = { 40, 40 }, .Size = { 240, 140 }, .Color = Color::White, .CornerRadius = 12 });
```

`Blur` fades the edge the way a Gaussian blur of the shape would: half of the
shape's color on the edge itself, fading to almost nothing `Blur` points
outside, and reaching almost full color as far inside. It is worked out from the
distance to the outline, so it costs no more than a sharp shape. For small
shapes with a large blur it is an approximation, a little stronger than a true
blur would be.

```cpp
// A shadow that only shows outside a see-through card.
canvas.Rectangle({ .Position = { 40, 46 }, .Size = { 240, 140 }, .Color = Color::Black.WithAlpha(0.35f),
    .CornerRadius = 12, .Blur = 18, .Hole = Rectangle { 40, 40, 240, 140 }, .HoleCornerRadius = 12 });
```

`Hole` leaves a rounded rectangle undrawn, in points like the shape itself. A
shadow under a box that is partly see-through would otherwise show through the
box; with the box as its hole, it only shows around it. A hole is not used
together with a gradient.

### Circles

`canvas.Circle(center, radius, style)`, where `CircleStyle` has `Color`,
`BorderWidth`, `BorderColor`, `Gradient`, and `Blur`.

### Lines

`canvas.Line(from, to, style)`, where `LineStyle` has `Color` and `Width` in
points. The line is centered on the points it joins, and its ends are cut
square at the points.

## Images

```cpp
canvas.Image(logo, { .Position = { 20, 120 } });                             // one point per pixel
canvas.Image(logo, { .Position = { 20, 120 }, .Size = { 48, 48 } });         // scaled
canvas.Image(sheet, { .Position = { 20, 200 }, .Source = { 32, 0, 32, 32 } }); // part of it
```

| ImageStyle | Default | Meaning |
|---|---|---|
| `Position` | 0, 0 | The top left corner |
| `Size` | 0, 0 | The size to draw at; zero draws one point for each pixel of the part shown |
| `Source` | empty | The part of the texture to draw, in its pixels; empty is all of it |
| `Tint` | white | Multiplies every pixel; lower its alpha to fade the image |
| `CornerRadius` | 0 | Rounds the image's corners |
| `Slice` | 0 | Keeps this many pixels at each edge from stretching; see below |

Whether an image is smoothed when scaled is a setting of the
[texture](textures-and-fonts.md#textures).

### Images that stretch without distorting

```cpp
canvas.Image(frame, { .Position = { 20, 20 }, .Size = { 300, 120 }, .Slice = 12 });
```

With `Slice`, the image is cut into nine pieces 12 pixels from each edge. The
corners are drawn at one point per pixel, the top and bottom edges stretch only
sideways, the left and right edges only up and down, and the middle both ways.
A button or a frame drawn in a small image stays sharp at any size. The slice is
limited to half the image and half the drawn size. `CornerRadius` is not used
with a slice.

## Text

```cpp
canvas.Text(font, "Score: 1200\nLives: 3", { .Position = { 20, 20 }, .Size = 18, .Color = Color::White });
```

| TextStyle | Default | Meaning |
|---|---|---|
| `Position` | 0, 0 | The top left corner of the first line |
| `Size` | 16 | The font size in points: the height of an em |
| `Color` | white | |

Lines break at each `"\n"`. `font.Measure(text, size)` gives the size text will
take up, for centering it or making room for it. [Textures and
fonts](textures-and-fonts.md#fonts) has the details.

## Scenes

```cpp
canvas.Draw(scene);                          // fills the canvas
canvas.Draw(scene, { 400, 20, 360, 240 });   // an area
```

The scene is drawn through its camera into the area, with its own background,
and shapes drawn afterwards appear on top of it. See [scenes](scenes.md).

## Layers

```cpp
canvas.BeginLayer({ 20, 20, 300, 200 });
canvas.Rectangle({ .Position = { 20, 20 }, .Size = { 200, 100 }, .Color = Color::Hex("#2E5AAC") });
canvas.Circle({ 220, 120 }, 60, { .Color = Color::Hex("#3BB273") });
canvas.EndLayer({ .Opacity = 0.5f });
```

Everything drawn between `BeginLayer` and `EndLayer` goes into a picture of the
area, which `EndLayer` then puts on the canvas. With `.Opacity`, the group fades
as one, so where its shapes overlap there is no darker patch. With `.Shader`,
every pixel of the picture goes through a [shader](shaders.md), which reads it
as `input.Content`. Layers nest, and only what is inside the area is kept. An
area too large for the GPU to hold in one picture, such as a faded list many
screens long, is cut down to the part inside the clip.

| LayerStyle | Default | Meaning |
|---|---|---|
| `Shader` | none | Runs the picture through the shader |
| `Values` | none | The shader's values, such as `{ { "Speed", 2.0f } }` |
| `Opacity` | 1 | Fades the whole layer |

## Blurring what is behind

```cpp
canvas.Image(photo, { .Size = canvas.Size() });
canvas.BlurBehind({ 40, 40, 320, 200 }, 20, 12);    // area, radius, corner radius, and opacity, 1 if left out
canvas.Rectangle({ .Position = { 40, 40 }, .Size = { 320, 200 }, .Color = Color::White.WithAlpha(0.4f),
    .CornerRadius = 12 });
```

`BlurBehind` blurs what has been drawn under the area so far, like frosted
glass, and puts the blurred picture in its place, with rounded corners when
given a corner radius. What is drawn afterwards covers it as usual, so a
see-through rectangle on top tints the glass. The blur reaches `radius` points
and takes in what is just outside the area, so the edges blend with their
surroundings. An opacity below 1 mixes the blur with what was there, for glass
that fades in. Where the area passes the edge of the canvas, its corners stay
round.

To blur, the GPU has to finish drawing what is under the area first, so each
`BlurBehind` splits the frame's drawing in two. A few per frame are fine; one
for every item in a long list is not. Inside a layer, it blurs only what was
drawn into the layer.

## Shaders

`canvas.Shaded(shader, area, values)` runs a shader over an area, with nothing as
its content: for backgrounds, gradients, and patterns worked out per pixel.
[Shaders](shaders.md) describes the language.

## Clipping

```cpp
canvas.PushClip({ 20, 20, 300, 200 });
// ... only the inside of the area is drawn to
canvas.PopClip();
```

Clip areas nest: each is cut down to the one before it. They are axis-aligned
rectangles. `canvas.ClipArea()` is where drawing can still show, in points after
the transforms: the canvas cut down by every clip and by the layer being drawn.
Code that draws a lot can skip what lies outside it.

## Moving and scaling

```cpp
canvas.PushTransform({ 100, 50 }, 2.0f);
canvas.Rectangle({ .Size = { 10, 10 } });    // drawn at 100, 50, 20 points wide
canvas.PopTransform();
```

A point is multiplied by the scale, then moved by the offset. Transforms nest,
and apply to clip areas pushed inside them too.

## The size of the canvas

`canvas.Size()` is the canvas's size in points and `canvas.Scale()` the pixels
per point. A canvas from a window's renderer is the window's content.

## How it is drawn

Everything drawn on a canvas is kept until `EndFrame`, then sent to the GPU in
as few draws as possible: shapes in a row that use the same texture or glyph
page are drawn together. Each shape is measured by its distance from its
rounded outline on the GPU, which is how corners, borders, and edges share one
anti-aliased formula.

## Limitations

- Lines have square ends; there are no round or joined ends and no dashed lines.
- There are no paths, polygons, or curves beyond the rounded rectangle and circle.
- Gradients are linear, with two colors. For radial gradients or more colors, use
  a [shader](shaders.md).
- Images and text cannot be blurred on their own; draw them in a layer and blur
  behind a later area, or use a shader.
- Transforms move and scale; they do not rotate.
- A clip area is a rectangle, not a rounded shape.
