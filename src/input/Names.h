#pragma once

// The names bindings are saved with: the same words as the enumerations in the
// public headers, so a bindings file reads like the code that made it.

#include <optional>
#include <string_view>

#include <easyforge/input/Binding.h>

namespace easyforge::internal
{
    std::string_view KeyIdentifier(Key key);
    std::optional<Key> KeyFromIdentifier(std::string_view identifier);

    std::string_view MouseButtonIdentifier(MouseButton button);
    std::optional<MouseButton> MouseButtonFromIdentifier(std::string_view identifier);

    std::string_view WheelDirectionIdentifier(WheelDirection direction);
    std::optional<WheelDirection> WheelDirectionFromIdentifier(std::string_view identifier);

    std::string_view GamepadButtonIdentifier(GamepadButton button);
    std::optional<GamepadButton> GamepadButtonFromIdentifier(std::string_view identifier);

    std::string_view GamepadAxisIdentifier(GamepadAxis axis);
    std::optional<GamepadAxis> GamepadAxisFromIdentifier(std::string_view identifier);

    std::string_view StickIdentifier(Stick stick);
    std::optional<Stick> StickFromIdentifier(std::string_view identifier);
}
