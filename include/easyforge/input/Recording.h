#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <easyforge/core/Event.h>
#include <easyforge/core/Result.h>
#include <easyforge/input/Gamepad.h>

namespace easyforge
{
    // What Controls received in one frame: the seconds the frame took, the events
    // that arrived before it, and every gamepad as it was read.
    struct RecordedFrame
    {
        float DeltaSeconds = 0.0f;
        std::vector<Event> Events;
        std::array<GamepadState, MaximumGamepads> Gamepads {};
    };

    // A session of input, frame by frame, made by Controls::StopRecording and
    // repeated by Controls::Play.
    class Recording
    {
    public:
        std::vector<RecordedFrame> Frames;

        // The seconds the recorded frames add up to.
        float Duration() const;

        Result<> Save(std::string_view path) const;
        static Result<Recording> Load(std::string_view path);

        // The same as Save and Load, in memory.
        std::vector<std::uint8_t> Encode() const;
        static Result<Recording> Decode(std::span<const std::uint8_t> bytes);
    };
}
