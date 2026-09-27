# Recording and playback

Controls can record everything they receive, frame by frame, and play it back
later. The recorded frames replace the real keyboard, mouse, and gamepads, so
the program sees exactly what it saw when recording. That is useful for tests,
for demo modes, and for reproducing a bug someone reported.

```cpp
controls.StartRecording();
// ... play for a while ...
Recording recording = controls.StopRecording();
recording.Save("session.recording");

// later, or in another run of the program:
Result<Recording> loaded = Recording::Load("session.recording");
if (loaded)
{
    controls.Play(*loaded);
}
```

## What is recorded

A `Recording` is a list of `RecordedFrame`s:

| Member | What it holds |
|---|---|
| `DeltaSeconds` | The seconds the frame took |
| `Events` | Every event the controls took in before the frame started |
| `Gamepads` | Every gamepad as it was read, before the dead zone |

Keys and mouse buttons already held when recording starts are recorded as
pressed in the first frame, so playback starts from the same state.

Events a view already used are left out, as they are when reading actions, so
the recording holds what the game itself reacted to.

## Playing back

While playing, the controls ignore the real keyboard, mouse, and gamepads, and
each frame takes the next recorded frame instead. `controls.IsPlaying()` is
true until the frame after the last recorded one, when the controls go back to
the real input with nothing held.

To repeat a session exactly, the game has to advance by the same amounts as
when it was recorded. `controls.FrameSeconds()` gives the recorded frame's
seconds while playing, and the real ones otherwise, so a game that moves by
`FrameSeconds()` instead of the window's delta replays the same way:

```cpp
window.OnFrame = [&](float) {
    float seconds = controls.FrameSeconds();
    player.Position += controls.Axis("Move") * speed * seconds;
};
```

`controls.StopPlaying()` ends playback early.

## Tests without a gamepad

A `Recording` can be built in code, which is how easyforge's own tests check
gamepad bindings without a controller:

```cpp
Recording pressSouth;
RecordedFrame frame;
frame.Gamepads[0].Connected = true;
frame.Gamepads[0].Buttons[static_cast<std::size_t>(GamepadButton::South)] = true;
pressSouth.Frames.push_back(frame);

controls.Play(pressSouth);
controls.NextFrame();
// controls.Pressed("Jump") is now true for an action bound to GamepadButton::South
```

## The file

`Save` writes a small binary file that keeps every number exactly, so a
recording played back from a file matches the one made in memory.
`Encode()` and `Recording::Decode(bytes)` do the same in memory. A damaged or
cut short file is refused with an error, and a file from a different version of
the format says which version it is.

## Limitations

- Recordings hold input only. Anything else the game depends on, such as random
  numbers or time of day, has to be made repeatable separately, for example by
  seeding `Random` with the same number.
- Playback feeds the controls, not the window, so an interface from `ui` does
  not see the recorded events.
