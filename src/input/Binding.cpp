#include <easyforge/input/Binding.h>

#include <array>

namespace easyforge
{
    namespace
    {
        constexpr std::array<std::string_view, static_cast<std::size_t>(MouseButton::Count)> MouseButtonNames = {
            "Left Mouse Button", "Right Mouse Button", "Middle Mouse Button", "Back Mouse Button", "Forward Mouse Button",
        };

        constexpr std::array<std::string_view, 4> WheelNames = { "Wheel Up", "Wheel Down", "Wheel Left", "Wheel Right" };

        constexpr std::array<std::string_view, static_cast<std::size_t>(GamepadButton::Count)> GamepadButtonNames = {
            "South Button", "East Button", "West Button", "North Button", "Left Shoulder", "Right Shoulder",
            "Left Trigger", "Right Trigger", "Back", "Start", "Left Stick Button", "Right Stick Button",
            "Directional Pad Up", "Directional Pad Down", "Directional Pad Left", "Directional Pad Right",
        };
        static_assert(GamepadButtonNames.back() == "Directional Pad Right");

        constexpr std::array<std::string_view, 6> GamepadAxisNames = {
            "Left Stick Across", "Left Stick Up and Down", "Right Stick Across", "Right Stick Up and Down",
            "Left Trigger", "Right Trigger",
        };

        template <std::size_t Size, typename Enumeration>
        std::string NameFrom(const std::array<std::string_view, Size>& names, Enumeration value)
        {
            std::size_t index = static_cast<std::size_t>(value);
            return std::string(index < names.size() ? names[index] : std::string_view("Unknown"));
        }
    }

    std::string Binding::Name() const
    {
        if (const Key* key = As<Key>())
        {
            return std::string(KeyName(*key));
        }
        if (const MouseButton* button = As<MouseButton>())
        {
            return NameFrom(MouseButtonNames, *button);
        }
        if (const WheelDirection* direction = As<WheelDirection>())
        {
            return NameFrom(WheelNames, *direction);
        }
        if (const GamepadButton* button = As<GamepadButton>())
        {
            return NameFrom(GamepadButtonNames, *button);
        }
        if (const GamepadAxis* axis = As<GamepadAxis>())
        {
            return NameFrom(GamepadAxisNames, *axis);
        }
        if (const Stick* stick = As<Stick>())
        {
            return *stick == Stick::Left ? "Left Stick" : "Right Stick";
        }

        // Keys in the order people say them: up, left, down, right, as in "W A S D".
        const KeyAxis& keys = *As<KeyAxis>();
        std::string name;
        for (Key key : { keys.Up, keys.Left, keys.Down, keys.Right })
        {
            if (key == Key::Unknown)
            {
                continue;
            }
            if (!name.empty())
            {
                name += ' ';
            }
            name += KeyName(key);
        }
        return name;
    }
}
