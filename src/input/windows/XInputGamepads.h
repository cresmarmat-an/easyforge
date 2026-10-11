#pragma once

#include <easyforge/input/Export.h>
#include <easyforge/input/Gamepad.h>

// The library's own tests use what is marked for export here, so it stays
// reachable when the library is a DLL.
namespace easyforge::internal
{
    // The parts of XINPUT_GAMEPAD that easyforge reads, so the conversion can be
    // tested without a controller or the XInput headers.
    struct XInputReading
    {
        unsigned short Buttons = 0;
        unsigned char LeftTrigger = 0;
        unsigned char RightTrigger = 0;
        short LeftStickX = 0;
        short LeftStickY = 0;
        short RightStickX = 0;
        short RightStickY = 0;
    };

    EASYFORGE_INPUT_EXPORT GamepadState GamepadFromXInput(const XInputReading& reading);
}
