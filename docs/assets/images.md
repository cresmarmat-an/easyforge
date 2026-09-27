# Images

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

ImageData photo = ImageData::Load("photos/beach.jpg");
if (!photo)
{
    Log(LogLevel::Error, photo.Error());
    return;
}

Color corner = photo.ColorAt(0, 0);
photo.SetColorAt(0, 0, Color::Hex("#FF0000"));
```

## ImageData

Every image, whatever its file format, becomes the same thing in memory:

| Member | What it holds |
|---|---|
| `Width`, `Height` | Size in pixels |
| `Pixels` | `Width * Height * 4` bytes: red, green, blue, alpha for each pixel, rows from top to bottom |
| `Stride()` | Bytes from one row to the next, always `Width * 4` |
| `ColorAt(x, y)` | A pixel as a [Color](../core/colors.md). Outside the image it is transparent black |
| `SetColorAt(x, y, color)` | Writes a pixel. Outside the image it does nothing |

Colors are sRGB, as the file stored them. Alpha is straight, not premultiplied.

To make an image yourself:

```cpp
ImageData canvas(256, 256);                         // transparent black
ImageData fromPixels(2, 1, { 255, 0, 0, 255,  0, 0, 255, 255 });
```

`ImageData(width, height, pixels)` checks that `pixels` holds exactly
`width * height * 4` bytes; if not, the image is empty and `Error()` says why.

## Resizing

```cpp
ImageData thumbnail = photo.Resized(128, 96);
```

`Resized` returns a copy at the new size. Shrinking averages every pixel that
each new pixel covers, so fine detail turns smooth instead of speckled; growing
blends the four nearest pixels. Colors are blended in linear light, weighted by
alpha, so a transparent pixel's color never bleeds into its neighbors and dark
and light edges keep their brightness.

Resizing an empty image, or to a size of zero or larger than
`ImageData::MaximumSide`, gives an empty image with an error. Resizing to the
same size gives an exact copy.

## Recognizing the format

The format is recognized from the first bytes of the file, not from its name, so
a PNG saved as `picture.jpg` still loads. TGA files have no such signature and are
recognized by their `.tga` extension only; when decoding bytes, pass a name ending
in `.tga`.

## Formats

### PNG

Reads every PNG: grayscale, grayscale with alpha, truecolor, truecolor with alpha,
and palette images, at every bit depth (1, 2, 4, 8, and 16 bits), interlaced or
not. Transparency from a `tRNS` chunk works for all of them. Every chunk's CRC is
checked, so a damaged file is reported instead of drawn wrong.

Not done:

- 16-bit channels are reduced to 8 bits by keeping the high byte.
- Gamma (`gAMA`), color profiles (`iCCP`, `cHRM`, `sRGB`) and text chunks are
  ignored; pixels are taken as sRGB.
- Animated PNGs load their first frame.

### JPEG

Reads baseline and extended sequential JPEG with 8-bit samples: grayscale or
color, any chroma subsampling with a whole-number ratio (4:4:4, 4:2:2, 4:2:0,
and others), restart markers, and the Adobe marker that says whether color is
stored as RGB. Subsampled color is upsampled smoothly, the way most decoders do,
so results match other decoders to within a step or two of 255.

Not done:

- Progressive JPEG gives an error. It arrives with 3D in stage 6.
- CMYK JPEG, 12-bit JPEG, arithmetic coding, and lossless JPEG give an error.
- The EXIF orientation flag is ignored, so a photo a camera stored sideways loads
  sideways.

### BMP

Reads Windows bitmaps with 1, 2, 4, and 8-bit palettes, 16, 24, and 32 bits per
pixel, custom bit masks (including 5-6-5 and alpha masks), every header version,
and bottom-up or top-down rows.

A 32-bit image whose alpha mask is present but whose alpha is zero everywhere is
treated as opaque, because many programs write BMPs that way.

Not done: run-length compressed BMPs (RLE4 and RLE8) give an error.

### TGA

Reads truecolor (15, 16, 24, and 32-bit), grayscale (8-bit), and color-mapped
TGA, with or without run-length compression, in any of the four corner origins.

### QOI

Reads every QOI image, with 3 or 4 channels.

## Loading in the background

```cpp
Pending<ImageData> pending = ImageData::LoadInBackground("textures/stone.png");
```

See [the overview](overview.md#loading-in-the-background).
