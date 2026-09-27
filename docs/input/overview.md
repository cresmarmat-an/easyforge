# input

`input` will turn keys, mouse buttons, touches, and gamepad sticks into named
actions such as "Jump" and "Move", so a program asks what the player wants to do
instead of which key was pressed.

> [!NOTE] Not available yet
> `input` is step 4 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- Bind several keys and buttons to one action, and change the bindings while the
  program runs.
- Report whether an action was just pressed, is held, or was just released, and
  read two-dimensional movement from sticks or from four keys.
- Read gamepads directly: XInput on Windows, then evdev, GameController, the
  Android input queue, and the browser's Gamepad API.
- Skip events the interface already used, so typing in a text field does not
  make a character jump.
- Record events and play them back, to repeat a session exactly in a test.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#input).
