# Effects and shaders

```cpp
ui::Panel({
    .Width = 320,
    .Padding = 16,
    .CornerRadius = 12,
    .Background = ui::Background::Image("paper.png", { .Slice = 12 }),
    .Effects = {
        ui::Shadow { .Offset = { 0, 6 }, .Blur = 18, .Color = Color::Black.WithAlpha(0.35f) },
        ui::BackgroundBlur { .Radius = 20 },
    },
    .Children = { /* ... */ },
})
```

Example 04 in [easyforge-examples](https://github.com/cresmarmat-an/easyforge-examples)
shows every effect on its own card.

## Backgrounds and borders

Every element can have a box behind its content:

| Setting | Meaning |
|---|---|
| `Background` | What fills the box, below |
| `CornerRadius` | Rounds the box's corners, in points |
| `BorderWidth` | A border inside the box's edge |
| `BorderColor` | The border's color; the theme's border color without one |

| Background | Fills the box with |
|---|---|
| `Color::Hex("#15151A")` | A color |
| `ui::Background::Gradient(from, to, angle)` | Colors changing along `angle` degrees: 0 from left to right, 90 from top to bottom |
| `ui::Background::Image("paper.png", settings)` | A picture from a file, or `Image(texture, settings)` |
| `ui::Background()` | Nothing, so what is behind shows through |

An image background takes `ImageBackgroundSettings`: `Fit` (`Stretch` unless
set; see [images](elements.md#images)), `Slice`, and `Tint`. With `Slice`, the
corners of the picture keep their size and only the edges and middle stretch,
so a frame drawn in a small image stays sharp at any size.

Buttons, panels, text fields, and title bars have a box from the theme unless
given their own; other elements have none. An element with a visible
background takes the clicks on it, so they do not reach what is behind it.

## Effects

`Effects` is a list of any of these; each is drawn at its own place, as listed:

| Effect | Draws | Settings |
|---|---|---|
| `ui::Shadow` | A soft shadow under the box | `Offset` (0, 4), `Blur` (12), `Spread` (0), `Color` (black at 30%) |
| `ui::Glow` | Light around the box | `Blur` (16), `Spread` (0), `Color` (the theme's accent) |
| `ui::BackgroundBlur` | Blurs whatever is behind the element, like frosted glass | `Radius` (20) |
| `ui::Gradient` | Colors changing across the element, over its background | `From` (white at 20%), `To` (transparent), `Angle` (90) |
| `ui::Outline` | A line around the box, outside it | `Width` (2), `Gap` (0), `Color` (the theme's focus color) |
| `ui::ColorAdjust` | Changes the colors of the element and everything in it | `Brightness` (0), `Contrast` (1), `Saturation` (1), `Hue` (0 degrees) |
| `ui::Mask` | Shows the element and everything in it only inside a rounded box | `CornerRadius` (0), `Feather` (0) |

Shadows, glows, and outlines take the shape of the box, rounded corners
included. For frosted glass, give the element a background that lets some of
what is behind through, such as `Color::White.WithAlpha(0.15f)`: the blur is
drawn first and the background over it.

`ColorAdjust` with `.Saturation = 0` turns everything gray; `.Brightness = -0.3f`
darkens; `.Hue = 180` turns every color to its opposite. `Mask` cuts children to
the box's rounded shape too, and `Feather` softens the edge over that many
points.

## Your own shaders

```cpp
ui::Panel({
    .Shader = Shader::Load("ripple.shader"),
    .ShaderValues = { { "Speed", 2.0f }, { "Tint", Color::Hex("#66CCFF") } },
    .Children = { /* ... */ },
})
```

With a `Shader`, the element and everything in it are drawn into a picture
first, and the shader decides each pixel, reading that picture as
`input.Content`. `ShaderValues` sets the shader's values each frame. The
picture covers the element's box and a margin around it for its effects, and
`input.Position` and `input.Size` describe that whole area. The shader language
is described in [shaders](../graphics/shaders.md);
[easyforge_add_shaders](../installation/shaders.md) checks shader files when the
program is built.

```
-- ripple.shader
value Speed: number = 1
value Tint: color = #FFFFFF

function Pixel(input: PixelInput) returns color then
    constant wave = Sine(input.Position.X * 0.08 + input.Time * Speed * 3)
    return Sample(input.Content, input.Coordinates + vector2(0, wave * 0.02)) * Tint
end
```

## Opacity, shaders, and pictures

An element with an `Opacity` below 1, a shader, `ColorAdjust`, or `Mask` is
drawn into a picture of its own first, then put on the screen. That is how a
faded panel fades as one, without its children showing through each other, and
it costs a little more than drawing directly. Everything else is drawn straight
to the screen.

## Limitations

- `BackgroundBlur` blurs what was drawn before the element in the same picture.
  Inside an element that is faded, shaded, adjusted, or masked, it sees only
  what is inside that element.
- A mask is a rounded box; masks in the shape of a picture need shaders that
  read two pictures, which the shader language does not have yet.
- Shadows and glows of small boxes with a large blur are an approximation, a
  little stronger than a true blur.
