# physics

`physics` simulates bodies that fall, collide, rest on each other, and are
joined together. 2D comes first; 3D follows the same design in stage 6.

```cpp
#include <easyforge/physics.h>

using namespace easyforge;

int main()
{
    Physics2D physics = Physics2D::New({ .Gravity = { 0, -9.8f } });

    Body2D ground = physics.AddBox({ .Size = { 50, 1 }, .Type = BodyType::Static });
    Body2D crate = physics.AddBox({ .Position = { 0, 10 }, .Size = { 1, 1 } });

    crate.OnTouch = [](const Contact2D& contact) {
        Log("landed at {} metres a second", contact.Speed);
    };

    for (int step = 0; step < 120; ++step)
    {
        physics.Step(1.0f / 60.0f);
    }

    Vector2 position = crate.Position;   // resting on the ground, at y = 1
    RayHit2D hit = physics.CastRay({ 0, 20 }, { 0, -1 }, 50);
}
```

Link `easyforge::physics`. It needs only `core`.

## Pages

- [Bodies and shapes](bodies-and-shapes.md): circles, boxes, capsules, and
  polygons; static, kinematic, and dynamic bodies; moving them by hand, with
  forces, and with impulses.
- [Collisions and queries](collisions-and-queries.md): touch events, sensors,
  collision layers, rays, circle casts, and finding bodies by place.
- [Joints](joints.md): rods and springs, hinges, sliders, welds, and motor
  joints.
- [Stepping](stepping.md): units, steps and substeps, staying the same on every
  run, and how the solver works.

[Example 08](https://github.com/cresmarmat-an/easyforge-examples/tree/main/08-physics)
stacks boxes and drops more where you click, in two worlds that stay the same
to the last bit.

## How it works

Each body has one shape, held as a convex outline with rounded corners: a
circle is a point with a radius, a capsule two points with a radius. A tree of
bounding boxes finds the bodies near each other, contacts are worked out
exactly for every pair close enough to touch, and a solver turns contacts and
joints into velocities. The solver treats every constraint as a very stiff,
heavily damped spring, and cuts each step into substeps, so stacks settle and
stay still. Everything is written for easyforge.

Nothing in a step depends on the time, the machine's speed, threads, or the
order memory happens to be in, so the same steps with the same bodies give the
same result every time the program runs.

## Limitations

- 2D only until stage 6.
- One shape per body; join bodies with a weld joint for shapes that need more.
- Fast, small bodies can pass through thin walls: there is no continuous
  collision yet. Keep walls thicker than a body moves in one step.
- Bodies never sleep, so a world of thousands of resting bodies still costs
  time every step.
- A world is used from one thread at a time.
