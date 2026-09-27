#include "../Gamepads.h"

namespace easyforge::internal
{
    // Gamepads on other platforms arrive with those platforms.
    std::array<GamepadState, MaximumGamepads> ReadGamepads()
    {
        return {};
    }

    void SetGamepadRumble(int, float, float)
    {
    }
}
