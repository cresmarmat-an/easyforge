#pragma once

#include <string>
#include <variant>

#include <easyforge/core/Keys.h>
#include <easyforge/input/Gamepad.h>

namespace easyforge
{
    // The mouse wheel turned one way, for binding actions such as "next weapon".
    enum class WheelDirection
    {
        Up,
        Down,
        Left,
        Right,
    };

    // Four keys that together give a direction, as W, A, S, and D or the arrow
    // keys do. A key left as Key::Unknown is not used, so Left and Right alone
    // make a single axis.
    struct KeyAxis
    {
        Key Left = Key::Unknown;
        Key Right = Key::Unknown;
        Key Down = Key::Unknown;
        Key Up = Key::Unknown;

        bool operator==(const KeyAxis&) const = default;
    };

    // Something an action can be bound to: a key, a mouse button, the wheel, a
    // gamepad button, one axis of a gamepad, a whole stick, or four keys that
    // make a direction. It converts from each of those, so a list of them can be
    // written directly:
    //
    //     controls.Bind("Jump", { Key::Space, GamepadButton::South });
    class Binding
    {
    public:
        Binding(Key key) : Source(key) {}
        Binding(MouseButton button) : Source(button) {}
        Binding(WheelDirection direction) : Source(direction) {}
        Binding(GamepadButton button) : Source(button) {}
        Binding(GamepadAxis axis) : Source(axis) {}
        Binding(Stick stick) : Source(stick) {}
        Binding(KeyAxis keys) : Source(keys) {}

        // The binding as the type it was made from, or null when it is another
        // kind: `if (const Key* key = binding.As<Key>())`.
        template <typename Type>
        const Type* As() const
        {
            return std::get_if<Type>(&Source);
        }

        // True for sticks and key axes, which give a direction rather than a
        // single value.
        bool IsDirection() const
        {
            return std::holds_alternative<Stick>(Source) || std::holds_alternative<KeyAxis>(Source);
        }

        // A name to show people, such as "Space", "Left Mouse Button",
        // "South Button", "Left Stick", or "W A S D".
        std::string Name() const;

        bool operator==(const Binding&) const = default;

    private:
        std::variant<Key, MouseButton, WheelDirection, GamepadButton, GamepadAxis, Stick, KeyAxis> Source;
    };
}
