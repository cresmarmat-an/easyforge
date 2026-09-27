#include <easyforge/core/Keys.h>

#include <array>

namespace easyforge
{
    std::string_view KeyName(Key key)
    {
        static constexpr std::array<std::string_view, static_cast<std::size_t>(Key::Count)> names = {
            "Unknown",
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
            "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
            "Space", "Enter", "Escape", "Tab", "Backspace",
            "Insert", "Delete", "Home", "End", "Page Up", "Page Down",
            "Left", "Right", "Up", "Down",
            "`", "-", "=", "[", "]", "\\", ";", "'", ",", ".", "/",
            "International Backslash",
            "Left Shift", "Right Shift", "Left Control", "Right Control", "Left Alt", "Right Alt",
            "Left Meta", "Right Meta",
            "Menu",
            "Caps Lock", "Scroll Lock", "Number Lock", "Print Screen", "Pause",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
            "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
            "Number Pad 0", "Number Pad 1", "Number Pad 2", "Number Pad 3", "Number Pad 4",
            "Number Pad 5", "Number Pad 6", "Number Pad 7", "Number Pad 8", "Number Pad 9",
            "Number Pad .", "Number Pad +", "Number Pad -", "Number Pad *", "Number Pad /", "Number Pad Enter",
        };
        std::size_t index = static_cast<std::size_t>(key);
        return index < names.size() ? names[index] : names[0];
    }
}
