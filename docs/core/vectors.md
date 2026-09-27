# Vectors

`Vector2`, `Vector3`, and `Vector4` hold two, three, or four `float` values.
They are plain structs, so you can write them with braces:

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Vector3 position = { 1.0f, 2.0f, 3.0f };
Vector3 velocity = { .X = 0.5f, .Z = -1.0f };     // Y is 0

position += velocity * 2.0f;
float speed = Length(velocity);
Vector3 direction = Normalize(velocity);

Log("now at {}, moving at {:.2f}", position, speed);   // now at (2, 2, 1), moving at 1.12
```

The components are `X`, `Y`, `Z`, and `W`. A vector made with `{}` is all zeros.

## Arithmetic

| Written | Result |
|---|---|
| `first + second`, `first - second` | Adds or subtracts each component |
| `first * second`, `first / second` | Multiplies or divides each component |
| `vector * 2.0f`, `2.0f * vector`, `vector / 2.0f` | Scales every component |
| `-vector` | Flips every component |
| `+=`, `-=`, `*=`, `/=` | The same, changing the vector in place |
| `first == second` | True when every component is exactly equal |

`Vector3::XY()` gives the first two components as a `Vector2`, and
`Vector4::XY()` and `Vector4::XYZ()` do the same for a `Vector4`.

## Functions

| Function | Works on | Result |
|---|---|---|
| `Dot(first, second)` | 2, 3, 4 | Sum of the component products |
| `Cross(first, second)` | 3 | A vector at right angles to both. Right-handed: `Cross(X, Y)` is `Z` |
| `Cross(first, second)` | 2 | A number, positive when `second` is counterclockwise from `first` |
| `Length(vector)`, `LengthSquared(vector)` | 2, 3, 4 | The length, or its square, which avoids a square root |
| `Distance(first, second)` | 2, 3 | Length of the difference |
| `Normalize(vector)` | 2, 3, 4 | Same direction, length 1. A zero vector stays zero |
| `Lerp(from, to, amount)` | 2, 3, 4 | `from` at amount 0, `to` at amount 1 |
| `Min(first, second)`, `Max(first, second)` | 2, 3 | Smallest or largest of each component |
| `Abs(vector)` | 2, 3 | Each component without its sign |
| `Perpendicular(vector)` | 2 | Turned a quarter turn counterclockwise |
| `NearlyEqual(first, second, tolerance)` | 2, 3, 4 | True when every component is within `tolerance` |

`Normalize` returns a zero vector for a zero vector, instead of dividing by
zero. Check `LengthSquared(vector) > 0.0f` first when a zero result would be a
problem.

## Numbers

These work on plain numbers:

| Name | What it is |
|---|---|
| `Pi`, `Tau` | 3.14159... and two times Pi, a full turn in radians |
| `Radians(degrees)`, `Degrees(radians)` | Converts between the two |
| `Min(first, second)`, `Max(first, second)` | For any type with `<`; both must be the same type |
| `Clamp(value, minimum, maximum)` | `value` kept between the two |
| `Lerp(from, to, amount)` | `from + (to - from) * amount` |
| `NearlyEqual(first, second, tolerance)` | True when the difference is at most `tolerance`, 0.00001 by default |

Everything that does not need a square root or trigonometry is `constexpr`.

## Printing

`std::format` and `Log` print vectors as `(1, 2, 3)`. A number format applies to
every component: `std::format("{:.1f}", position)` gives `(2.0, 2.0, 1.0)`.

## Limitations

- Components are always `float`. There are no integer or double vectors.
- `Distance`, `Min`, `Max`, and `Abs` exist for 2 and 3 components only.
- There are no swizzles beyond `XY()` and `XYZ()`.
- `==` compares exactly. Use `NearlyEqual` for results of arithmetic.
