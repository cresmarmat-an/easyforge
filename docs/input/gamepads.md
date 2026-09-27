# Gamepads

Gamepads need no window, so `input` reads them itself, once a frame. On
Windows it reads Xbox controllers, and controllers that act like one, through
XInput, up to four at once.

```cpp
controls.Bind("Jump", GamepadButton::South);
controls.Bind("Move", Stick::Left);
controls.Bind("Accelerate", GamepadButton::RightTrigger);

float throttle = controls.Value("Accelerate");    // 0 to 1
Vector2 move = controls.Axis("Move");             // Y positive for up
```

## Buttons

The four face buttons are named by where they are, since each maker labels them
differently:

| GamepadButton | Xbox | PlayStation |
|---|---|---|
| `South` | A | Cross |
| `East` | B | Circle |
| `West` | X | Square |
| `North` | Y | Triangle |
| `LeftShoulder`, `RightShoulder` | LB, RB | L1, R1 |
| `LeftTrigger`, `RightTrigger` | LT, RT | L2, R2 |
| `Back`, `Start` | View, Menu | Share, Options |
| `LeftStick`, `RightStick` | Pressing a stick in | L3, R3 |
| `Up`, `Down`, `Left`, `Right` | The directional pad | The directional pad |

The triggers count as held past a quarter of their travel. Bound as a button,
their `Value` still gives how far they are pulled, so one binding serves both
"is the trigger down" and "how hard".

## Sticks and the dead zone

Sticks rarely rest exactly in the middle. Movement inside the dead zone, 0.2 of
the stick's travel by default, counts as none, and the rest of the travel is
spread from 0 to 1, so the smallest push past the dead zone still starts from
zero:

```cpp
controls.DeadZone = 0.15f;
```

The dead zone is round, so it does not make diagonals snap to the axes. Stick
values are from -1 to 1 on each side, with Y positive for up.

## Reading gamepads directly

```cpp
std::array<GamepadState, MaximumGamepads> gamepads = controls.Gamepads();
if (gamepads[0].Connected && gamepads[0].Held(GamepadButton::Start))
{
    Pause();
}
```

| GamepadState member | Meaning |
|---|---|
| `Connected` | Whether a gamepad is in this slot |
| `Held(button)` | Whether a button is down |
| `LeftStick`, `RightStick` | After the dead zone |
| `LeftTrigger`, `RightTrigger` | From 0 to 1 |
| `Value(axis)` | One `GamepadAxis` as a number |

A gamepad that is plugged in is noticed within about a second.

## One player per gamepad

By default the controls read every gamepad together, as if they were one, which
suits a single-player game. For several players, make one `Controls` for each
player and give each its gamepad:

```cpp
Controls playerOne = Controls::New(window, { .Gamepad = 0 });
Controls playerTwo = Controls::New(window, { .Gamepad = 1 });
playerOne.Bind("Jump", { Key::Space, GamepadButton::South });
playerTwo.Bind("Jump", { Key::Enter, GamepadButton::South });
```

## Rumble

```cpp
controls.Rumble(0.4f, 0.8f, 0.25f);    // low motor, high motor, seconds
```

The low frequency motor is the heavy rumble in the left grip; the high
frequency one is the lighter buzz on the right. Strengths are from 0 to 1.
Rumble stops by itself after the given time, and when the controls are
destroyed. It goes to the gamepad the controls read, or all of them with
`AnyGamepad`.

## Limitations

- Windows only reads controllers through XInput, which covers Xbox controllers
  and most controllers for PC. PlayStation and Switch controllers need a program
  such as Steam Input to appear as one.
- No names or pictures of the connected controller, and no battery level.
- No motion sensors, touchpads, lights, or trigger rumble.
- The Xbox button in the middle of the controller is kept by Windows.
