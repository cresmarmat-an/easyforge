# Bodies and shapes

```cpp
Physics2D physics = Physics2D::New();

Body2D floor = physics.AddBox({ .Size = { 40, 1 }, .Type = BodyType::Static });
Body2D ball = physics.AddCircle({ .Position = { 0, 5 }, .Radius = 0.5f, .Restitution = 0.6f });
Body2D pill = physics.AddCapsule({ .Position = { 2, 5 }, .Length = 1.0f, .Radius = 0.25f });
Body2D rock = physics.AddPolygon({
    .Position = { -2, 5 },
    .Points = { { -0.5f, -0.4f }, { 0.6f, -0.3f }, { 0.4f, 0.5f }, { -0.3f, 0.6f } },
    .Rounding = 0.05f,
});
```

Positions are in metres, angles in radians counterclockwise, and Y points up.
Each body has one shape around its position.

## Shapes

| Add | Shape | Its settings |
|---|---|---|
| `AddBox` | a rectangle around the position | `Size`, `Rounding` |
| `AddCircle` | a circle | `Radius` |
| `AddCapsule` | a rectangle with round ends along the body's X axis | `Length` between the ends' centers, `Radius` |
| `AddPolygon` | the convex outline of the points | `Points`, `Rounding` |

`Rounding` rounds a box's or polygon's corners by that many metres, outside its
size, which helps bodies slide over each other's edges. A polygon keeps only the
outline of its points: points inside it or along a straight edge are left out,
and at most eight corners are kept. Points that enclose no area make no body,
and the handle tests as false.

`Shape()`, `Size()`, `Radius()`, `Length()`, and `Points()` describe a body's
shape, which never changes.

## The settings every body has

After the shape's own settings, every Add takes the same ones:

| Setting | Default | Does |
|---|---|---|
| `Position`, `Rotation` | the origin, 0 | where the body starts |
| `Type` | `Dynamic` | `Static` never moves; `Kinematic` moves at its velocity and pushes but is never pushed; `Dynamic` moves by gravity, forces, and collisions |
| `Velocity`, `AngularVelocity` | 0 | metres a second, radians a second |
| `Density` | 1 | kilograms a square metre, which sets the mass |
| `Friction` | 0.6 | 0 slides like ice, 1 grips like rubber; two bodies use the root of both |
| `Restitution` | 0 | 0 stops dead, 1 bounces back as fast; two bodies use the larger |
| `LinearDamping`, `AngularDamping` | 0 | slow the body over time, as air does |
| `GravityScale` | 1 | how much gravity the body feels; 0 floats |
| `FixedRotation` | false | the body never turns |
| `Sensor` | false | the body reports what touches it but collides with nothing |
| `Layer`, `CollidesWith` | 1, every layer | [collision layers](collisions-and-queries.md#layers) |
| `Bullet` | false | swept against dynamic bodies too, not only walls; see [fast bodies](#fast-bodies) |
| `AllowSleep` | true | the body may sleep once it has been still; see [sleeping](#sleeping) |

Everything except the shape and the density can be changed later through the
body's properties of the same names.

## Moving bodies

```cpp
crate.Velocity = Vector2 { 3, 0 };
crate.ApplyImpulse({ 0, 5 });                         // a kick, at once
crate.ApplyForce({ 20, 0 }, crate.WorldPoint({ 0.5f, 0.5f }));   // over the next step, at a corner
crate.ApplyTorque(2.0f);
crate.Position = Vector2 { 0, 10 };                   // straight there
```

A force acts over the next step and is then cleared, so a steady push is applied
every step. An impulse changes the velocity at once. Both act at the center of
mass unless given a point in the world, where they also turn the body. Setting
`Position` or `Rotation` moves the body straight there, without passing what is
in between. Forces, impulses, and velocities do nothing to static bodies.

Pushing or moving a sleeping body wakes it, so none of this needs a
[wake](#sleeping) first.

`Mass()` and `Inertia()` give a dynamic body's mass in kilograms and its
inertia about its center of mass, `Center()` the center of mass in the world,
`Bounds()` the smallest box around it, and `WorldPoint` and `LocalPoint` turn
points between the world and the body's own coordinates.

## Sleeping

A dynamic body that has been still for half a second, along with everything it
touches or is joined to, falls asleep: it is not moved or solved until something
wakes it, so a pile of resting bodies costs almost nothing and stays perfectly
still. "Still" means no point of the body moves faster than 5 centimetres a
second, counting its turning as well as its movement.

A sleeping body wakes, with everything that fell asleep with it, when:

- something moving reaches it, or a body is added next to it;
- a force, torque, or impulse is applied to it, or its velocity, position,
  rotation, type, gravity scale, or collision layers are set;
- a joint is added to it, removed from it, or changed;
- the body it rests on is removed or moved, or the world's gravity changes;
- `Wake()` is called.

`IsAwake()` tells whether a body is awake. Static and kinematic bodies are
always awake, and a body resting on a kinematic body that moves stays awake.
`AllowSleep = false` keeps one body, and whatever touches it, awake;
`physics.AllowSleep = false` keeps every body awake. A sleeping body reports no
new touches, since nothing about it changes.

## Fast bodies

A body that moves more than about half its own thickness in one step is swept:
its whole move through the step is checked against static and kinematic
bodies, and it stops where it would first touch one, keeping its velocity, so
the contact in the next step takes it from there. It cannot pass through a
wall however thin the wall or however fast the body. A body already resting on
or sliding along a wall keeps sliding; only a part of it that turns toward the
wall is caught.

A bullet (`Bullet = true`) is also swept against dynamic bodies, where the step
left them, for small fast things such as projectiles. Sweeping costs a little
time for each fast body, so it is not on for every body by default.

A body turns at most an eighth of a turn in each substep, which is about 190
radians a second with the default settings. Faster spinning, such as a thin
rod struck at its end, is slowed to that.

## Removing bodies

`body.Remove()` takes a body out of its world with the joints attached to it.
Bodies it was touching get their `OnSeparate` before Remove returns. Every
handle to the body then tests as false and does nothing, even when a new body
reuses its place. `physics.Bodies()` lists the bodies, and `BodyCount()`
counts them.

## Limitations

- A dynamic body with no area, or a density of 0, is given one kilogram.
- A shape cannot be changed once the body is made; remove it and add another.
