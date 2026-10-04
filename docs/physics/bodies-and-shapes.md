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

`Mass()` and `Inertia()` give a dynamic body's mass in kilograms and its
inertia about its center of mass, `Center()` the center of mass in the world,
`Bounds()` the smallest box around it, and `WorldPoint` and `LocalPoint` turn
points between the world and the body's own coordinates.

## Removing bodies

`body.Remove()` takes a body out of its world with the joints attached to it.
Bodies it was touching get their `OnSeparate` before Remove returns. Every
handle to the body then tests as false and does nothing, even when a new body
reuses its place. `physics.Bodies()` lists the bodies, and `BodyCount()`
counts them.

## Limitations

- A dynamic body with no area, or a density of 0, is given one kilogram.
- A shape cannot be changed once the body is made; remove it and add another.
