#include "XInputGamepads.h"

#include <easyforge/core/Clock.h>
#include <easyforge/core/Scalar.h>

#include "../Gamepads.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <Xinput.h>

namespace easyforge::internal
{
    namespace
    {
        using GetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        using SetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

        // XInput is loaded when first needed, so a computer without it has no
        // gamepads instead of a program that will not start. Windows 8 and later
        // have version 1.4; version 9.1.0 is the one every Windows has.
        struct XInputLibrary
        {
            GetStateFunction GetState = nullptr;
            SetStateFunction SetState = nullptr;

            XInputLibrary()
            {
                HMODULE module = LoadLibraryW(L"xinput1_4.dll");
                if (!module)
                {
                    module = LoadLibraryW(L"xinput9_1_0.dll");
                }
                if (module)
                {
                    GetState = reinterpret_cast<GetStateFunction>(GetProcAddress(module, "XInputGetState"));
                    SetState = reinterpret_cast<SetStateFunction>(GetProcAddress(module, "XInputSetState"));
                }
            }
        };

        const XInputLibrary& Library()
        {
            static const XInputLibrary library;
            return library;
        }

        float StickValue(short value)
        {
            return Clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
        }
    }

    GamepadState GamepadFromXInput(const XInputReading& reading)
    {
        GamepadState gamepad;
        gamepad.Connected = true;
        auto set = [&](GamepadButton button, bool held) { gamepad.Buttons[static_cast<std::size_t>(button)] = held; };
        set(GamepadButton::South, (reading.Buttons & XINPUT_GAMEPAD_A) != 0);
        set(GamepadButton::East, (reading.Buttons & XINPUT_GAMEPAD_B) != 0);
        set(GamepadButton::West, (reading.Buttons & XINPUT_GAMEPAD_X) != 0);
        set(GamepadButton::North, (reading.Buttons & XINPUT_GAMEPAD_Y) != 0);
        set(GamepadButton::LeftShoulder, (reading.Buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
        set(GamepadButton::RightShoulder, (reading.Buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
        set(GamepadButton::Back, (reading.Buttons & XINPUT_GAMEPAD_BACK) != 0);
        set(GamepadButton::Start, (reading.Buttons & XINPUT_GAMEPAD_START) != 0);
        set(GamepadButton::LeftStick, (reading.Buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
        set(GamepadButton::RightStick, (reading.Buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
        set(GamepadButton::Up, (reading.Buttons & XINPUT_GAMEPAD_DPAD_UP) != 0);
        set(GamepadButton::Down, (reading.Buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
        set(GamepadButton::Left, (reading.Buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
        set(GamepadButton::Right, (reading.Buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);

        gamepad.LeftTrigger = static_cast<float>(reading.LeftTrigger) / 255.0f;
        gamepad.RightTrigger = static_cast<float>(reading.RightTrigger) / 255.0f;
        set(GamepadButton::LeftTrigger, gamepad.LeftTrigger > 0.25f);
        set(GamepadButton::RightTrigger, gamepad.RightTrigger > 0.25f);

        gamepad.LeftStick = { StickValue(reading.LeftStickX), StickValue(reading.LeftStickY) };
        gamepad.RightStick = { StickValue(reading.RightStickX), StickValue(reading.RightStickY) };
        return gamepad;
    }

    std::array<GamepadState, MaximumGamepads> ReadGamepads()
    {
        static std::array<GamepadState, MaximumGamepads> gamepads {};
        static Clock sinceRead;
        static bool readOnce = false;
        // Asking XInput about an empty slot is slow, so empty slots are asked
        // about again only once a second.
        static std::array<bool, MaximumGamepads> foundEmpty {};
        static std::array<Clock, MaximumGamepads> sinceFoundEmpty;

        if (readOnce && sinceRead.Seconds() < 0.001)
        {
            return gamepads;
        }
        readOnce = true;
        sinceRead.Restart();

        const XInputLibrary& library = Library();
        if (!library.GetState)
        {
            return gamepads;
        }
        for (int index = 0; index < MaximumGamepads; ++index)
        {
            std::size_t slot = static_cast<std::size_t>(index);
            GamepadState& gamepad = gamepads[slot];
            if (foundEmpty[slot] && sinceFoundEmpty[slot].Seconds() < 1.0)
            {
                continue;
            }
            XINPUT_STATE state {};
            if (library.GetState(static_cast<DWORD>(index), &state) != ERROR_SUCCESS)
            {
                gamepad = GamepadState {};
                foundEmpty[slot] = true;
                sinceFoundEmpty[slot].Restart();
                continue;
            }
            foundEmpty[slot] = false;
            const XINPUT_GAMEPAD& pad = state.Gamepad;
            gamepad = GamepadFromXInput({ pad.wButtons, pad.bLeftTrigger, pad.bRightTrigger, pad.sThumbLX, pad.sThumbLY,
                pad.sThumbRX, pad.sThumbRY });
        }
        return gamepads;
    }

    void SetGamepadRumble(int index, float low, float high)
    {
        const XInputLibrary& library = Library();
        if (!library.SetState || index < 0 || index >= MaximumGamepads)
        {
            return;
        }
        XINPUT_VIBRATION vibration {};
        vibration.wLeftMotorSpeed = static_cast<WORD>(Clamp(low, 0.0f, 1.0f) * 65535.0f);
        vibration.wRightMotorSpeed = static_cast<WORD>(Clamp(high, 0.0f, 1.0f) * 65535.0f);
        library.SetState(static_cast<DWORD>(index), &vibration);
    }
}
