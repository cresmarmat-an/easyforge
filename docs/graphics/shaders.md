# Shaders

A shader decides the color of every pixel in an area. easyforge shaders are
written in the easyforge shader language, which reads like the script language,
and they run on every backend easyforge draws with.

```
-- ripple.shader
value Speed: number = 1
value Tint: color = #FFFFFF

function Pixel(input: PixelInput) returns color then
    constant wave = Sine(input.Position.X * 20 + input.Time * Speed)
    return Sample(input.Content, input.Coordinates + vector2(0, wave * 0.01)) * Tint
end
```

```cpp
Shader ripple = Shader::Load("ripple.shader");
if (!ripple)
{
    Log(LogLevel::Error, ripple.Error());
}

canvas.BeginLayer({ 20, 20, 300, 200 });
// ... draw the content ...
canvas.EndLayer({ .Shader = ripple, .Values = { { "Speed", 2.0f }, { "Tint", Color::Hex("#66CCFF") } } });
```

## Loading

`Shader::Load(path)` reads a `.shader` file through
[Files](../assets/files-and-packs.md), and `Shader::FromText(source, name)` reads
source you already have. Either way the whole shader is checked at once: a
shader with problems tests as false, and `Error()` lists every problem on its
own line, with the file, line, and column:

```
ripple.shader:6:22: 'wave' is not declared
ripple.shader:7:12: this function returns a color, but this is a vector2
```

The shader is turned into the GPU's own language the first time it is drawn.
[easyforge_add_shaders](../installation/shaders.md) checks shaders when the
program is built, so problems show up before the program runs.

## Drawing with a shader

There are two ways:

```cpp
// Over an area. input.Content is empty.
canvas.Shaded(sky, { 0, 0, 800, 600 }, { { "Top", Color::Hex("#1E3C72") } });

// Over what was drawn: everything between BeginLayer and EndLayer is drawn into
// a picture of the area, and the shader reads it as input.Content.
canvas.BeginLayer({ 20, 20, 300, 200 });
canvas.Image(photo, { .Position = { 20, 20 } });
canvas.EndLayer({ .Shader = ripple });
```

`EndLayer` without a shader puts the picture back as it is, and
`.Opacity` fades it; that is how a group of shapes fades together without the
overlaps showing. See [layers](canvas.md#layers).

## The Pixel function

Every shader has one `Pixel` function, which takes a `PixelInput` and returns
the pixel's color:

| PixelInput | Type | Meaning |
|---|---|---|
| `Position` | vector2 | Where the pixel is, in points from the top left of the area |
| `Size` | vector2 | The area's size in points |
| `Coordinates` | vector2 | The same position from 0 to 1 across the area |
| `Time` | number | Seconds since the renderer was made, for animation |
| `Content` | texture | What was drawn in the layer, read with `Sample` |

The color returned is a plain color, not multiplied by its alpha; easyforge
takes care of blending it with what is underneath.

## Values

```
value Speed: number = 1
value Tint: color = #FFFFFF
value Center: vector2 = vector2(0.5, 0.5)
value Enabled: boolean = true
```

A value is set by the program each time the shader is drawn, through
`ShaderValue`s: `{ "Speed", 2.0f }`, `{ "Tint", Color::Hex("#66CCFF") }`,
`{ "Center", Vector2 { 0.3f, 0.7f } }`, `{ "Enabled", false }`. A value that is not
given keeps its starting value, which has to be written out: a number, a color
code, `true` or `false`, or a vector of numbers. `shader.ValueNames()` lists
them.

## The language

The shader language is the [script language](../script/overview.md)'s syntax
with the parts that make sense on a GPU:

- `function Name(parameter: type) returns type then ... end`, where every
  parameter needs a type. Functions can call each other, but not themselves.
- `variable` and `constant`, which need a starting value; the type is worked out
  from it, or written as `variable total: number = 0`.
- `constant` at the top of the shader, for numbers used everywhere.
- `if ... then ... else if ... then ... else ... end`, `while ... then ... end`,
  `for index in 1 to 10 then ... end` (both ends included), `break`, `continue`,
  and `return`.
- `--` comments and `--[[ ... ]]` comments.

### Types

| Type | Written | Parts |
|---|---|---|
| `number` | `1`, `0.5`, `2e3` | |
| `boolean` | `true`, `false` | |
| `vector2`, `vector3`, `vector4` | `vector2(1, 2)`, `vector3(position, 0)`, `vector4(0)` | `X`, `Y`, `Z`, `W` |
| `color` | `#FF8000`, `#FF800080`, `color(1, 0.5, 0)`, `color(1, 0.5, 0, 1)` | `Red`, `Green`, `Blue`, `Alpha` |

A vector or color is made from numbers and smaller vectors adding up to its
size, or from one number that fills every part. `color` with three numbers is
opaque. A `vector4` and a `color` turn into each other: `color(someVector4)`.
Parts can be read and assigned: `result.Alpha = 0.5`.

### Operators

`+`, `-`, `*`, and `/` work between two of the same type, part by part, and
between a vector or color and a number. `%` is the remainder and `^` raises to
a power. `<`, `<=`, `>`, `>=`, `==`, and `!=` compare numbers; `and`, `or`, and
`not` join true and false.

### Built-in functions

| Function | Does |
|---|---|
| `Sine`, `Cosine`, `Tangent`, `ArcSine`, `ArcCosine`, `ArcTangent` | Trigonometry, in radians |
| `ArcTangent2(y, x)` | The angle of a direction |
| `SquareRoot`, `Power`, `Exponential`, `Logarithm` | |
| `Absolute`, `Sign`, `Floor`, `Ceiling`, `Round`, `Fraction` | `Fraction` keeps what is after the point |
| `Min`, `Max`, `Clamp(value, low, high)` | |
| `Lerp(from, to, amount)` | Blends from one to the other |
| `Step(edge, value)`, `SmoothStep(low, high, value)` | 0 below, 1 above; smoothly between for `SmoothStep` |
| `Radians`, `Degrees` | Converts angles |
| `Length`, `Distance`, `Dot`, `Normalize`, `Cross` | For vectors; `Cross` needs vector3 |
| `Sample(input.Content, coordinates)` | The layer's color at coordinates from 0 to 1 |

The functions that work part by part take numbers, vectors, and colors alike:
`Sine(input.Coordinates)` is a vector2.

## Seeing what the GPU gets

`shader.GeneratedCode()` is the code the shader became, which helps when a
shader draws something unexpected. `easyforge-shader --code ripple.shader`
prints it too.

## Limitations

- Only pixel shaders, over rectangular areas. Shaders for 3D models arrive with
  the rest of 3D.
- One texture, the layer's content. Shaders cannot sample other textures yet.
- No lists, text, or tables, and functions cannot call themselves, since GPUs
  have neither.
- Numbers are 32-bit floating point, and loops should stay short: a pixel
  shader runs for every pixel of the area, every frame.
