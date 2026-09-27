#pragma once

#include <string_view>

namespace easyforge
{
    // A key on the keyboard, named by where it is on a US keyboard, not by the
    // character it types. Key::W is the key left of E whatever the layout, so
    // game controls stay in the same place for everyone. For the characters a
    // person types, use EventType::TextEntered instead.
    enum class Key
    {
        Unknown,

        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,

        Space, Enter, Escape, Tab, Backspace,
        Insert, Delete, Home, End, PageUp, PageDown,
        Left, Right, Up, Down,

        Backquote, Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe,
        Comma, Period, Slash,

        // The extra key next to left Shift on many keyboards outside the US.
        InternationalBackslash,

        LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,

        // The Windows key, or Command on a Mac.
        LeftMeta, RightMeta,

        // The context menu key.
        Menu,

        CapsLock, ScrollLock, NumberLock, PrintScreen, Pause,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,

        NumberPad0, NumberPad1, NumberPad2, NumberPad3, NumberPad4,
        NumberPad5, NumberPad6, NumberPad7, NumberPad8, NumberPad9,
        NumberPadDecimal, NumberPadAdd, NumberPadSubtract, NumberPadMultiply, NumberPadDivide, NumberPadEnter,

        // How many keys there are, for arrays indexed by key.
        Count,
    };

    enum class MouseButton
    {
        Left,
        Right,
        Middle,

        // The side buttons many mice have.
        Back,
        Forward,

        Count,
    };

    // The modifier keys held when an event happened.
    struct KeyModifiers
    {
        bool Shift = false;
        bool Control = false;
        bool Alt = false;

        // The Windows key, or Command on a Mac.
        bool Meta = false;

        constexpr bool operator==(const KeyModifiers&) const = default;
    };

    // A name to show people, such as "A", "Left Shift", or "Page Down".
    std::string_view KeyName(Key key);
}
