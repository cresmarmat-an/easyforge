# Stepping

```cpp
Physics2D physics = Physics2D::New({ .Gravity = { 0, -9.8f } });
Body2D ball = physics.AddCircle({ .Position = { 0, 10 } });

window.OnFrame = [&](float deltaSeconds) {
    physics.Update(deltaSeconds);
};

// when drawing
canvas.Circle(ToScreen(ball.SmoothPosition()), 0.5f * pixelsPerMetre, { .Color = Color::White });
```

## Update and smooth drawing

`Update(deltaSeconds)` moves the world on by the time since the last frame. It
runs as many steps of `StepSeconds` (1/60 of a second unless set) as fit, keeps
what is left for the next frame, and returns how many steps it ran. Steps of one
length keep the world stable and repeatable whatever the frame rate.

Frames and steps rarely line up: on a 144 hertz screen some frames run no step
and others run one, and a body drawn at `Position` would hop. Draw it at
`SmoothPosition()` and `SmoothRotation()` instead, or turn its corners with
`SmoothWorldPoint(local)`. They blend the body's places at the last two steps by
how far the clock is into the next step, so it moves the same distance every
frame. That puts what is drawn up to one step behind the simulation, which at
1/60 of a second is not noticeable.

Game rules read `Position`, `Rotation`, and `Velocity`, which are exact. Setting
`Position` or `Rotation` moves the smooth place too, so a body put somewhere new
appears there at once instead of sliding across.

After a long pause, such as while a window is being dragged, one `Update` runs
at most `MaximumStepsPerUpdate` steps (8 unless set) and lets the rest of the
time go, so the world slows down for a moment instead of the program spending
longer and longer catching up. `StepSeconds` and `MaximumStepsPerUpdate` are
settings in braces and properties, like every other setting.

`Window` already hands `OnFrame` steady frame times, rounded to whole refreshes
of the screen, so `Update` gets the same number of steps on frames that look
the same.

## Single steps

`Step(seconds)` moves the world on by exactly one step, for programs that keep
time themselves, such as a server running a fixed number of steps a second or a
test. It finds new contacts, works out every contact and joint, moves the
bodies, and then reports the touches it found. After `Step`, a body's smooth
place is its place.

```cpp
for (int step = 0; step < 120; ++step)
{
    physics.Step(1.0f / 60.0f);
}
```

`Substeps` cuts each step into that many passes; four is the default. More make
tall stacks, long chains, and heavy loads stiffer, and steps slower. `Gravity`
and `Substeps` can be changed at any time.

## Units

Positions are in metres, masses in kilograms, and time in seconds, and the
solver's tolerances are set for that: bodies from about 5 centimetres to about
50 metres across behave best. A game measured in pixels converts at the edges,
for example 50 pixels to a metre when drawing, rather than putting pixels into
the world.

## The same on every run

The same bodies, made in the same order, stepped the same way, give the same
result every time the program runs, to the last bit. `Update` keeps that: the
same frame times give the same steps. Smooth positions only read the world and
never change it. Nothing in a step depends
on the clock, threads, or where things happen to be in memory. That allows
replays from recorded input alone, and multiplayer games that send only inputs.

The result can differ between different builds of a program or different kinds
of processor, since compilers may order floating point arithmetic differently.

## How the solver works

Contacts and joints are solved as very stiff, heavily damped springs rather
than as rigid rules. In each substep the bodies' velocities are first worked out
with a push toward the right positions, then the bodies move, and then the
velocities are worked out again without the push, so the correction leaves no
speed behind. Each contact and joint starts from the force it needed last time,
which is most of the answer when things rest. Shapes may overlap by half a
centimetre before they are pushed apart, which keeps resting contacts from
jittering, and overlapping bodies are pushed apart at no more than 3 metres a
second, so a body pushed deep into another comes out gently instead of shooting
away.

## Limitations

- A very fast, small body can pass through a thin wall within one step.
- Bodies never sleep: every body costs a little time every step, resting or not.
- Very different masses resting on each other, such as a 1000 kilogram block on
  a 1 kilogram box, sink in a little.
