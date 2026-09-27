# Colors

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Color background = Color::Hex("#15151A");
Color shadow = Color::Black.WithAlpha(0.35f);
Color accent = { 0.9f, 0.35f, 0.1f };                 // alpha is 1

Log("background is {}", background);                  // background is #15151A
```

## What the numbers mean

```cpp
struct Color
{
    float Red = 0.0f;
    float Green = 0.0f;
    float Blue = 0.0f;
    float Alpha = 1.0f;
};
```

Components go from 0 to 1. Alpha 1 is opaque and 0 is invisible. The red,
green, and blue values are sRGB, the same space hex codes and design tools use,
so a color copied from a design tool looks the same in easyforge. Blending in a
physically correct way needs linear light instead; `graphics` converts when it
needs to, and `ToLinear` does the same for your own code.

## Making colors

| Written | Result |
|---|---|
| `Color::Hex("#RRGGBB")` | From a hex code. Also `#RGB`, `#RGBA`, and `#RRGGBBAA`, with or without `#`, in either case |
| `Color::IsHex(text)` | True when `Hex` can read the text |
| `Color::FromBytes(red, green, blue, alpha)` | From numbers 0 to 255. Alpha is 255 unless given |
| `Color::White`, `Color::Black`, `Color::Transparent` | The three named colors |
| `color.WithAlpha(0.5f)` | The same color with a different alpha |

`Hex` never fails. Text that is not a hex color gives magenta, `#FF00FF`, which is
hard to miss on screen. When the text comes from outside the program, such as a
settings file, check it with `IsHex` first. `Hex`, `IsHex`, and `FromBytes` are
`constexpr`, so colors can be constants.

## Converting

| Written | Result |
|---|---|
| `color.ToHex()` | `"#RRGGBB"` when opaque, otherwise `"#RRGGBBAA"`. Components outside 0 to 1 are clamped |
| `color.ToLinear()` | The same color in linear light. Alpha is unchanged |
| `Color::FromLinear(linear)` | Back from linear light to sRGB |
| `Lerp(from, to, amount)` | Blends each component. This blends in sRGB; for a physically even blend, convert to linear first and back afterward |
| `NearlyEqual(first, second, tolerance)` | True when every component is within `tolerance` |

`std::format` and `Log` print a color as its hex code.

## Limitations

- There are no HSV or HSL conversions yet.
- Only white, black, and transparent have names.
- Colors are not premultiplied by alpha; `graphics` takes care of that where it
  matters.
