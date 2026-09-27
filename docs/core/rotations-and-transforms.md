# Rotations and transforms

A `Quaternion` is a rotation. A `Transform` is a position, a rotation, and a
scale together: everything needed to place an object.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Transform crate = {
    .Position = { 0.0f, 1.0f, 0.0f },
    .Rotation = Quaternion::FromAngles(0.0f, Radians(90.0f), 0.0f),
    .Scale = { 2.0f, 2.0f, 2.0f },
};

Vector3 corner = crate.ApplyToPoint({ 0.5f, 0.5f, 0.5f });
Matrix4 model = crate.ToMatrix();
```

## Angles

Angles are in radians; `Radians(90.0f)` converts from degrees. A positive angle
turns counterclockwise when the axis points toward you, the usual right-handed
rule. So a quarter turn around +Y turns +X into -Z.

## Making rotations

| Function | Result |
|---|---|
| `Quaternion {}`, `Quaternion::Identity()` | No rotation |
| `Quaternion::FromAxisAngle(axis, angle)` | Turns around `axis`, which does not need to be length 1 |
| `Quaternion::FromAngles(pitch, yaw, roll)` | Turns around X (pitch), Y (yaw), and Z (roll) |
| `Quaternion::LookRotation(forward, up)` | Turns the forward direction (-Z) toward `forward`, with its top as close to `up` as possible. `up` is +Y unless given |
| `Quaternion::FromMatrix(matrix)` | The rotation in a `Matrix3` that has no scale |

`FromAngles` applies roll first, then pitch, then yaw. That is the order that
suits a camera or a character: yaw turns left and right around the world's up
axis, and pitch tilts up and down around the object's own side axis.

`LookRotation` still works when `forward` points straight along `up`; it then
picks a sideways direction itself.

## Using rotations

| Written | Result |
|---|---|
| `rotation.Rotate(vector)` | The vector turned |
| `first * second` | Turns by `second`, then by `first` |
| `Inverse(rotation)` | The opposite rotation |
| `Slerp(from, to, amount)` | Turns from `from` (amount 0) to `to` (amount 1) at a steady speed, the short way around |
| `Normalize(rotation)` | Rescales to length 1 |
| `Length(rotation)`, `Dot(first, second)`, `Conjugate(rotation)` | The quaternion as four numbers |
| `NearlyEqual(first, second, tolerance)` | Compares the four numbers |

Rotations made by the functions above already have length 1. After combining
thousands of them, rounding makes the length drift, and `Normalize` brings it
back.

A rotation can be written two ways: its four numbers, or all four negated. They
turn things the same way but `NearlyEqual` sees them as different. To test
whether two quaternions are the same rotation, check that
`std::abs(Dot(first, second))` is close to 1.

## Transforms

```cpp
struct Transform
{
    Vector3 Position = {};
    Quaternion Rotation = {};
    Vector3 Scale = { 1.0f, 1.0f, 1.0f };
};
```

A transform applies its scale first, then its rotation, then its position.

| Written | Result |
|---|---|
| `transform.ApplyToPoint(point)` | The point scaled, turned, and moved |
| `transform.ApplyToDirection(direction)` | Scaled and turned, not moved |
| `transform.ToMatrix()` | The same as a `Matrix4` |
| `Combine(parent, child)` | The child's transform in the space the parent lives in |

`Combine` is how a transform hierarchy works: a sword held by a hand held by an
arm. It is exact except in one case. When the parent is scaled unevenly, say
twice as wide as it is tall, and the child is rotated, the combined result would
need a shear, which a `Transform` cannot hold. The result's scale is then the
closest a `Transform` can get. Use `ToMatrix()` and multiply the matrices when
that matters.

## Limitations

- There is no function to turn a quaternion back into pitch, yaw, and roll.
- `Slerp` always takes the short way around; there is no option for the long way.
