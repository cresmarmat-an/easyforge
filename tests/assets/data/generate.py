"""Writes the test files for the assets library into this folder.

Uses only Python's standard library. Every image comes with a .rgba file of the
pixels it must decode to (width and height as two little-endian 32-bit numbers,
then red, green, blue, alpha per pixel), and every sound with a .f32 file of its
samples as 32-bit floats. The expected values are computed here, separately from
easyforge's decoders.

JPEG files are written here too; reference_jpeg.ps1 then decodes them with
Windows' own decoder to make their .rgba files, because JPEG decoders are allowed
to differ slightly.

    python generate.py
"""

import math
import struct
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
WIDTH, HEIGHT = 13, 11


def write(name, data):
    path = HERE / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(data))


def write_expected(name, width, height, pixels):
    write(name, struct.pack("<II", width, height) + bytes(pixels))


def pattern(x, y):
    """The color of pixel (x, y) in most test images."""
    return ((x * 37 + y * 11) & 255, (x * 13 + y * 29 + 7) & 255, (x * x + y * 3) & 255, (x * 23 + y * 41 + 100) & 255)


# PNG --------------------------------------------------------------------------

ADAM7 = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


def png_chunk(kind, data):
    body = kind + bytes(data)
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def paeth(left, above, above_left):
    estimate = left + above - above_left
    distances = (abs(estimate - left), abs(estimate - above), abs(estimate - above_left))
    if distances[0] <= distances[1] and distances[0] <= distances[2]:
        return left
    return above if distances[1] <= distances[2] else above_left


def png_filter(kind, row, previous, step):
    out = bytearray(len(row))
    for index, value in enumerate(row):
        left = row[index - step] if index >= step else 0
        above = previous[index]
        above_left = previous[index - step] if index >= step else 0
        predictor = [0, left, above, (left + above) >> 1, paeth(left, above, above_left)][kind]
        out[index] = (value - predictor) & 255
    return out


