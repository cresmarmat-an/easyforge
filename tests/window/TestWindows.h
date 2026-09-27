#pragma once

// Helpers for window tests. Windows start hidden so the tests do not take the
// keyboard or cover the screen, and the system's messages are sent to them
// directly, which makes each test independent of what the person at the
// computer does.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>

#include <string>
#include <vector>

#include <easyforge/window.h>

namespace testwindows
{
    inline easyforge::Window NewHidden(float width = 400, float height = 300)
    {
        return easyforge::Window::New({ .Title = "easyforge test", .Width = width, .Height = height, .Visible = false });
    }

    inline HWND HandleOf(const easyforge::Window& window)
    {
        return static_cast<HWND>(window.NativeSurface().Handle);
    }

    // Handles what was posted and runs a frame of every open window.
    inline void Pump(int frames = 1)
    {
        for (int frame = 0; frame < frames; ++frame)
        {
            easyforge::Window::RunOneFrame();
        }
    }

    // The events a window reported, kept by OnEvent.
    struct EventLog
    {
        std::vector<easyforge::Event> Events;

        void Follow(const easyforge::Window& window)
        {
            window.OnEvent = [this](const easyforge::Event& event) { Events.push_back(event); };
        }

        std::vector<easyforge::Event> OfType(easyforge::EventType type) const
        {
            std::vector<easyforge::Event> found;
            for (const easyforge::Event& event : Events)
            {
                if (event.Type == type)
                {
                    found.push_back(event);
                }
            }
            return found;
        }
    };

    // lParam for a key message: the scan code, the extended flag, and whether the
    // key was already down.
    inline LPARAM KeyParameter(unsigned scanCode, bool extended = false, bool wasDown = false, bool release = false)
    {
        LPARAM value = 1 | (static_cast<LPARAM>(scanCode) << 16);
        if (extended)
        {
            value |= LPARAM { 1 } << 24;
        }
        if (wasDown || release)
        {
            value |= LPARAM { 1 } << 30;
        }
        if (release)
        {
            value |= LPARAM { 1 } << 31;
        }
        return value;
    }
}
