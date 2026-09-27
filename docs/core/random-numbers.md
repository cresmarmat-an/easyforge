# Random numbers

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Random random;                                  // different every run
int roll = random.IntegerBetween(1, 6);
float spread = random.Between(-0.1f, 0.1f);

Random level(20260927);                         // the same every run
Vector2 treePosition = level.InsideCircle() * 50.0f;
```

`Random` uses the PCG32 generator: fast, small, and good enough for games,
simulations, and procedural content.

> [!WARNING] Not for secrets
> Do not use `Random` for passwords, keys, tokens, or anything an attacker could
> gain from predicting. Its numbers can be predicted from a few outputs.

## Seeds

A `Random` made with a seed gives the same numbers every time, on every
platform: `Random random(42);` always starts with 3270867926. That makes
generated levels repeatable and bugs reproducible. A `Random` made without a
seed starts from the system's random device and differs every run.

## Getting numbers

| Function | Result |
|---|---|
| `Next()` | The next 32 random bits, as `std::uint32_t` |
| `Fraction()` | A `float` from 0 up to, but not including, 1 |
| `Between(minimum, maximum)` | A `float` from `minimum` up to, but not including, `maximum` |
| `IntegerBetween(minimum, maximum)` | An `int` from `minimum` to `maximum`, both included, every value equally likely. Swapped arguments are fine |
| `Chance(probability)` | True with that probability: `Chance(0.25f)` is true a quarter of the time |
| `InsideCircle()` | A point inside a circle of radius 1, every spot equally likely |
| `Direction()` | A 3D direction of length 1, every direction equally likely |

`IntegerBetween` avoids the bias of `Next() % count`, which makes small numbers
slightly more likely than large ones.

## Threads

A `Random` is not safe to use from two threads at once. Give each thread its own,
with its own seed.

## Limitations

- `Next`, `Fraction`, `IntegerBetween`, and `Chance` give identical results on
  every platform. `Between`, `InsideCircle`, and `Direction` do further
  arithmetic with floats, and different compilers can round the last digit
  differently.
- There is no way to choose a PCG32 stream other than the default, or to jump
  ahead in the sequence.
