#pragma once

#include <string>
#include <vector>

#include <easyforge/core/Keys.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    enum class EventType
    {
        // Key, Repeat, and Modifiers are set. Repeat is true for the presses a
        // held key sends automatically.
        KeyPressed,
        KeyReleased,

        // Text holds the characters typed, already in the person's layout and
        // language, as UTF-8.
        TextEntered,

        // Text holds what an input method, such as one for Japanese, is composing
        // but has not entered yet. Empty when composing ends.
        TextComposition,

        // Position is set, and Movement is the change since the last MouseMoved.
        MouseMoved,

        // Button, Position, and ClickCount are set.
        MouseButtonPressed,
        MouseButtonReleased,

        // Wheel is set: Y is positive away from the person, X positive to the right.
        MouseWheel,

        MouseEntered,
        MouseLeft,

        // Touch identifies the finger and Position is where it is.
        TouchBegan,
        TouchMoved,
        TouchEnded,
        TouchCancelled,

        // Size is the new size of the window's content.
        Resized,

        // Scale is the new number of pixels per point, after the window moved to a
        // screen with different scaling, or the scaling was changed.
        ScaleChanged,

        Minimized,
        Maximized,
        Restored,
        FocusGained,
        FocusLost,

        // The person asked to close the window.
        CloseRequested,

        // Files holds the dropped files' paths, and Position where they were dropped.
        FilesDropped,

        // The system switched between light and dark colors.
        ColorSchemeChanged,

        // A phone or browser tab was sent to the background, or came back.
        Suspended,
        Resumed,
    };

    // Something a window reports. Only the members listed for its type are set.
    //
    // Positions and sizes are in points, measured from the top left of the
    // window's content. A point is one pixel at 100% scaling; on a screen scaled
    // to 150%, a point is 1.5 pixels.
    struct Event
    {
        EventType Type = EventType::MouseMoved;

        easyforge::Key Key = Key::Unknown;
        bool Repeat = false;
        KeyModifiers Modifiers;

        std::string Text;

        Vector2 Position;
        Vector2 Movement;
        easyforge::MouseButton Button = MouseButton::Left;

        // 1 for a single click, 2 for a double click, 3 for a triple click.
        int ClickCount = 1;

        Vector2 Wheel;
        int Touch = 0;

        Vector2 Size;
        float Scale = 1.0f;
        std::vector<std::string> Files;

        // Set by whatever used the event, such as an interface element that was
        // clicked. Everything later in line still sees the event, and can skip it.
        bool Handled = false;
    };
}
