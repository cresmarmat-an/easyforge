#pragma once

#include <array>
#include <cstddef>

#include <easyforge/core/Vector.h>

namespace easyforge
{
    // A button on a gamepad. The four face buttons are named by where they are,
    // since each maker labels them differently: South is A on an Xbox controller
    // and the cross on a PlayStation controller.
    enum class GamepadButton
    {
        South,
        East,
        West,
        North,
        LeftShoulder,
        RightShoulder,

        // The triggers count as pressed past a quarter of their travel. Their
        // full range is in GamepadState and GamepadAxis.
        LeftTrigger,
        RightTrigger,

        // View and Menu on an Xbox controller.
        Back,
        Start,

        // Pressing the sticks in.
        LeftStick,
        RightStick,

        // The directional pad.
        Up,
        Down,
        Left,
        Right,

        Count,
    };

    // One direction of a stick or trigger, for bindings that want a single number.
    enum class GamepadAxis
    {
        LeftStickX,
        LeftStickY,
        RightStickX,
        RightStickY,
        LeftTrigger,
        RightTrigger,
    };

    enum class Stick
    {
        Left,
        Right,
    };

    // Everything a gamepad reports at one moment.
    struct GamepadState
    {
        bool Connected = false;
        std::array<bool, static_cast<std::size_t>(GamepadButton::Count)> Buttons {};

        // From -1 to 1 on each side, with Y positive for up, after the dead zone.
        Vector2 LeftStick;
        Vector2 RightStick;

        // From 0 to 1.
        float LeftTrigger = 0.0f;
        float RightTrigger = 0.0f;

        bool Held(GamepadButton button) const
        {
            std::size_t index = static_cast<std::size_t>(button);
            return index < Buttons.size() && Buttons[index];
        }

        float Value(GamepadAxis axis) const;

        bool operator==(const GamepadState&) const = default;
    };

    // How many gamepads easyforge reads at once.
    inline constexpr int MaximumGamepads = 4;
}
