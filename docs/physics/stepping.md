# Stepping

```cpp
Physics2D physics = Physics2D::New({ .Gravity = { 0, -9.8f }, .Substeps = 4 });

constexpr float StepSeconds = 1.0f / 60.0f;
float waiting = 0.0f;

window.OnFrame = [&](float deltaSeconds) {
    // Steps of the same length, however long the frames are.
    waiting += deltaSeconds;
    while (waiting >= StepSeconds)
    {
        physics.Step(StepSeconds);
        waiting -= StepSeconds;
    }
};
```

## Steps

`Step(seconds)` moves the world on: it finds new contacts, works out every
contact and joint, moves the bodies, and then reports the touches it found.
Steps of the same length every time keep the world stable and repeatable, so a
program steps by a fixed time, as above, rather than by each frame's length.
1/60 of a second suits most games.

`Substeps` cuts each step into that many passes; four is the default. More make
tall stacks, long chains, and heavy loads stiffer, and steps slower. Both
`Gravity` and `Substeps` can be changed at any time.

## Units

Positions are in metres, masses in kilograms, and time in seconds, and the
solver's tolerances are set for that: bodies from about 5 centimetres to about
50 metres across behave best. A game measured in pixels converts at the edges,
for example 50 pixels to a metre when drawing, rather than putting pixels into
the world.

## The same on every run

The same bodies, made in the same order, stepped the same way, give the same
result every time the program runs, to the last bit. Nothing in a step depends
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
