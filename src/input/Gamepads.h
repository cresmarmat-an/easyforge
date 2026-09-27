#pragma once

// What each platform folder provides: reading gamepads, which needs no window.

#include <array>

#include <easyforge/input/Gamepad.h>

namespace easyforge::internal
{
    // Every gamepad slot as it is now, sticks from -1 to 1 without any dead zone.
    // Reading twice within a millisecond returns the same result, so several
    // Controls can read in the same frame cheaply.
    std::array<GamepadState, MaximumGamepads> ReadGamepads();

    // Sets both rumble motors of one gamepad, from 0 to 1. Zero stops them.
    void SetGamepadRumble(int index, float low, float high);
}