def png(name, color_type, depth, samples, palette=None, transparency=None, interlace=False, compression="dynamic",
        split=False):
    """Writes a PNG whose pixel (x, y) has the channel values samples(x, y)."""
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
    bits = channels * depth
    step = max(1, bits // 8)

    def pack(values):
        if depth == 8:
            return bytearray(values)
        if depth == 16:
            return bytearray(b"".join(struct.pack(">H", value) for value in values))
        out = bytearray((len(values) * depth + 7) // 8)
        for index, value in enumerate(values):
            bit = index * depth
            out[bit // 8] |= value << (8 - depth - bit % 8)
        return out

    raw = bytearray()
    filter_kind = 0
    for x0, y0, dx, dy in (ADAM7 if interlace else [(0, 0, 1, 1)]):
        pass_width = (WIDTH - x0 + dx - 1) // dx if WIDTH > x0 else 0
        pass_height = (HEIGHT - y0 + dy - 1) // dy if HEIGHT > y0 else 0
        if pass_width == 0 or pass_height == 0:
            continue
        previous = bytearray((pass_width * bits + 7) // 8)
        for pass_y in range(pass_height):
            values = []
            for pass_x in range(pass_width):
                values.extend(samples(x0 + pass_x * dx, y0 + pass_y * dy))
            row = pack(values)
            raw.append(filter_kind)
            raw += png_filter(filter_kind, row, previous, step)
            filter_kind = (filter_kind + 1) % 5
            previous = row

    if compression == "stored":
        compressed = zlib.compress(bytes(raw), 0)
    elif compression == "fixed":
        compressor = zlib.compressobj(9, zlib.DEFLATED, 15, 9, zlib.Z_FIXED)
        compressed = compressor.compress(bytes(raw)) + compressor.flush()
    else:
        compressed = zlib.compress(bytes(raw), 9)

    data = bytearray(b"\x89PNG\r\n\x1a\n")
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, depth, color_type, 0, 0, 1 if interlace else 0))
    data += png_chunk(b"tEXt", b"Comment\x00made by generate.py")
    if palette is not None:
        data += png_chunk(b"PLTE", b"".join(bytes(color) for color in palette))
    if transparency is not None:
        data += png_chunk(b"tRNS", transparency)
    pieces = [compressed[:len(compressed) // 2], compressed[len(compressed) // 2:]] if split else [compressed]
    for piece in pieces:
        data += png_chunk(b"IDAT", piece)
    data += png_chunk(b"IEND", b"")
    write(f"images/{name}.png", data)


def write_pngs():
    def scale(value, depth):
        return {1: 255, 2: 85, 4: 17, 8: 1}[depth] * value

    expected = [channel for y in range(HEIGHT) for x in range(WIDTH) for channel in pattern(x, y)]

    png("rgba8", 6, 8, pattern)
    write_expected("images/rgba8_png.rgba", WIDTH, HEIGHT, expected)
    png("rgba8_interlaced", 6, 8, pattern, interlace=True)
    write_expected("images/rgba8_interlaced_png.rgba", WIDTH, HEIGHT, expected)
    png("rgba8_stored", 6, 8, pattern, compression="stored", split=True)
    write_expected("images/rgba8_stored_png.rgba", WIDTH, HEIGHT, expected)
    png("rgba8_fixed", 6, 8, pattern, compression="fixed")
    write_expected("images/rgba8_fixed_png.rgba", WIDTH, HEIGHT, expected)

    # 16-bit channels whose high byte is the pattern; the low byte must be dropped.
    png("rgba16", 6, 16, lambda x, y: [(value << 8) | ((x * 7 + y) & 255) for value in pattern(x, y)], interlace=True)
    write_expected("images/rgba16_png.rgba", WIDTH, HEIGHT, expected)

    rgb = [value for y in range(HEIGHT) for x in range(WIDTH) for value in pattern(x, y)[:3] + (255,)]
    png("rgb8", 2, 8, lambda x, y: pattern(x, y)[:3])
    write_expected("images/rgb8_png.rgba", WIDTH, HEIGHT, rgb)
    png("rgb16", 2, 16, lambda x, y: [(value << 8) | 0x5A for value in pattern(x, y)[:3]])
    write_expected("images/rgb16_png.rgba", WIDTH, HEIGHT, rgb)

    # A color key: every pixel with the key color becomes transparent.
    def keyed(x, y):
        return (10, 20, 30) if (x + y) % 3 == 0 else pattern(x, y)[:3]
    png("rgb8_key", 2, 8, keyed, transparency=struct.pack(">HHH", 10, 20, 30))
    write_expected("images/rgb8_key_png.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in keyed(x, y) + ((0,) if (x + y) % 3 == 0 else (255,))])

    for depth in (1, 2, 4, 8, 16):
        top = (1 << depth) - 1

        def gray(x, y, depth=depth, top=top):
            return ((x * 5 + y * 3) % (top + 1),)
        png(f"gray{depth}", 0, depth, gray, interlace=depth in (1, 4))
        pixels = []
        for y in range(HEIGHT):
            for x in range(WIDTH):
                value = gray(x, y)[0]
                eight = value >> 8 if depth == 16 else scale(value, depth)
                pixels += [eight, eight, eight, 255]
        write_expected(f"images/gray{depth}_png.rgba", WIDTH, HEIGHT, pixels)

    png("gray_alpha8", 4, 8, lambda x, y: (pattern(x, y)[0], pattern(x, y)[3]))
    write_expected("images/gray_alpha8_png.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in (pattern(x, y)[0],) * 3 + (pattern(x, y)[3],)])
    png("gray_alpha16", 4, 16, lambda x, y: (pattern(x, y)[0] << 8 | 1, pattern(x, y)[3] << 8 | 2))
    write_expected("images/gray_alpha16_png.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in (pattern(x, y)[0],) * 3 + (pattern(x, y)[3],)])

    png("gray8_key", 0, 8, lambda x, y: ((x * 19 + y) & 255,), transparency=struct.pack(">H", 38))
    write_expected("images/gray8_key_png.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in ((x * 19 + y) & 255,) * 3 + ((0,) if (x * 19 + y) & 255 == 38 else (255,))])

    for depth in (1, 2, 4, 8):
        count = 1 << depth
        palette = [((index * 40) & 255, (255 - index * 20) & 255, (index * 7) & 255) for index in range(count)]
        alpha = bytes((index * 30) & 255 for index in range(min(count, 5)))

        def indexed(x, y, count=count):
            return ((x + y * 2) % count,)
        png(f"palette{depth}", 3, depth, indexed, palette=palette, transparency=alpha, interlace=depth == 4)
        pixels = []
        for y in range(HEIGHT):
            for x in range(WIDTH):
                index = indexed(x, y)[0]
                pixels += list(palette[index]) + [alpha[index] if index < len(alpha) else 255]
        write_expected(f"images/palette{depth}_png.rgba", WIDTH, HEIGHT, pixels)


# BMP, TGA, QOI -----------------------------------------------------------------

def bmp(name, bits, rows_top_down=False, header_size=40, compression=0, masks=None, palette=None, pixel=None):
    """pixel(x, y) gives the stored value: an index, a packed integer, or (b, g, r)."""
    row_bytes = ((WIDTH * bits + 31) // 32) * 4
    image = bytearray()
    for row in range(HEIGHT):
        y = row if rows_top_down else HEIGHT - 1 - row
        out = bytearray(row_bytes)
        for x in range(WIDTH):
            value = pixel(x, y)
            if bits < 8:
                bit = x * bits
                out[bit // 8] |= value << (8 - bits - bit % 8)
            elif bits == 8:
                out[x] = value
            elif bits == 24:
                out[x * 3:x * 3 + 3] = bytes(value)
            else:
                out[x * bits // 8:(x + 1) * bits // 8] = value.to_bytes(bits // 8, "little")
        image += out

    if header_size == 12:
        header = struct.pack("<IHhHH", 12, WIDTH, HEIGHT, 1, bits)
    else:
        header = struct.pack("<IiiHHIIiiII", header_size, WIDTH, -HEIGHT if rows_top_down else HEIGHT, 1, bits,
                             compression, len(image), 2835, 2835, len(palette or []), 0)
        extra = b""
        if masks is not None:
            extra = struct.pack("<" + "I" * len(masks), *masks)
        if header_size > 40:
            extra = extra.ljust(header_size - 40, b"\x00")
        header += extra
    palette_bytes = b""
    if palette is not None:
        entry = 3 if header_size == 12 else 4
        palette_bytes = b"".join(bytes((blue, green, red, 0)[:entry]) for red, green, blue in palette)
    offset = 14 + len(header) + palette_bytes.__len__()
    data = b"BM" + struct.pack("<IHHI", offset + len(image), 0, 0, offset) + header + palette_bytes + image
    write(f"images/{name}.bmp", data)


def write_bmps():
    opaque = [value for y in range(HEIGHT) for x in range(WIDTH) for value in pattern(x, y)[:3] + (255,)]
    straight = [value for y in range(HEIGHT) for x in range(WIDTH) for value in pattern(x, y)]

    bgr = lambda x, y: (pattern(x, y)[2], pattern(x, y)[1], pattern(x, y)[0])
    bmp("bgr24", 24, pixel=bgr)
    write_expected("images/bgr24_bmp.rgba", WIDTH, HEIGHT, opaque)
    bmp("bgr24_top_down", 24, rows_top_down=True, pixel=bgr)
    write_expected("images/bgr24_top_down_bmp.rgba", WIDTH, HEIGHT, opaque)
    bmp("bgr24_core", 24, header_size=12, pixel=bgr)
    write_expected("images/bgr24_core_bmp.rgba", WIDTH, HEIGHT, opaque)

    def packed(x, y):
        red, green, blue, alpha = pattern(x, y)
        return (alpha << 24) | (red << 16) | (green << 8) | blue
    bmp("bgra32_v4", 32, header_size=108, compression=3, masks=[0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000],
        pixel=packed)
    write_expected("images/bgra32_v4_bmp.rgba", WIDTH, HEIGHT, straight)
    bmp("bgrx32", 32, pixel=packed)
    write_expected("images/bgrx32_bmp.rgba", WIDTH, HEIGHT, opaque)

    def five(value):
        return value >> 3

    def expand(value, bits):
        return (value * 255 + ((1 << bits) - 1) // 2) // ((1 << bits) - 1)

    def rgb565(x, y):
        red, green, blue, _ = pattern(x, y)
        return (five(red) << 11) | ((green >> 2) << 5) | five(blue)
    bmp("rgb565", 16, compression=3, masks=[0xF800, 0x07E0, 0x001F], pixel=rgb565)
    write_expected("images/rgb565_bmp.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in (expand(five(pattern(x, y)[0]), 5), expand(pattern(x, y)[1] >> 2, 6),
                                  expand(five(pattern(x, y)[2]), 5), 255)])

    def rgb555(x, y):
        red, green, blue, _ = pattern(x, y)
        return (five(red) << 10) | (five(green) << 5) | five(blue)
    bmp("rgb555", 16, pixel=rgb555)
    write_expected("images/rgb555_bmp.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in (expand(five(pattern(x, y)[0]), 5), expand(five(pattern(x, y)[1]), 5),
                                  expand(five(pattern(x, y)[2]), 5), 255)])

    for bits in (1, 4, 8):
        count = 1 << bits
        palette = [((index * 53) & 255, (index * 91 + 30) & 255, (200 - index * 3) & 255) for index in range(count)]
        index_of = lambda x, y, count=count: (x * 3 + y) % count
        bmp(f"palette{bits}", bits, palette=palette, pixel=index_of)
        write_expected(f"images/palette{bits}_bmp.rgba", WIDTH, HEIGHT,
                       [value for y in range(HEIGHT) for x in range(WIDTH)
                        for value in palette[index_of(x, y)] + (255,)])


def tga(name, image_type, depth, pixels, descriptor=0, color_map=None, map_depth=0, run_length=False):
    """pixels is a list, top row first, of the stored bytes of each pixel."""
    rows = [pixels[row * WIDTH:(row + 1) * WIDTH] for row in range(HEIGHT)]
    if not descriptor & 0x20:
        rows.reverse()
    stored = [pixel for row in rows for pixel in row]

    body = bytearray()
    if run_length:
        index = 0
        while index < len(stored):
            run = 1
            while index + run < len(stored) and run < 128 and stored[index + run] == stored[index]:
                run += 1
            if run > 1:
                body.append(0x80 | (run - 1))
                body += stored[index]
                index += run
            else:
                raw = 1
                while index + raw < len(stored) and raw < 128 and (index + raw + 1 >= len(stored) or
                                                                    stored[index + raw] != stored[index + raw + 1]):
                    raw += 1
                body.append(raw - 1)
                for pixel in stored[index:index + raw]:
                    body += pixel
                index += raw
    else:
        for pixel in stored:
            body += pixel

    identifier = b"easyforge"
    map_bytes = b"".join(color_map) if color_map else b""
    header = struct.pack("<BBBHHBHHHHBB", len(identifier), 1 if color_map else 0, image_type, 0,
                         len(color_map) if color_map else 0, map_depth, 0, 0, WIDTH, HEIGHT, depth, descriptor)
    write(f"images/{name}.tga", header + identifier + map_bytes + body)


def write_tgas():
    opaque =[value for y in range(HEIGHT) for x in range(WIDTH) for value in pattern(x, y)[:3] + (255,)]
    top_rows = lambda function: [function(x, y) for y in range(HEIGHT) for x in range(WIDTH)]

    tga("bgr24", 2, 24, top_rows(lambda x, y: bytes((pattern(x, y)[2], pattern(x, y)[1], pattern(x, y)[0]))))
    write_expected("images/bgr24_tga.rgba", WIDTH, HEIGHT, opaque)

    # Runs of equal pixels, so run-length packets appear.
    def blocky(x, y):
        return pattern(x // 4 * 4, y)
    tga("bgra32_rle_top", 10, 32,
        top_rows(lambda x, y: bytes((blocky(x, y)[2], blocky(x, y)[1], blocky(x, y)[0], blocky(x, y)[3]))),
        descriptor=0x28, run_length=True)
    write_expected("images/bgra32_rle_top_tga.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH) for value in blocky(x, y)])

    tga("gray8_rle", 11, 8, top_rows(lambda x, y: bytes(((x // 3 * 40 + y) & 255,))), run_length=True)
    write_expected("images/gray8_rle_tga.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH) for value in ((x // 3 * 40 + y) & 255,) * 3 + (255,)])

    def argb1555(x, y):
        red, green, blue, alpha = pattern(x, y)
        return ((1 if alpha >= 128 else 0) << 15) | ((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3)
    tga("argb16", 2, 16, top_rows(lambda x, y: argb1555(x, y).to_bytes(2, "little")), descriptor=0x01)
    write_expected("images/argb16_tga.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in (((pattern(x, y)[0] >> 3) * 255 + 15) // 31, ((pattern(x, y)[1] >> 3) * 255 + 15) // 31,
                                  ((pattern(x, y)[2] >> 3) * 255 + 15) // 31, 255 if pattern(x, y)[3] >= 128 else 0)])

    color_map = [bytes(((index * 9) & 255, (index * 5) & 255, (index * 3) & 255)) for index in range(40)]
    tga("mapped8", 1, 8, top_rows(lambda x, y: bytes(((x + y * 3) % 40,))), color_map=color_map, map_depth=24)
    write_expected("images/mapped8_tga.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH)
                    for value in (color_map[(x + y * 3) % 40][2], color_map[(x + y * 3) % 40][1],
                                  color_map[(x + y * 3) % 40][0], 255)])


def qoi(name, channels, color):
    data = bytearray(b"qoif" + struct.pack(">IIBB", WIDTH, HEIGHT, channels, 0))
    seen = [(0, 0, 0, 0)] * 64
    previous = (0, 0, 0, 255)
    run = 0
    pixels = [color(x, y) for y in range(HEIGHT) for x in range(WIDTH)]
    for index, pixel in enumerate(pixels):
        if pixel == previous:
            run += 1
            if run == 62 or index == len(pixels) - 1:
                data.append(0xC0 | (run - 1))
                run = 0
            continue
        if run > 0:
            data.append(0xC0 | (run - 1))
            run = 0
        position = (pixel[0] * 3 + pixel[1] * 5 + pixel[2] * 7 + pixel[3] * 11) % 64
        if seen[position] == pixel:
            data.append(position)
        else:
            seen[position] = pixel
            if pixel[3] == previous[3]:
                red, green, blue = ((pixel[i] - previous[i] + 128) % 256 - 128 for i in range(3))
                red_green, blue_green = red - green, blue - green
                if -2 <= red <= 1 and -2 <= green <= 1 and -2 <= blue <= 1:
                    data.append(0x40 | ((red + 2) << 4) | ((green + 2) << 2) | (blue + 2))
                elif -32 <= green <= 31 and -8 <= red_green <= 7 and -8 <= blue_green <= 7:
                    data.append(0x80 | (green + 32))
                    data.append(((red_green + 8) << 4) | (blue_green + 8))
                else:
                    data += bytes((0xFE,) + pixel[:3])
            else:
                data += bytes((0xFF,) + pixel)
        previous = pixel
    data += bytes(7) + b"\x01"
    write(f"images/{name}.qoi", data)
    write_expected(f"images/{name}_qoi.rgba", WIDTH, HEIGHT, [value for pixel in pixels for value in pixel])


def write_qois():
    # Smooth changes, repeats, and returns to earlier colors exercise every operation.
    def smooth(x, y):
        if y == 5:
            return (200, 10, 10, 255)
        if y == 7 and x % 4 == 0:
            return (10, 20, 30, 255)
        return ((x * 3 + y * 20) & 255, (x * 2 + y * 21) & 255, (x + y * 19) & 255, 255)
    qoi("smooth", 3, smooth)
    qoi("alpha", 4, lambda x, y: pattern(x, y) if (x + y) % 5 else (7, 7, 7, 128))


# JPEG ----------------------------------------------------------------------------

LUMINANCE_QUANTIZATION = [
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99]
CHROMINANCE_QUANTIZATION = [
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99] + [99] * 32
ZIGZAG = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14,
          21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53,
          60, 61, 54, 47, 55, 62, 63]

DC_LUMINANCE = ([0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0], list(range(12)))
DC_CHROMINANCE = ([0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0], list(range(12)))
AC_LUMINANCE = ([0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D], bytes.fromhex(
    "01020300041105122131410613516107227114328191a1082342b1c11552d1f02433627282090a161718191a25262728292a34353637"
    "38393a434445464748494a535455565758595a636465666768696a737475767778797a838485868788898a92939495969798999aa2a3"
    "a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8f9fa"))
AC_CHROMINANCE = ([0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77], bytes.fromhex(
    "00010203110405213106124151076171132232810814429 1a1b1c109233352f0156272d10a162434e125f11718191a262728292a3536"
    "3738393a434445464748494a535455565758595a636465666768696a737475767778797a82838485868788898a92939495969798999a"
    "a2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae2e3e4e5e6e7e8e9eaf2f3f4f5f6f7f8f9fa"
    .replace(" ", "")))


def huffman_codes(table):
    counts, values = table
    assert sum(counts) == len(values) and len(set(values)) == len(values)
    codes = {}
    code = 0
    index = 0
    for length in range(1, 17):
        for _ in range(counts[length - 1]):
            codes[values[index]] = (code, length)
            code += 1
            index += 1
        code <<= 1
    return codes


class BitWriter:
    def __init__(self):
        self.data = bytearray()
        self.buffer = 0
        self.count = 0

    def write(self, value, length):
        for bit in range(length - 1, -1, -1):
            self.buffer = (self.buffer << 1) | ((value >> bit) & 1)
            self.count += 1
            if self.count == 8:
                self.data.append(self.buffer)
                if self.buffer == 0xFF:
                    self.data.append(0)
                self.buffer = 0
                self.count = 0

    def flush(self):
        if self.count:
            self.write((1 << (8 - self.count)) - 1, 8 - self.count)


def jpeg(name, color, gray=False, sampling=(1, 1), restart=0, quality=90):
    scale = 5000 / quality if quality < 50 else 200 - quality * 2
    tables = [[max(1, min(255, (value * scale + 50) // 100)) for value in table]
              for table in (LUMINANCE_QUANTIZATION, CHROMINANCE_QUANTIZATION)]

    def ycbcr(x, y):
        red, green, blue = color(min(x, WIDTH - 1), min(y, HEIGHT - 1))
        return (0.299 * red + 0.587 * green + 0.114 * blue,
                -0.168736 * red - 0.331264 * green + 0.5 * blue + 128,
                0.5 * red - 0.418688 * green - 0.081312 * blue + 128)

    horizontal, vertical = sampling
    units_wide = (WIDTH + 8 * horizontal - 1) // (8 * horizontal)
    units_high = (HEIGHT + 8 * vertical - 1) // (8 * vertical)

    def block(component, block_x, block_y, factor_x, factor_y):
        samples = []
        for y in range(8):
            for x in range(8):
                total = 0.0
                for sub_y in range(factor_y):
                    for sub_x in range(factor_x):
                        total += ycbcr((block_x * 8 + x) * factor_x + sub_x, (block_y * 8 + y) * factor_y + sub_y)[component]
                samples.append(total / (factor_x * factor_y) - 128)
        coefficients = []
        for v in range(8):
            for u in range(8):
                total = 0.0
                for y in range(8):
                    for x in range(8):
                        total += samples[y * 8 + x] * math.cos((2 * x + 1) * u * math.pi / 16) * \
                            math.cos((2 * y + 1) * v * math.pi / 16)
                cu = math.sqrt(0.5) if u == 0 else 1
                cv = math.sqrt(0.5) if v == 0 else 1
                coefficients.append(0.25 * cu * cv * total)
        table = tables[0 if component == 0 else 1]
        return [int(round(coefficients[ZIGZAG[index]] / table[index])) for index in range(64)]

    codes = [(huffman_codes(DC_LUMINANCE), huffman_codes(AC_LUMINANCE)),
             (huffman_codes(DC_CHROMINANCE), huffman_codes(AC_CHROMINANCE))]
    components = [(1, horizontal, vertical, 0)] if gray else [(1, horizontal, vertical, 0), (2, 1, 1, 1), (3, 1, 1, 1)]

    def size_of(value):
        return 0 if value == 0 else abs(value).bit_length()

    def bits_of(value, size):
        return value if value >= 0 else value + (1 << size) - 1

    writer = BitWriter()
    predictors = [0] * len(components)
    output = bytearray()
    units = units_wide * units_high
    unit = 0
    for unit_y in range(units_high):
        for unit_x in range(units_wide):
            for index, (_, h, v, table) in enumerate(components):
                factor_x = horizontal // h
                factor_y = vertical // v
                for y in range(v):
                    for x in range(h):
                        coefficients = block(index, unit_x * h + x, unit_y * v + y, factor_x, factor_y)
                        dc_codes, ac_codes = codes[table]
                        difference = coefficients[0] - predictors[index]
                        predictors[index] = coefficients[0]
                        size = size_of(difference)
                        writer.write(*dc_codes[size])
                        writer.write(bits_of(difference, size), size)
                        run = 0
                        for position in range(1, 64):
                            value = coefficients[position]
                            if value == 0:
                                run += 1
                                continue
                            while run > 15:
                                writer.write(*ac_codes[0xF0])
                                run -= 16
                            size = size_of(value)
                            writer.write(*ac_codes[(run << 4) | size])
                            writer.write(bits_of(value, size), size)
                            run = 0
                        if run:
                            writer.write(*ac_codes[0x00])
            unit += 1
            if restart and unit % restart == 0 and unit < units:
                writer.flush()
                output += writer.data + bytes((0xFF, 0xD0 + (unit // restart - 1) % 8))
                writer = BitWriter()
                predictors = [0] * len(components)
    writer.flush()
    output += writer.data

    def segment(marker, body):
        return bytes((0xFF, marker)) + struct.pack(">H", len(body) + 2) + bytes(body)

    data = bytearray(b"\xFF\xD8")
    data += segment(0xE0, b"JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00")
    data += segment(0xFE, b"made by generate.py")
    for index, table in enumerate(tables[:1] if gray else tables):
        data += segment(0xDB, bytes((index,)) + bytes(table))
    frame = bytes((8,)) + struct.pack(">HH", HEIGHT, WIDTH) + bytes((len(components),))
    for identifier, h, v, table in components:
        frame += bytes((identifier, (h << 4) | v, table))
    data += segment(0xC0, frame)
    huffman = [(0x00, DC_LUMINANCE), (0x10, AC_LUMINANCE)]
    if not gray:
        huffman += [(0x01, DC_CHROMINANCE), (0x11, AC_CHROMINANCE)]
    for class_and_index, (counts, values) in huffman:
        data += segment(0xC4, bytes((class_and_index,)) + bytes(counts) + bytes(values))
    if restart:
        data += segment(0xDD, struct.pack(">H", restart))
    scan = bytes((len(components),))
    for identifier, _, _, table in components:
        scan += bytes((identifier, (table << 4) | table))
    scan += bytes((0, 63, 0))
    data += segment(0xDA, scan)
    data += output + b"\xFF\xD9"
    write(f"images/{name}.jpg", data)


def write_jpegs():
    def smooth(x, y):
        return (int(128 + 100 * math.sin(x / 3.0)), int(128 + 90 * math.cos(y / 2.5)), int((x * 9 + y * 11) % 256))
    jpeg("color444", smooth)
    jpeg("color420", smooth, sampling=(2, 2))
    jpeg("color422", smooth, sampling=(2, 1), restart=1)
    jpeg("gray", smooth, gray=True, restart=2)

    # Only the start of a progressive JPEG, to check the error it gives.
    progressive = bytearray(b"\xFF\xD8")
    progressive += bytes((0xFF, 0xC2)) + struct.pack(">HBHHB", 11, 8, 8, 8, 1) + bytes((1, 0x11, 0))
    progressive += b"\xFF\xD9"
    write("images/progressive.jpg", progressive)


# Sounds --------------------------------------------------------------------------

def wav(name, rate, channels, format_tag, bits, samples, extensible=False):
    """samples are the stored integers, or floats for format 3, interleaved."""
    if format_tag == 3:
        body = b"".join(struct.pack("<f" if bits == 32 else "<d", value) for value in samples)
    elif bits == 8:
        body = bytes(samples)
    else:
        body = b"".join(value.to_bytes(bits // 8, "little", signed=True) for value in samples)
    block = channels * bits // 8
    if extensible:
        subformat = struct.pack("<H", format_tag) + bytes.fromhex("000000001000800000aa00389b71")
        fmt = struct.pack("<HHIIHHHHI", 0xFFFE, channels, rate, rate * block, block, bits, 22, bits, 3) + subformat
    else:
        fmt = struct.pack("<HHIIHH", format_tag, channels, rate, rate * block, block, bits)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"LIST" + struct.pack("<I", 5) + b"INFOx" + b"\x00"
    chunks += b"data" + struct.pack("<I", len(body)) + body
    write(f"sounds/{name}.wav", b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


def write_floats(name, values):
    write(name, b"".join(struct.pack("<f", value) for value in values))


def write_wavs():
    frames = 300
    tone = [math.sin(2 * math.pi * 440 * frame / 8000) for frame in range(frames)]
    stereo = [value for frame in range(frames) for value in (tone[frame], -tone[frame] * 0.5)]

    pcm16 = [int(round(value * 32000)) for value in stereo]
    wav("pcm16_stereo", 8000, 2, 1, 16, pcm16)
    write_floats("sounds/pcm16_stereo.f32", [value / 32768 for value in pcm16])

    pcm8 = [int(round(128 + value * 120)) for value in tone]
    wav("pcm8_mono", 8000, 1, 1, 8, pcm8)
    write_floats("sounds/pcm8_mono.f32", [(value - 128) / 128 for value in pcm8])

    pcm24 = [int(round(value * 8000000)) for value in stereo]
    wav("pcm24_stereo", 8000, 2, 1, 24, pcm24, extensible=True)
    write_floats("sounds/pcm24_stereo.f32", [value / 8388608 for value in pcm24])

    pcm32 = [int(round(value * 2000000000)) for value in tone]
    wav("pcm32_mono", 8000, 1, 1, 32, pcm32)
    write_floats("sounds/pcm32_mono.f32", [value / 2147483648 for value in pcm32])

    wav("float32_stereo", 8000, 2, 3, 32, stereo)
    write_floats("sounds/float32_stereo.f32", stereo)
    wav("float64_mono", 8000, 1, 3, 64, tone, extensible=True)
    write_floats("sounds/float64_mono.f32", tone)


QOA_SCALE = [1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048]


def round_half_away(value):
    return int(math.floor(abs(value) + 0.5)) * (1 if value >= 0 else -1)


def qoa_tables():
    dequantize = [[round_half_away(scale * factor) for factor in (0.75, -0.75, 2.5, -2.5, 4.5, -4.5, 7, -7)]
                  for scale in QOA_SCALE]
    reciprocal = [((1 << 16) + scale - 1) // scale for scale in QOA_SCALE]
    quantize = [7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6, 6]
    return dequantize, reciprocal, quantize


def qoa_encode(name, rate, channels, samples):
    """Encodes 16-bit samples (interleaved) with the QOA reference algorithm and
    returns the samples a correct decoder must produce."""
    dequantize, reciprocal, quantize = qoa_tables()
    frames = len(samples) // channels
    data = bytearray(b"qoaf" + struct.pack(">I", frames))
    decoded = [0] * len(samples)
    history = [[0, 0, 0, 0] for _ in range(channels)]
    weights = [[0, 0, -(1 << 13), 1 << 14] for _ in range(channels)]

    def clamp16(value):
        return max(-32768, min(32767, value))

    def divide(value, scale_index):
        result = (value * reciprocal[scale_index] + (1 << 15)) >> 16
        return result + ((value > 0) - (value < 0)) - ((result > 0) - (result < 0))

    for start in range(0, frames, 5120):
        count = min(5120, frames - start)
        slices = (count + 19) // 20
        size = 8 + 16 * channels + slices * 8 * channels
        data += struct.pack(">B", channels) + rate.to_bytes(3, "big") + struct.pack(">HH", count, size)
        for channel in range(channels):
            data += b"".join(struct.pack(">h", value) for value in history[channel])
            data += b"".join(struct.pack(">h", value) for value in weights[channel])
        for slice_start in range(start, start + count, 20):
            slice_end = min(slice_start + 20, start + count)
            for channel in range(channels):
                best = None
                for scale_index in range(16):
                    trial_history = list(history[channel])
                    trial_weights = list(weights[channel])
                    error = 0
                    codes = []
                    outputs = []
                    for frame in range(slice_start, slice_end):
                        sample = samples[frame * channels + channel]
                        predicted = sum(h * w for h, w in zip(trial_history, trial_weights)) >> 13
                        residual = sample - predicted
                        scaled = max(-8, min(8, divide(residual, scale_index)))
                        code = quantize[scaled + 8]
                        dequantized = dequantize[scale_index][code]
                        reconstructed = clamp16(predicted + dequantized)
                        error += (sample - reconstructed) ** 2
                        delta = dequantized >> 4
                        trial_weights = [w + (-delta if h < 0 else delta) for h, w in zip(trial_history, trial_weights)]
                        trial_history = trial_history[1:] + [reconstructed]
                        codes.append(code)
                        outputs.append(reconstructed)
                    if best is None or error < best[0]:
                        best = (error, scale_index, codes, outputs, trial_history, trial_weights)
                _, scale_index, codes, outputs, history[channel], weights[channel] = best
                value = scale_index
                for code in codes:
                    value = (value << 3) | code
                value <<= 3 * (20 - len(codes))
                data += struct.pack(">Q", value)
                for offset, output in enumerate(outputs):
                    decoded[(slice_start + offset) * channels + channel] = output
    write(f"sounds/{name}.qoa", data)
    return decoded


def write_qoas():
    frames = 12000
    samples = []
    for frame in range(frames):
        left = 0.6 * math.sin(2 * math.pi * 330 * frame / 22050) + 0.2 * math.sin(2 * math.pi * 1250 * frame / 22050)
        right = 0.5 * math.sin(2 * math.pi * 220 * frame / 22050 + 1.0)
        samples += [int(round(left * 32767)), int(round(right * 32767))]
    decoded = qoa_encode("music", 22050, 2, samples)
    write_floats("sounds/music_qoa_decoded.f32", [value / 32768 for value in decoded])
    write_floats("sounds/music_qoa_source.f32", [value / 32768 for value in samples])


# Fonts ---------------------------------------------------------------------------

def checksum_pad(data):
    return data + b"\x00" * ((4 - len(data) % 4) % 4)


def encode_simple_glyph(contours):
    points = [point for contour in contours for point in contour]
    xs = [x for x, _, _ in points]
    ys = [y for _, y, _ in points]
    header = struct.pack(">hhhhh", len(contours), min(xs), min(ys), max(xs), max(ys))
    ends = []
    total = 0
    for contour in contours:
        total += len(contour)
        ends.append(total - 1)
    body = b"".join(struct.pack(">H", end) for end in ends) + struct.pack(">H", 0)

    flags = []
    x_bytes = bytearray()
    y_bytes = bytearray()
    previous_x = previous_y = 0
    for x, y, on_curve in points:
        flag = 1 if on_curve else 0
        dx, dy = x - previous_x, y - previous_y
        if dx == 0:
            flag |= 0x10
        elif -255 <= dx <= 255:
            flag |= 0x02 | (0x10 if dx > 0 else 0)
            x_bytes.append(abs(dx))
        else:
            x_bytes += struct.pack(">h", dx)
        if dy == 0:
            flag |= 0x20
        elif -255 <= dy <= 255:
            flag |= 0x04 | (0x20 if dy > 0 else 0)
            y_bytes.append(abs(dy))
        else:
            y_bytes += struct.pack(">h", dy)
        flags.append(flag)
        previous_x, previous_y = x, y

    flag_bytes = bytearray()
    index = 0
    while index < len(flags):
        repeat = 0
        while index + repeat + 1 < len(flags) and flags[index + repeat + 1] == flags[index] and repeat < 255:
            repeat += 1
        if repeat:
            flag_bytes += bytes((flags[index] | 0x08, repeat))
        else:
            flag_bytes.append(flags[index])
        index += repeat + 1
    return header + body + flag_bytes + x_bytes + y_bytes


def encode_composite_glyph(components, bounds):
    data = struct.pack(">hhhhh", -1, *bounds)
    for index, (glyph, dx, dy, scale) in enumerate(components):
        flags = 0x0001 | 0x0002
        if index + 1 < len(components):
            flags |= 0x0020
        if scale is not None:
            flags |= 0x0008
        data += struct.pack(">HHhh", flags, glyph, dx, dy)
        if scale is not None:
            data += struct.pack(">h", int(round(scale * 16384)))
    return data


def utf16(text):
    return text.encode("utf-16-be")


def build_font(family, style, glyphs, characters, use_format12, long_locations, kerning=None, positioning=None,
               typographic_metrics=False, metric_count=None):
    units = 1000
    glyph_data = []
    for glyph in glyphs:
        if "contours" in glyph:
            glyph_data.append(encode_simple_glyph(glyph["contours"]))
        elif "components" in glyph:
            glyph_data.append(encode_composite_glyph(glyph["components"], glyph["bounds"]))
        else:
            glyph_data.append(b"")
    glyf = bytearray()
    offsets = []
    for data in glyph_data:
        offsets.append(len(glyf))
        glyf += data + b"\x00" * (len(data) % 2)
    offsets.append(len(glyf))
    if long_locations:
        loca = b"".join(struct.pack(">I", offset) for offset in offsets)
    else:
        loca = b"".join(struct.pack(">H", offset // 2) for offset in offsets)

    head = struct.pack(">IIIIHH", 0x00010000, 0x00010000, 0, 0x5F0F3CF5, 0x000B, units) + bytes(16)
    head += struct.pack(">hhhhHHhhh", -100, -250, 1100, 900, 0, 8, 2, 1 if long_locations else 0, 0)

    metric_count = metric_count or len(glyphs)
    hhea = struct.pack(">Ihhh", 0x00010000, 800, -200, 90) + struct.pack(">Hhhhhhh", 1200, 0, 0, 1100, 1, 0, 0)
    hhea += bytes(8) + struct.pack(">hH", 0, metric_count)
    maxp = struct.pack(">IH", 0x00010000, len(glyphs)) + bytes(26)

    hmtx = bytearray()
    for index, glyph in enumerate(glyphs):
        if index < metric_count:
            hmtx += struct.pack(">Hh", glyph["advance"], glyph.get("bearing", 0))
        else:
            hmtx += struct.pack(">h", glyph.get("bearing", 0))

    ordered = sorted(characters.items())
    if use_format12:
        groups = []
        for code, glyph in ordered:
            if groups and code == groups[-1][1] + 1 and glyph == groups[-1][2] + (code - groups[-1][0]):
                groups[-1][1] = code
            else:
                groups.append([code, code, glyph])
        subtable = struct.pack(">HHIII", 12, 0, 16 + 12 * len(groups), 0, len(groups))
        subtable += b"".join(struct.pack(">III", first, last, glyph) for first, last, glyph in groups)
        cmap = struct.pack(">HHHHI", 0, 1, 3, 10, 12) + subtable
    else:
        runs = []
        for code, glyph in ordered:
            if runs and code == runs[-1][-1][0] + 1:
                runs[-1].append((code, glyph))
            else:
                runs.append([(code, glyph)])
        segments = len(runs) + 1
        ends, starts, deltas, ranges = [], [], [], []
        glyph_array = []
        for index, run in enumerate(runs):
            starts.append(run[0][0])
            ends.append(run[-1][0])
            if all(glyph - code == run[0][1] - run[0][0] for code, glyph in run):
                deltas.append((run[0][1] - run[0][0]) & 0xFFFF)
                ranges.append(None)
            else:
                deltas.append(0)
                ranges.append(len(glyph_array))
                glyph_array += [glyph for _, glyph in run]
        starts.append(0xFFFF)
        ends.append(0xFFFF)
        deltas.append(1)
        ranges.append(None)
        range_values = []
        for index, start in enumerate(ranges):
            if start is None:
                range_values.append(0)
            else:
                # Offset in bytes from this idRangeOffset entry to its first glyph.
                range_values.append(2 * (segments - index) + 2 * start)
        body = struct.pack(">HHHH", segments * 2, 0, 0, 0)
        body += b"".join(struct.pack(">H", end) for end in ends) + struct.pack(">H", 0)
        body += b"".join(struct.pack(">H", start) for start in starts)
        body += b"".join(struct.pack(">H", delta) for delta in deltas)
        body += b"".join(struct.pack(">H", value) for value in range_values)
        body += b"".join(struct.pack(">H", glyph) for glyph in glyph_array)
        subtable = struct.pack(">HHH", 4, 6 + len(body), 0) + body
        cmap = struct.pack(">HHHHI", 0, 1, 3, 1, 12) + subtable

    records = [(1, family), (2, style), (4, f"{family} {style}")]
    strings = b""
    name = struct.pack(">HHH", 0, len(records), 6 + 12 * len(records))
    for identifier, text in records:
        encoded = utf16(text)
        name += struct.pack(">HHHHHH", 3, 1, 0x409, identifier, len(encoded), len(strings))
        strings += encoded
    name += strings

    # OS/2 version 4: fsSelection at byte 62, then the typographic metrics at 68.
    os2 = struct.pack(">HhHHH", 4, 500, 400, 5, 0) + bytes(20) + struct.pack(">h", 0) + bytes(10) + bytes(16)
    os2 += b"EFTS" + struct.pack(">HHH", 0x80 if typographic_metrics else 0x40, 32, 0xFFFF)
    os2 += struct.pack(">hhhHH", 750, -250, 100, 1000, 300) + bytes(8) + struct.pack(">hhHHH", 500, 700, 0, 32, 2)
    assert len(os2) == 96
    post = struct.pack(">IIhhIIIII", 0x00030000, 0, -100, 50, 0, 0, 0, 0, 0)

    tables = {b"head": head, b"hhea": hhea, b"maxp": maxp, b"hmtx": bytes(hmtx), b"loca": loca, b"glyf": bytes(glyf),
              b"cmap": cmap, b"name": name, b"OS/2": os2, b"post": post}
    if kerning:
        pairs = sorted(kerning.items())
        subtable = struct.pack(">HHHHHHH", 0, 14 + 6 * len(pairs), 0x0001, len(pairs), 6, 0, 0)
        subtable += b"".join(struct.pack(">HHh", left, right, value) for (left, right), value in pairs)
        tables[b"kern"] = struct.pack(">HH", 0, 1) + subtable
    if positioning:
        tables[b"GPOS"] = positioning
    return tables


def assemble_font(tables, base=0):
    """The table directory and tables, with offsets counted from `base`."""
    tags = sorted(tables)
    directory = struct.pack(">IHHHH", 0x00010000, len(tags), 0, 0, 0)
    offset = base + 12 + 16 * len(tags)
    records = b""
    body = b""
    for tag in tags:
        data = tables[tag]
        records += tag + struct.pack(">III", 0, offset, len(data))
        padded = checksum_pad(data)
        body += padded
        offset += len(padded)
    return directory + records + body


def positioning_table(a, o):
    """GPOS with a "kern" feature of two lookups: pair adjustment format 1 (A then O,
    with an X placement before the X advance), and format 2 inside an extension (O then A)."""
    def coverage(glyphs):
        return struct.pack(">HH", 1, len(glyphs)) + b"".join(struct.pack(">H", glyph) for glyph in glyphs)

    pair_set = struct.pack(">H", 1) + struct.pack(">Hhh", o, 5, -70)
    format1_header = 12
    format1 = struct.pack(">HHHHHH", 1, format1_header + len(pair_set), 0x0005, 0, 1, format1_header)
    format1 += pair_set + coverage([a])

    class_first = struct.pack(">HHHH", 1, o, 1, 1)
    class_second = struct.pack(">HHHH", 1, a, 1, 1)
    records = struct.pack(">hhhh", 0, 0, 0, -30)
    header_size = 16
    offset_records = header_size
    offset_cover = offset_records + len(records)
    offset_first = offset_cover + len(coverage([o]))
    offset_second = offset_first + len(class_first)
    format2 = struct.pack(">HHHHHHHH", 2, offset_cover, 0x0004, 0, offset_first, offset_second, 2, 2)
    format2 += records + coverage([o]) + class_first + class_second
    extension = struct.pack(">HHI", 1, 2, 8) + format2

    lookup1 = struct.pack(">HHHH", 2, 0, 1, 8) + format1
    lookup2 = struct.pack(">HHHH", 9, 0, 1, 8) + extension
    lookup_list = struct.pack(">HHH", 2, 6, 6 + len(lookup1)) + lookup1 + lookup2

    feature = struct.pack(">HHHH", 0, 2, 0, 1)
    feature_list = struct.pack(">H", 1) + b"kern" + struct.pack(">H", 8) + feature
    language = struct.pack(">HHHH", 0, 0xFFFF, 1, 0)
    script = struct.pack(">HH", 4, 0) + language
    script_list = struct.pack(">H", 1) + b"DFLT" + struct.pack(">H", 8) + script

    header_length = 10
    offset_scripts = header_length
    offset_features = offset_scripts + len(script_list)
    offset_lookups = offset_features + len(feature_list)
    return struct.pack(">HHHHH", 1, 0, offset_scripts, offset_features, offset_lookups) + script_list + \
        feature_list + lookup_list


def write_fonts():
    notdef = {"advance": 600, "bearing": 50, "contours": [[(50, 0, 1), (50, 700, 1), (550, 700, 1), (550, 0, 1)]]}
    space = {"advance": 250}
    # An A: a triangle with a triangular hole going the other way.
    letter_a = {"advance": 700, "bearing": 0, "contours": [
        [(0, 0, 1), (350, 700, 1), (700, 0, 1)],
        [(250, 150, 1), (450, 150, 1), (350, 400, 1)]]}
    # An O made only of off-curve points, the case where the outline has to invent its start.
    letter_o = {"advance": 800, "bearing": 50, "contours": [
        [(50, 350, 0), (400, 700, 0), (750, 350, 0), (400, 0, 0)],
        [(250, 350, 0), (400, 200, 0), (550, 350, 0), (400, 500, 0)]]}
    dot = {"advance": 200, "bearing": 50, "contours": [[(50, 0, 1), (50, 100, 1), (150, 100, 1), (150, 0, 1)]]}
    # A with two dots above, built from the other glyphs, the second dot half size.
    a_dots = {"advance": 700, "bearing": 0, "components": [(2, 0, 0, None), (4, 200, 800, None), (4, 400, 800, 0.5)],
              "bounds": (0, 0, 700, 900)}
    # Uses long coordinate deltas.
    wide = {"advance": 1200, "bearing": 0, "contours": [[(0, 0, 1), (0, 600, 1), (1100, 600, 1), (1100, 0, 1)]]}
    glyphs = [notdef, space, letter_a, letter_o, dot, a_dots, wide]
    characters = {0x20: 1, 0x41: 2, 0x4F: 3, 0x2E: 4, 0xC4: 5, 0x57: 6, 0x58: 3, 0x59: 2, 0x5A: 6}

    regular = build_font("Easyforge Test", "Regular", glyphs, characters, use_format12=False, long_locations=False,
                         kerning={(2, 3): -50, (3, 2): -20}, metric_count=5)
    write("fonts/test_regular.ttf", assemble_font(regular))

    bold_characters = dict(characters)
    bold_characters[0x1F600] = 3
    bold = build_font("Easyforge Test", "Bold", glyphs, bold_characters, use_format12=True, long_locations=True,
                      positioning=positioning_table(2, 3), typographic_metrics=True)
    write("fonts/test_bold.ttf", assemble_font(bold))

    header_size = 12 + 4 * 2
    first = assemble_font(regular, header_size)
    second = assemble_font(bold, header_size + len(first))
    collection = b"ttcf" + struct.pack(">IIII", 0x00010000, 2, header_size, header_size + len(first)) + first + second
    write("fonts/test_collection.ttc", collection)


# Models --------------------------------------------------------------------------

def write_models():
    write("models/cube.obj", b"""# A unit cube with texture coordinates, normals, and quads.
mtllib cube.mtl
o Cube
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 0 1
vn 0 0 -1
vn 1 0 0
vn -1 0 0
vn 0 1 0
vn 0 -1 0
usemtl Painted
f 1/1/1 2/2/1 3/3/1 4/4/1
f 6/1/2 5/2/2 8/3/2 7/4/2
f 2/1/3 6/2/3 7/3/3 3/4/3
f 5/1/4 1/2/4 4/3/4 8/4/4
usemtl Metal
f 4/1/5 3/2/5 7/3/5 8/4/5
f 5/1/6 6/2/6 2/3/6 1/4/6
""")
    write("models/cube.mtl", b"""# Two materials.
newmtl Painted
Kd 0.5 0.25 1.0
Ns 98
d 0.75
map_Kd textures\\paint.png

newmtl Metal
Kd 1 1 1
Pm 1
Pr 0.2
Ke 0 0 0
map_Bump -bm 1.0 textures/metal_normal.png
""")
    write("models/pyramid.obj", b"""# No normals and negative indices; normals must be made smooth. Faces wind
# counterclockwise seen from outside.
v 0 0 0
v 1 0 0
v 1 0 1
v 0 0 1
v 0.5 1 0.5
f -5 -4 -3
f -5 -3 -2
f -5 -1 -4
f -4 -1 -3
f -3 -1 -2
f -2 -1 -5
""")
    write("models/broken.obj", b"v 0 0 0\nv 1 0 0\nf 1 2 3\n")

# Compression ----------------------------------------------------------------------

def write_compression():
    # Repeats, long matches, and every byte value, so all block types and codes appear.
    original = (b"easyforge " * 300 + bytes(range(256)) * 4 +
                b"".join(bytes(((index * 7919) >> 3) & 255 for index in range(start, start + 50)) for start in range(0, 5000, 50)))
    write("compression/original.bin", original)
    write("compression/dynamic.zlib", zlib.compress(original, 9))
    write("compression/stored.zlib", zlib.compress(original, 0))
    fixed = zlib.compressobj(9, zlib.DEFLATED, 15, 9, zlib.Z_FIXED)
    write("compression/fixed.zlib", fixed.compress(original) + fixed.flush())
    raw = zlib.compressobj(9, zlib.DEFLATED, -15)
    write("compression/raw.deflate", raw.compress(original) + raw.flush())
    # The same data compressed in two separate flushes, so the stream has several blocks.
    blocks = zlib.compressobj(6)
    write("compression/blocks.zlib",
          blocks.compress(original[:3000]) + blocks.flush(zlib.Z_FULL_FLUSH) + blocks.compress(original[3000:]) + blocks.flush())


if __name__ == "__main__":
    write_pngs()
    write_bmps()
    write_tgas()
    write_qois()
    write_jpegs()
    write_wavs()
    write_qoas()
    write_fonts()
    write_models()
    write_compression()
    write_expected("images/pattern.rgba", WIDTH, HEIGHT,
                   [value for y in range(HEIGHT) for x in range(WIDTH) for value in pattern(x, y)[:3] + (255,)])
    print("Wrote the test files into", HERE)
