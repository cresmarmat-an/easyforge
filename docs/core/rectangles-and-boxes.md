# Rectangles and boxes

A `Rectangle` is an area on a flat surface, such as part of a window. A
`BoundingBox` is a box in 3D lined up with the X, Y, and Z axes, the usual quick
test for whether two objects might touch.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Rectangle button = { 20.0f, 20.0f, 120.0f, 40.0f };
if (button.Contains(mousePosition))
{
    Log("over the button");
}

BoundingBox bounds;
for (Vector3 point : points)
{
    bounds.Include(point);
}
```

## Rectangle

```cpp
struct Rectangle
{
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};
```

`X` and `Y` are the corner the rectangle starts from. The names `Top` and
`Bottom` below assume Y grows downward, as it does on screen.

| Written | Result |
|---|---|
| `Rectangle::FromCorners(first, second)` | The rectangle between two opposite corners, in either order |
| `Left()`, `Right()`, `Top()`, `Bottom()` | The edges |
| `Position()`, `Size()`, `Center()` | As `Vector2` |
| `IsEmpty()` | True when the width or height is zero or less |
| `Contains(point)` | True when the point is inside |
| `Intersects(other)` | True when the two share some area |
| `Intersection(first, second)` | The shared area, or an empty rectangle |
| `Union(first, second)` | The smallest rectangle covering both. Empty rectangles are ignored |

`Contains` counts the left and top edges as inside and the right and bottom
edges as outside. Two rectangles side by side then never both contain the same
point, which matters when deciding which button a click belongs to. For the same
reason, rectangles that only touch do not intersect.

## BoundingBox

```cpp
struct BoundingBox
{
    Vector3 Minimum;    // starts at +infinity
    Vector3 Maximum;    // starts at -infinity
};
```

A new `BoundingBox` is empty, and `Include` grows it.

| Written | Result |
|---|---|
| `BoundingBox::FromPoints(points)` | The box around a list of points, from any `std::span<const Vector3>` |
| `Include(point)`, `Include(otherBox)` | Grows the box to cover them. Including an empty box changes nothing |
| `IsEmpty()` | True until something has been included |
| `Center()`, `Size()` | The size of an empty box is zero |
| `Contains(point)` | Points on the surface count as inside |
| `Intersects(other)` | Boxes that touch count as intersecting; an empty box intersects nothing |

A box around a single point is not empty: it has zero size and contains that
point.

## Limitations

- Coordinates are `float`. There is no integer rectangle for pixel-exact work.
- A `BoundingBox` cannot rotate. Turning an object means computing a new box
  around its turned corners.
