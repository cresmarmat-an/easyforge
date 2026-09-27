#include "Names.h"

#include <array>

namespace easyforge::internal
{
    namespace
    {
        constexpr std::array<std::string_view, static_cast<std::size_t>(Key::Count)> KeyIdentifiers = {
            "Unknown",
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
            "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
            "Digit0", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7", "Digit8", "Digit9",
            "Space", "Enter", "Escape", "Tab", "Backspace",
            "Insert", "Delete", "Home", "End", "PageUp", "PageDown",
            "Left", "Right", "Up", "Down",
            "Backquote", "Minus", "Equals", "LeftBracket", "RightBracket", "Backslash", "Semicolon", "Apostrophe",
            "Comma", "Period", "Slash",
            "InternationalBackslash",
            "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt",
            "LeftMeta", "RightMeta",
            "Menu",
            "CapsLock", "ScrollLock", "NumberLock", "PrintScreen", "Pause",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
            "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
            "NumberPad0", "NumberPad1", "NumberPad2", "NumberPad3", "NumberPad4",
            "NumberPad5", "NumberPad6", "NumberPad7", "NumberPad8", "NumberPad9",
            "NumberPadDecimal", "NumberPadAdd", "NumberPadSubtract", "NumberPadMultiply", "NumberPadDivide",
            "NumberPadEnter",
        };

        constexpr std::array<std::string_view, static_cast<std::size_t>(MouseButton::Count)> MouseButtonIdentifiers = {
            "Left", "Right", "Middle", "Back", "Forward",
        };

        constexpr std::array<std::string_view, 4> WheelDirectionIdentifiers = { "Up", "Down", "Left", "Right" };

        constexpr std::array<std::string_view, static_cast<std::size_t>(GamepadButton::Count)> GamepadButtonIdentifiers = {
            "South", "East", "West", "North", "LeftShoulder", "RightShoulder", "LeftTrigger", "RightTrigger",
            "Back", "Start", "LeftStick", "RightStick", "Up", "Down", "Left", "Right",
        };

        constexpr std::array<std::string_view, 6> GamepadAxisIdentifiers = {
            "LeftStickX", "LeftStickY", "RightStickX", "RightStickY", "LeftTrigger", "RightTrigger",
        };

        constexpr std::array<std::string_view, 2> StickIdentifiers = { "Left", "Right" };

        // Each list must reach the last value of its enumeration.
        static_assert(KeyIdentifiers.back() == "NumberPadEnter");
        static_assert(MouseButtonIdentifiers.back() == "Forward");
        static_assert(GamepadButtonIdentifiers.back() == "Right");

        template <typename Enumeration, std::size_t Size>
        std::string_view Lookup(const std::array<std::string_view, Size>& names, Enumeration value)
        {
            std::size_t index = static_cast<std::size_t>(value);
            return index < names.size() ? names[index] : std::string_view("Unknown");
        }

        template <typename Enumeration, std::size_t Size>
        std::optional<Enumeration> Find(const std::array<std::string_view, Size>& names, std::string_view name)
        {
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                if (names[index] == name)
                {
                    return static_cast<Enumeration>(index);
                }
            }
            return std::nullopt;
        }
    }

    std::string_view KeyIdentifier(Key key)
    {
        return Lookup(KeyIdentifiers, key);
    }

    std::optional<Key> KeyFromIdentifier(std::string_view identifier)
    {
        std::optional<Key> key = Find<Key>(KeyIdentifiers, identifier);
        return key == Key::Unknown ? std::nullopt : key;
    }

    std::string_view MouseButtonIdentifier(MouseButton button)
    {
        return Lookup(MouseButtonIdentifiers, button);
    }

    std::optional<MouseButton> MouseButtonFromIdentifier(std::string_view identifier)
    {
        return Find<MouseButton>(MouseButtonIdentifiers, identifier);
    }

    std::string_view WheelDirectionIdentifier(WheelDirection direction)
    {
        return Lookup(WheelDirectionIdentifiers, direction);
    }

    std::optional<WheelDirection> WheelDirectionFromIdentifier(std::string_view identifier)
    {
        return Find<WheelDirection>(WheelDirectionIdentifiers, identifier);
    }

    std::string_view GamepadButtonIdentifier(GamepadButton button)
    {
        return Lookup(GamepadButtonIdentifiers, button);
    }

    std::optional<GamepadButton> GamepadButtonFromIdentifier(std::string_view identifier)
    {
        return Find<GamepadButton>(GamepadButtonIdentifiers, identifier);
    }

    std::string_view GamepadAxisIdentifier(GamepadAxis axis)
    {
        return Lookup(GamepadAxisIdentifiers, axis);
    }

    std::optional<GamepadAxis> GamepadAxisFromIdentifier(std::string_view identifier)
    {
        return Find<GamepadAxis>(GamepadAxisIdentifiers, identifier);
    }

    std::string_view StickIdentifier(Stick stick)
    {
        return Lookup(StickIdentifiers, stick);
    }

    std::optional<Stick> StickFromIdentifier(std::string_view identifier)
    {
        return Find<Stick>(StickIdentifiers, identifier);
    }
}
