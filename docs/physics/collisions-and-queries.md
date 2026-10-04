# Collisions and queries

```cpp
Body2D ball = physics.AddCircle({ .Position = { 0, 8 }, .Radius = 0.5f });
ball.OnTouch = [&](const Contact2D& contact) {
    if (contact.Speed > 2.0f)
    {
        mixer.Play(thud, { .Volume = contact.Speed / 10.0f });
    }
};

Body2D coin = physics.AddCircle({ .Position = { 3, 1 }, .Radius = 0.3f, .Type = BodyType::Static, .Sensor = true });
coin.OnTouch = [coin, player](const Contact2D& contact) {
    if (contact.Other == player)
    {
        coin.Remove();
    }
};

RayHit2D below = physics.CastRay(player.Position, { 0, -1 }, 1.1f);
bool onGround = static_cast<bool>(below);
```

## Touches

`OnTouch` is called when another body starts touching a body, and `OnSeparate`
when it stops. Both are called after the step that found them, on the thread
that called `Step`, in the same order every run. A handler may change the world:
add, move, or remove bodies.

| Member of `Contact2D` | Gives |
|---|---|
| `Other` | the other body; it tests as false if the other body was removed |
| `Point` | where they touch, in the world |
| `Normal` | the direction from this body toward the other |
| `Speed` | how fast they were coming together, in metres a second; 0 for separations |
| `Sensor` | whether either body is a sensor |

`Speed` makes impact sounds easy: louder for harder hits, and none for a body
merely settling.

## Sensors

A body made with `Sensor = true` collides with nothing: other bodies pass
through it, and it reports with `OnTouch` when one starts overlapping it and
`OnSeparate` when it stops. Sensors make trigger areas, pickups, and finish
lines. A static sensor still reports dynamic and kinematic bodies.

## Layers

Each body is on the layers whose bits are set in `Layer`, and collides with the
layers in `CollidesWith`. Two bodies collide only when each is on a layer the
other collides with:

```cpp
constexpr std::uint32_t Players = 1 << 1;
constexpr std::uint32_t Ghosts = 1 << 2;

physics.AddCircle({ .Radius = 0.4f, .Layer = Players, .CollidesWith = AllLayers & ~Players });   // players pass through each other
physics.AddBox({ .Layer = Ghosts, .CollidesWith = 0 });                                         // touches nothing
```

Layers can be changed later through the properties of the same names, and take
effect at once. Bodies joined by a joint do not collide with each other unless
the joint's `Collide` is set.

## Rays and circle casts

```cpp
RayHit2D hit = physics.CastRay(eye, direction, 30.0f);
RayHit2D landing = physics.CastCircle(position, 0.5f, { 0, -1 }, 10.0f);
if (hit)
{
    Log("{} metres away", hit.Distance);
}
```

`CastRay(origin, direction, distance, layers)` finds the first body along a
ray, and `CastCircle(center, radius, direction, distance, layers)` the first
body a circle moving that way would meet, which is how to check whether a
character fits through a gap or where it would land. The direction need not be
of length 1.

A `RayHit2D` tests as false when nothing was hit. Otherwise `Body` is what was
hit, `Point` where on its surface, `Normal` the surface's direction there, and
`Distance` how far along the ray, or how far the circle moved. A ray that
starts inside a body does not hit that body, and sensors are never hit.

## Finding bodies by place

`BodiesAt(point, layers)` gives the bodies whose shape contains a point, such as
the one under the mouse, and `BodiesIn(area, layers)` those overlapping an area,
given as a `Rectangle` whose X and Y are its lowest corner. Both list bodies in
the same order every run.

## Limitations

- A touch is reported when bodies come within half a centimetre of each other,
  and a contact is prepared for the solver from two centimetres, so a contact
  can begin a step before the shapes meet.
- Casting shapes other than circles is not supported yet.
- Each pair of bodies touches at one or two points; a body resting on several
  others touches each of them that way.
