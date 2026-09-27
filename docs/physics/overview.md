# physics

`physics` will simulate bodies that fall, collide, stack, and are joined
together, first in 2D and later in 3D.

> [!NOTE] Not available yet
> `physics` is step 10 of stage 1, for 2D. 3D follows in stage 6. This page will
> describe how to use it once it exists.

What it is planned to do:

- `Physics2D::New({ .Gravity = ... })`, then `physics.AddBox(...)` and friends
  to add circles, boxes, capsules, and convex polygons.
- Stacks that stay still, from a solver with warm starting and substeps.
- Distance, hinge, slider, weld, and motor joints.
- Ray casts, shape casts, sensors, and collision layers.
- The same results on every run for the same inputs.
- `Physics3D` with convex hulls, triangle meshes, height fields, and a character
  controller.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#physics).
