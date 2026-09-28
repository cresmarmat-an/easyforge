# Textures and fonts

Textures and fonts are ready to draw. They are loaded without a renderer and
sent to the GPU the first time something draws them, so they can be loaded
anywhere, including on a background thread.

## Textures

```cpp
Texture logo = Texture::Load("logo.png");
Texture tiles = Texture::Load("tiles.png", { .Smooth = false });

ImageData generated(64, 64);
// ... fill it ...
Texture made = Texture::FromImage(generated);
```

`Texture::Load` reads any image [assets](../assets/images.md) reads, through
[Files](../assets/files-and-packs.md). A texture that could not be loaded tests
as false, and `Error()` says why; drawing it does nothing.

| TextureSettings | Default | Meaning |
|---|---|---|
| `Smooth` | true | Blends between pixels when drawn larger or smaller. Turn it off for pixel art |
| `Repeat` | false | Repeats the image outside its edges, for tiling textures on 3D models |

`texture.Width()` and `texture.Height()` give the size in pixels.

### Changing a texture

```cpp
ImageData frame = camera.NextFrame();
video.Update(frame);
```

`Update` replaces the pixels, and the size if it changes. Everything that draws
the texture shows the new pixels from the next frame. A texture is sent to the
GPU again only after it changes.

## Fonts

```cpp
Font font = Font::Load("Inter.ttf");
canvas.Text(font, "Hello", { .Position = { 20, 20 }, .Size = 24 });
```

`Font::Load` reads TrueType fonts and collections through
[FontData](../assets/fonts.md); `Font::FromData` uses one already loaded.

### Measuring text

```cpp
Vector2 size = font.Measure("Game over", 48);
canvas.Text(font, "Game over", { .Position = { (width - size.X) / 2, 100 }, .Size = 48 });

float line = font.LineHeight(16);
```

`Measure` gives the width of the widest line and the height of all the lines.
`LineHeight` is the distance from one line's top to the next, from the font's
own ascender, descender, and line gap.

### How text is laid out

Text is UTF-8. Each character is looked up in the font, and pairs of letters are
moved closer or apart by the font's kerning. Lines break at `"\n"`, and a tab is
as wide as four spaces. Glyphs sit on whole pixels, which keeps small text sharp.
A character the font does not have is drawn as the font's "missing character"
glyph, usually a box.

### How glyphs are drawn

Each glyph is drawn once for each size, by the anti-aliased rasterizer in
`assets`, and kept on the GPU in a large texture, the glyph atlas, that every
font and size share. Sizes within a quarter of a pixel of each other share
their glyphs. White text on a black background shows exactly the coverage the
rasterizer computed, which easyforge's tests check pixel by pixel.

## Limitations

- Textures have no mipmaps yet, so a texture drawn much smaller than its size can
  shimmer.
- Text has no shaping beyond kerning: no ligatures, no right-to-left scripts,
  and no joined scripts such as Arabic or Devanagari.
- No fallback to another font for characters a font lacks.
- No automatic line wrapping at a width; `ui::Label` adds it.
- No color glyphs such as emoji, and no fonts with CFF outlines.
