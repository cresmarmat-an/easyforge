# Matrices

`Matrix4` moves, turns, scales, and projects points in 3D. `Matrix3` does the
same without moving. Both start as the identity matrix, which changes nothing.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Matrix4 model = Matrix4::Translation({ 0.0f, 1.0f, 0.0f }) *
                Matrix4::Rotation(Quaternion::FromAngles(0.0f, Radians(30.0f), 0.0f)) *
                Matrix4::Scale({ 2.0f, 2.0f, 2.0f });

Matrix4 view = Matrix4::LookAt({ 0.0f, 2.0f, 6.0f }, { 0.0f, 1.0f, 0.0f });
Matrix4 projection = Matrix4::Perspective(Radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

Matrix4 everything = projection * view * model;
Vector3 onScreen = everything.TransformPoint({ 0.5f, 0.5f, 0.5f });
```

## How they are arranged

A matrix is stored as columns: `matrix.Columns[3]` is the fourth column of a
`Matrix4`, which holds the position. Vectors are columns too, and the matrix
goes on the left. So `second * first` applies `first`, then `second`, and the
example above scales, then turns, then moves.

## Conventions

easyforge uses one set of conventions everywhere, the same as Direct3D 12,
Vulkan, Metal, and WebGPU expect:

- Right-handed coordinates with Y up. A camera looks down -Z.
- After a projection, depth goes from 0 at the near plane to 1 at the far plane.
- After a projection, Y points up. On Vulkan, whose screen Y points down,
  `graphics` flips it, so your matrices stay the same on every backend.

## Making matrices

| Function | Result |
|---|---|
| `Matrix4::Identity()`, `Matrix4 {}` | Changes nothing |
| `Matrix4::Translation(offset)` | Moves by `offset` |
| `Matrix4::Scale(scale)` | Scales each axis |
| `Matrix4::Rotation(quaternion)` | Turns by a [quaternion](rotations-and-transforms.md) |
| `Matrix4::Perspective(fieldOfViewY, aspectRatio, nearDistance, farDistance)` | A perspective projection. The field of view is vertical, in radians |
| `Matrix4::Orthographic(left, right, bottom, top, nearDistance, farDistance)` | A projection without perspective, for 2D and for shadows |
| `Matrix4::LookAt(eye, target, up)` | A view matrix: the world as seen from `eye` looking at `target`. `up` is +Y unless given |
| `Matrix3::Identity()`, `Matrix3::Scale(scale)`, `Matrix3::Rotation(quaternion)` | The 3 by 3 versions |
| `Matrix3::FromMatrix4(matrix)` | The top-left 3 by 3 part: rotation and scale without the position |

For screen coordinates with Y growing downward, swap `bottom` and `top`:
`Matrix4::Orthographic(0, width, height, 0, 0, 1)` puts (0, 0) at the top left.

## Using matrices

| Written | Result |
|---|---|
| `first * second` | Combines two matrices |
| `matrix * Vector4 {...}` | Applies the matrix to a four-component vector |
| `matrix * Vector3 {...}` | For a `Matrix3` |
| `matrix.TransformPoint(point)` | Applies the whole matrix, position included. After a projection, the result is divided by W |
| `matrix.TransformDirection(direction)` | Applies it without the position part, for directions |
| `Transpose(matrix)` | Rows become columns |
| `Determinant(matrix)` | How much the matrix scales volume. Zero means it flattens space |
| `Inverse(matrix)` | The matrix that undoes this one |
| `NearlyEqual(first, second, tolerance)` | True when every number is within `tolerance` |

A matrix whose determinant is zero, such as a scale of zero along one axis, has
no inverse. For those, `Inverse` returns the identity matrix; check
`Determinant(matrix) != 0.0f` first when that can happen.

## Limitations

- Numbers are always `float`.
- `Inverse` works on any matrix and does not take shortcuts for the common case
  of rotation plus position.
- There is no function yet to split a matrix back into position, rotation, and
  scale.
