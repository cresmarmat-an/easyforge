#pragma once

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Event.h>
#include <easyforge/core/Host.h>
#include <easyforge/core/Property.h>
#include <easyforge/core/Result.h>
#include <easyforge/core/Vector.h>
#include <easyforge/input/Binding.h>
#include <easyforge/input/Gamepad.h>
#include <easyforge/input/Recording.h>

namespace easyforge
{
    namespace internal
    {
        class ControlsState;
    }

    // Reads every gamepad together, as if they were one.
    inline constexpr int AnyGamepad = -1;

    struct ControlsSettings
    {
        // Which gamepad to read, from 0 to MaximumGamepads - 1, or AnyGamepad.
        // Programs with one player per gamepad make one Controls for each.
        int Gamepad = AnyGamepad;

        // How far a stick has to move before it counts, from 0 to 1. Sticks
        // rarely rest exactly in the middle.
        float DeadZone = 0.2f;

        // Skips presses that a view already used, so typing in a text field does
        // not also make a character jump. Releases always count.
        bool SkipHandledEvents = true;
    };

    // Named actions such as "Jump" or "Move", bound to keys, mouse buttons, and
    // gamepads, and read once a frame:
    //
    //     Controls controls = Controls::New(window);
    //     controls.Bind("Jump", { Key::Space, GamepadButton::South });
    //     controls.Bind("Move", { Stick::Left, KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W } });
    //
    //     window.OnFrame = [&](float deltaSeconds) {
    //         if (controls.Pressed("Jump")) { ... }
    //         Vector2 move = controls.Axis("Move");
    //     };
    //
    // Given a host such as a window, Controls follows its events and frames by
    // itself. Without one, pass events to Handle and call NextFrame once a frame.
    //
    // Controls is a handle: copies share the same bindings and state.
    class Controls
    {
    public:
        using Settings = ControlsSettings;

        // Controls that follow the host's events and frames.
        static Controls New(std::shared_ptr<Host> host, const ControlsSettings& settings = {});

        // Controls that receive events through Handle and frames through NextFrame.
        static Controls New(const ControlsSettings& settings = {});

        // Empty controls that follow nothing; the same as Controls::New().
        Controls();

        Controls(const Controls& other);
        Controls& operator=(const Controls& other);
        ~Controls();

        // Binding actions. An action can have any number of bindings; Bind adds
        // to what the action already has.
        void Bind(std::string_view action, Binding binding) const;
        void Bind(std::string_view action, std::initializer_list<Binding> bindings) const;
        void Unbind(std::string_view action) const;
        void UnbindAll() const;
        std::vector<Binding> Bindings(std::string_view action) const;
        std::vector<std::string> Actions() const;

        // Bindings as text, one action a line, to keep in a settings file:
        //
        //     Jump = Key Space, GamepadButton South
        //     Move = Stick Left, KeyAxis A D S W
        std::string BindingsText() const;

        // Replaces every binding with the ones in the text. On failure nothing
        // changes and the error names the line.
        Result<> SetBindingsText(std::string_view text) const;

        Result<> SaveBindings(std::string_view path) const;
        Result<> LoadBindings(std::string_view path) const;

        // Reading actions. Held is true while any binding of the action is held.
        // Pressed is true in the frame it started being held, and Released in the
        // frame it stopped, even when both happened within the same frame.
        bool Held(std::string_view action) const;
        bool Pressed(std::string_view action) const;
        bool Released(std::string_view action) const;

        // How far the action is held, from 0 to 1: 1 for a key, part way for a
        // trigger or a stick. The largest of its bindings.
        float Value(std::string_view action) const;

        // The direction of the action's sticks and key axes together, with Y
        // positive for up, no longer than 1.
        Vector2 Axis(std::string_view action) const;

        // The same questions about one binding, without naming an action:
        // `controls.Held(Key::LeftShift)`.
        bool Held(const Binding& binding) const;
        bool Pressed(const Binding& binding) const;
        bool Released(const Binding& binding) const;
        float Value(const Binding& binding) const;
        Vector2 Axis(const Binding& binding) const;

        // The mouse, in points from the top left of the window.
        Vector2 MousePosition() const;

        // How far the mouse moved this frame, and how far the wheel turned.
        Vector2 MouseMovement() const;
        Vector2 Wheel() const;

        // The text typed this frame, as UTF-8.
        std::string Text() const;

        // Every gamepad as it was read this frame, after the dead zone.
        std::array<GamepadState, MaximumGamepads> Gamepads() const;

        // The first key, mouse button, wheel turn, or gamepad button pressed this
        // frame, for settings screens that ask "press the key for Jump".
        std::optional<Binding> LastPressed() const;

        // Shakes the gamepads this reads, with the low and high frequency motors
        // at strengths from 0 to 1, for the given seconds.
        void Rumble(float low, float high, float seconds) const;

        // For Controls without a host: an event that arrived since the last frame.
        void Handle(const Event& event) const;

        // For Controls without a host: starts the next frame. Call it once a
        // frame, after passing that frame's events to Handle and before reading.
        void NextFrame(float deltaSeconds = 0.0f) const;

        // The seconds this frame took, or while playing, the seconds the recorded
        // frame took. Use it in place of the window's delta for replays that
        // repeat a session exactly.
        float FrameSeconds() const;

        // Recording and playing back. While playing, the recorded events and
        // gamepads replace the real ones; playing ends by itself after the last
        // recorded frame.
        void StartRecording() const;
        Recording StopRecording() const;
        bool IsRecording() const;
        void Play(Recording recording) const;
        void StopPlaying() const;
        bool IsPlaying() const;

        Property<int> Gamepad;
        Property<float> DeadZone;
        Property<bool> SkipHandledEvents;

    private:
        explicit Controls(std::shared_ptr<internal::ControlsState> state);
        void RebindProperties();

        std::shared_ptr<internal::ControlsState> State;
    };
}
