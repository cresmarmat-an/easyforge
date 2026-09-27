#include <easyforge/input/Gamepad.h>

namespace easyforge
{
    float GamepadState::Value(GamepadAxis axis) const
    {
        switch (axis)
        {
        case GamepadAxis::LeftStickX: return LeftStick.X;
        case GamepadAxis::LeftStickY: return LeftStick.Y;
        case GamepadAxis::RightStickX: return RightStick.X;
        case GamepadAxis::RightStickY: return RightStick.Y;
        case GamepadAxis::LeftTrigger: return LeftTrigger;
        case GamepadAxis::RightTrigger: return RightTrigger;
        }
        return 0.0f;
    }
}
