# Positional sound

```cpp
Mixer mixer = Mixer::New();

PlayingSound fire = mixer.Play(crackle, {
    .Loop = true,
    .Position = Vector3 { 10, 0, -4 },
    .MinimumDistance = 2.0f,
    .MaximumDistance = 40.0f,
});

window.OnFrame = [&](float deltaSeconds) {
    mixer.Listener.Position = camera.Position;
    mixer.Listener.Forward = camera.Forward;
    fire.Position = fireplace.Position;   // if it moves
};
```

A sound with a `Position` is heard from a place in the world: from the side it
is on, and quieter the farther it is from the mixer's listener. Positions are
in whatever unit the program's world uses; metres are a good choice.

## The listener

`mixer.Listener` is where positional sounds are heard from, usually the camera
or the player. Its `Position`, `Forward`, and `Up` are properties, and as
everywhere in easyforge it faces -Z with +Y up until turned. Only the direction
to the left and right matters for how a sound is panned, so a listener needs
`Forward` and `Up` set only when it turns.

## Distance

Within `MinimumDistance` of the listener a sound plays at its full volume.
Farther away it gets quieter the way sound does in the open: half as loud at
twice the minimum distance, a quarter at four times. Over the last fifth of the
way to `MaximumDistance` it fades out, and past the maximum it is silent but
keeps playing, so it is heard again when the listener comes near.

| Setting | Default | Does |
|---|---|---|
| `MinimumDistance` | `1` | the distance within which the sound is at full volume |
| `MaximumDistance` | `50` | the distance past which it is silent |

A loud sound such as an explosion wants a larger minimum distance than a quiet
one such as a footstep.

## Direction

A sound straight ahead or behind reaches both speakers equally; one directly to
the side comes from that side's speaker only, with equal power in between. A
stereo sound with a position is heard as one point, its two channels mixed. A
sound without a place becomes positional when its `Position` is first set.

## Limitations

- Sounds ahead and behind sound the same; there is no filtering of sounds
  behind the listener or of height yet.
- There is no Doppler shift for moving sounds; change `Pitch` to make one.
- Walls do not block sound by themselves; set `LowPass` on a sound or a bus
  when something is in the way.
