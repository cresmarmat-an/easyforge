#pragma once

#include <string>
#include <vector>

#include <easyforge/core/Rectangle.h>

namespace easyforge
{
    // A screen connected to the computer.
    //
    // Areas are in desktop pixels, not points, because screens side by side can
    // have different scaling and points would not line up across them.
    struct Monitor
    {
        // The name the screen reports, such as "DELL U2720Q", or a generic name
        // when it reports none.
        std::string Name;

        // All of the screen, placed on the desktop.
        Rectangle Area;

        // The part not covered by the taskbar, dock, or menu bar.
        Rectangle WorkArea;

        // Pixels per point, from the system's display scaling.
        float Scale = 1.0f;

        // Frames per second the screen shows.
        float RefreshRate = 60.0f;

        bool IsPrimary = false;

        // Every connected screen, the primary one first.
        static std::vector<Monitor> All();

        static Monitor Primary();
    };
}
