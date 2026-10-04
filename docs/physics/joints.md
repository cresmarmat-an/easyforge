# Joints

```cpp
Body2D post = physics.AddBox({ .Position = { 0, 10 }, .Size = { 0.2f, 0.2f }, .Type = BodyType::Static });
Body2D bob = physics.AddCircle({ .Position = { 3, 10 }, .Radius = 0.3f });
Joint2D rope = physics.AddDistanceJoint({ .First = post, .Second = bob, .FirstAnchor = { 0, 10 }, .SecondAnchor = { 3, 10 } });

Body2D wheel = physics.AddCircle({ .Position = { 0, 1 }, .Radius = 1.0f });
Joint2D axle = physics.AddHingeJoint({
    .First = chassis,
    .Second = wheel,
    .Anchor = { 0, 1 },
    .Motor = true,
    .MotorSpeed = -4.0f,
    .MaximumMotorTorque = 200.0f,
});
axle.MotorSpeed = 0.0f;   // let it roll
```

A joint keeps two bodies in some relation. Anchors are points in the world, as
the bodies are when the joint is made; angles are counted from how the bodies
are turned then. Bodies joined together do not collide with each other unless
the joint's `Collide` is set.

## The kinds

| Add | Keeps | Settings |
|---|---|---|
| `AddDistanceJoint` | two points at a distance: a rod, or with `Stiffness`, a spring | `FirstAnchor`, `SecondAnchor`, `Length` (negative keeps the distance now), `MinimumLength`, `MaximumLength`, `Stiffness`, `Damping` |
| `AddHingeJoint` | two bodies pinned at a point they turn around | `Anchor`, `Limit`, `LowerAngle`, `UpperAngle`, `Motor`, `MotorSpeed`, `MaximumMotorTorque` |
| `AddSliderJoint` | the second body sliding along an axis of the first without turning | `Anchor`, `Axis`, `Limit`, `Lower`, `Upper`, `Motor`, `MotorSpeed`, `MaximumMotorForce` |
| `AddWeldJoint` | two bodies held together as one | `Anchor`, `Stiffness`, `Damping` |
| `AddMotorJoint` | the second body pulled toward a place and angle relative to the first | `LinearOffset`, `AngularOffset`, `MaximumForce`, `MaximumTorque`, `Correction` |

Every kind also takes `First`, `Second`, and `Collide`. A joint needs two
different bodies of the same world; otherwise the handle tests as false.

## Springs and give

`Stiffness` is in hertz: how many times a second a spring would swing back and
forth. `Damping` is how quickly the swinging dies away: 0 never, 1 just without
overshooting. A distance joint with stiffness is a spring that rests at
`Length` and, between `MinimumLength` and `MaximumLength`, stretches and
squashes; with 0 stiffness it is a rigid rod. A weld joint with stiffness bends
under load and springs back.

## Limits and motors

A hinge's limit keeps the angle between the bodies within `LowerAngle` and
`UpperAngle`, in radians from where they started; a slider's keeps the distance
moved along the axis within `Lower` and `Upper`, in metres. A motor turns a
hinge, or drives a slider, at `MotorSpeed` with at most the maximum torque or
force; a motor at speed 0 holds the joint still against anything weaker. The
motor speed can be changed while the world runs.

## Motor joints

```cpp
Joint2D hand = physics.AddMotorJoint({ .First = ground, .Second = crate, .MaximumForce = 500.0f, .MaximumTorque = 100.0f });
hand.LinearOffset = mouseInWorld - ground.Position.Get();
```

A motor joint pulls the second body's origin toward `LinearOffset`, given in the
first body's coordinates, and its angle toward `AngularOffset`, with at most the
given force and torque, covering `Correction` of the remaining way each
substep. Joined to a static body, it drags a body with the mouse, or moves a
character, while still letting it be pushed by what it meets.

## Joint handles

`Joint2D` is a handle. `First()` and `Second()` give the bodies, `Angle()` a
hinge's angle, and `Translation()` how far a slider has moved. `MotorSpeed`,
`Length`, `LinearOffset`, and `AngularOffset` can be changed while the world
runs; each kind uses its own and ignores the rest. `Remove()` takes the joint
out, and removing either body removes it too; then every handle to it tests as
false.

## Limitations

- There are no pulleys, gears, ropes, or wheel joints yet; build them from these.
- Very long chains of joints, or heavy bodies hanging from light ones, stretch
  a little; more substeps make them stiffer.
