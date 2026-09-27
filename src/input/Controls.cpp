#include <easyforge/input/Controls.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <span>

#include <easyforge/core/Clock.h>
#include <easyforge/core/Scalar.h>

#include "Gamepads.h"
#include "Names.h"

namespace easyforge
{
    namespace internal
    {
        namespace
        {
            constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count);
            constexpr std::size_t ButtonCount = static_cast<std::size_t>(MouseButton::Count);
        }

        // What was held at one moment. Bindings are measured against the moment
        // now and the one at the start of the last frame.
        struct Snapshot
        {
            std::array<bool, KeyCount> Keys {};
            std::array<bool, ButtonCount> MouseButtons {};
            Vector2 Wheel;
            GamepadState Gamepad;
        };

        class ControlsState final : public HostListener
        {
        public:
            ~ControlsState() override;

            // HostListener
            void HandleEvent(const Event& event) override { Receive(event); }
            void FrameStarted(float deltaSeconds) override { BeginFrame(deltaSeconds); }
            void FrameEnded() override {}

            void Receive(const Event& event);
            void BeginFrame(float deltaSeconds);

            bool Held(const Binding& binding, const Snapshot& snapshot) const;
            float Value(const Binding& binding, const Snapshot& snapshot) const;
            Vector2 Direction(const Binding& binding, const Snapshot& snapshot) const;
            bool PressedDuringFrame(const Binding& binding) const;

            bool AnyHeld(std::span<const Binding> bindings, const Snapshot& snapshot) const;
            bool Pressed(std::span<const Binding> bindings) const;
            bool Released(std::span<const Binding> bindings) const;
            float Value(std::span<const Binding> bindings) const;
            Vector2 Axis(std::span<const Binding> bindings) const;

            void StartRecording();
            void Play(Recording recording);
            void Rumble(float low, float high, float seconds);
            void StopRumble();

            std::span<const Binding> BindingsOf(std::string_view action) const;

            std::shared_ptr<Host> TheHost;
            ControlsSettings Settings;
            std::map<std::string, std::vector<Binding>, std::less<>> Actions;

            // Events wait here until the next frame starts.
            std::vector<Event> Pending;

            Snapshot Now;
            Snapshot Before;
            std::array<bool, KeyCount> KeysPressed {};
            std::array<bool, ButtonCount> ButtonsPressed {};
            Vector2 MousePosition;
            Vector2 MouseMovement;
            std::string Text;
            std::array<GamepadState, MaximumGamepads> Gamepads {};
            std::optional<Binding> LastPressed;
            float FrameSeconds = 0.0f;

            bool Recording = false;
            easyforge::Recording Recorded;
            bool Playing = false;
            easyforge::Recording Playback;
            std::size_t PlaybackFrame = 0;

            bool Rumbling = false;
            float RumbleSeconds = 0.0f;
            Clock RumbleClock;

        private:
            void Apply(const Event& event);
            void ResetState();
        };

        namespace
        {
            Vector2 WithDeadZone(Vector2 stick, float deadZone)
            {
                float length = Length(stick);
                if (length <= deadZone || deadZone >= 1.0f)
                {
                    return {};
                }
                float scaled = Min((length - deadZone) / (1.0f - deadZone), 1.0f);
                return stick * (scaled / length);
            }

            GamepadState Combine(const std::array<GamepadState, MaximumGamepads>& gamepads, int selected)
            {
                if (selected >= 0 && selected < MaximumGamepads)
                {
                    return gamepads[static_cast<std::size_t>(selected)];
                }
                GamepadState combined;
                for (const GamepadState& gamepad : gamepads)
                {
                    if (!gamepad.Connected)
                    {
                        continue;
                    }
                    combined.Connected = true;
                    for (std::size_t button = 0; button < combined.Buttons.size(); ++button)
                    {
                        combined.Buttons[button] = combined.Buttons[button] || gamepad.Buttons[button];
                    }
                    if (Length(gamepad.LeftStick) > Length(combined.LeftStick))
                    {
                        combined.LeftStick = gamepad.LeftStick;
                    }
                    if (Length(gamepad.RightStick) > Length(combined.RightStick))
                    {
                        combined.RightStick = gamepad.RightStick;
                    }
                    combined.LeftTrigger = Max(combined.LeftTrigger, gamepad.LeftTrigger);
                    combined.RightTrigger = Max(combined.RightTrigger, gamepad.RightTrigger);
                }
                return combined;
            }

            bool IsPress(EventType type)
            {
                return type == EventType::KeyPressed || type == EventType::MouseButtonPressed ||
                       type == EventType::MouseWheel || type == EventType::TextEntered;
            }
        }

        ControlsState::~ControlsState()
        {
            StopRumble();
            if (TheHost)
            {
                TheHost->RemoveListener(*this);
            }
        }

        void ControlsState::Receive(const Event& event)
        {
            if (Playing)
            {
                return;
            }
            if (Settings.SkipHandledEvents && event.Handled && IsPress(event.Type))
            {
                return;
            }
            Pending.push_back(event);
        }

        void ControlsState::Apply(const Event& event)
        {
            switch (event.Type)
            {
            case EventType::KeyPressed:
            {
                std::size_t key = static_cast<std::size_t>(event.Key);
                if (event.Key == Key::Unknown || key >= KeyCount || event.Repeat)
                {
                    break;
                }
                if (!Now.Keys[key])
                {
                    KeysPressed[key] = true;
                    if (!LastPressed)
                    {
                        LastPressed = Binding(event.Key);
                    }
                }
                Now.Keys[key] = true;
                break;
            }
            case EventType::KeyReleased:
            {
                std::size_t key = static_cast<std::size_t>(event.Key);
                if (key < KeyCount)
                {
                    Now.Keys[key] = false;
                }
                break;
            }
            case EventType::MouseButtonPressed:
            {
                std::size_t button = static_cast<std::size_t>(event.Button);
                if (button >= ButtonCount)
                {
                    break;
                }
                if (!Now.MouseButtons[button])
                {
                    ButtonsPressed[button] = true;
                    if (!LastPressed)
                    {
                        LastPressed = Binding(event.Button);
                    }
                }
                Now.MouseButtons[button] = true;
                MousePosition = event.Position;
                break;
            }
            case EventType::MouseButtonReleased:
            {
                std::size_t button = static_cast<std::size_t>(event.Button);
                if (button < ButtonCount)
                {
                    Now.MouseButtons[button] = false;
                }
                MousePosition = event.Position;
                break;
            }
            case EventType::MouseMoved:
                MousePosition = event.Position;
                MouseMovement += event.Movement;
                break;
            case EventType::MouseWheel:
                Now.Wheel += event.Wheel;
                if (!LastPressed && (event.Wheel.X != 0.0f || event.Wheel.Y != 0.0f))
                {
                    LastPressed = Binding(std::abs(event.Wheel.Y) >= std::abs(event.Wheel.X)
                                              ? (event.Wheel.Y > 0 ? WheelDirection::Up : WheelDirection::Down)
                                              : (event.Wheel.X > 0 ? WheelDirection::Right : WheelDirection::Left));
                }
                break;
            case EventType::TextEntered:
                Text += event.Text;
                break;
            default:
                break;
            }
        }

        void ControlsState::ResetState()
        {
            Now = {};
            Before = {};
            KeysPressed = {};
            ButtonsPressed = {};
            MouseMovement = {};
            Text.clear();
            LastPressed.reset();
        }

        void ControlsState::BeginFrame(float deltaSeconds)
        {
            Before = Now;
            Now.Wheel = {};
            KeysPressed = {};
            ButtonsPressed = {};
            MouseMovement = {};
            Text.clear();
            LastPressed.reset();

            std::vector<Event> events;
            std::array<GamepadState, MaximumGamepads> raw {};
            if (Playing && PlaybackFrame >= Playback.Frames.size())
            {
                // Nothing recorded stays held once the recording is over.
                Playing = false;
                Playback = {};
                ResetState();
            }
            if (Playing)
            {
                const RecordedFrame& frame = Playback.Frames[PlaybackFrame++];
                FrameSeconds = frame.DeltaSeconds;
                events = frame.Events;
                raw = frame.Gamepads;
                Pending.clear();
            }
            else
            {
                FrameSeconds = deltaSeconds;
                events = std::move(Pending);
                Pending.clear();
                raw = ReadGamepads();
                if (Recording)
                {
                    Recorded.Frames.push_back({ deltaSeconds, events, raw });
                }
            }

            for (const Event& event : events)
            {
                Apply(event);
            }

            for (std::size_t index = 0; index < Gamepads.size(); ++index)
            {
                GamepadState gamepad = raw[index];
                gamepad.LeftStick = WithDeadZone(gamepad.LeftStick, Settings.DeadZone);
                gamepad.RightStick = WithDeadZone(gamepad.RightStick, Settings.DeadZone);
                Gamepads[index] = gamepad;
            }
            Now.Gamepad = Combine(Gamepads, Settings.Gamepad);
            for (std::size_t button = 0; button < Now.Gamepad.Buttons.size() && !LastPressed; ++button)
            {
                if (Now.Gamepad.Buttons[button] && !Before.Gamepad.Buttons[button])
                {
                    LastPressed = Binding(static_cast<GamepadButton>(button));
                }
            }

            if (Rumbling && RumbleClock.Seconds() >= RumbleSeconds)
            {
                StopRumble();
            }
        }

        bool ControlsState::Held(const Binding& binding, const Snapshot& snapshot) const
        {
            if (const Key* key = binding.As<Key>())
            {
                std::size_t index = static_cast<std::size_t>(*key);
                return *key != Key::Unknown && index < KeyCount && snapshot.Keys[index];
            }
            if (const MouseButton* button = binding.As<MouseButton>())
            {
                std::size_t index = static_cast<std::size_t>(*button);
                return index < ButtonCount && snapshot.MouseButtons[index];
            }
            if (const GamepadButton* button = binding.As<GamepadButton>())
            {
                return snapshot.Gamepad.Held(*button);
            }
            return Value(binding, snapshot) >= 0.5f;
        }

        float ControlsState::Value(const Binding& binding, const Snapshot& snapshot) const
        {
            if (const WheelDirection* direction = binding.As<WheelDirection>())
            {
                switch (*direction)
                {
                case WheelDirection::Up: return snapshot.Wheel.Y > 0.0f ? 1.0f : 0.0f;
                case WheelDirection::Down: return snapshot.Wheel.Y < 0.0f ? 1.0f : 0.0f;
                case WheelDirection::Left: return snapshot.Wheel.X < 0.0f ? 1.0f : 0.0f;
                case WheelDirection::Right: return snapshot.Wheel.X > 0.0f ? 1.0f : 0.0f;
                }
                return 0.0f;
            }
            if (const GamepadButton* button = binding.As<GamepadButton>())
            {
                // The triggers give how far they are pulled; other buttons are on or off.
                if (*button == GamepadButton::LeftTrigger)
                {
                    return snapshot.Gamepad.LeftTrigger;
                }
                if (*button == GamepadButton::RightTrigger)
                {
                    return snapshot.Gamepad.RightTrigger;
                }
                return snapshot.Gamepad.Held(*button) ? 1.0f : 0.0f;
            }
            if (const GamepadAxis* axis = binding.As<GamepadAxis>())
            {
                return std::abs(snapshot.Gamepad.Value(*axis));
            }
            if (binding.IsDirection())
            {
                return Length(Direction(binding, snapshot));
            }
            return Held(binding, snapshot) ? 1.0f : 0.0f;
        }

        Vector2 ControlsState::Direction(const Binding& binding, const Snapshot& snapshot) const
        {
            if (const Stick* stick = binding.As<Stick>())
            {
                return *stick == Stick::Left ? snapshot.Gamepad.LeftStick : snapshot.Gamepad.RightStick;
            }
            if (const KeyAxis* keys = binding.As<KeyAxis>())
            {
                auto held = [&](Key key) { return Held(Binding(key), snapshot) ? 1.0f : 0.0f; };
                Vector2 direction { held(keys->Right) - held(keys->Left), held(keys->Up) - held(keys->Down) };
                float length = Length(direction);
                return length > 1.0f ? direction / length : direction;
            }
            if (const GamepadAxis* axis = binding.As<GamepadAxis>())
            {
                float value = snapshot.Gamepad.Value(*axis);
                bool vertical = *axis == GamepadAxis::LeftStickY || *axis == GamepadAxis::RightStickY;
                return vertical ? Vector2 { 0.0f, value } : Vector2 { value, 0.0f };
            }
            return {};
        }

        bool ControlsState::PressedDuringFrame(const Binding& binding) const
        {
            if (const Key* key = binding.As<Key>())
            {
                std::size_t index = static_cast<std::size_t>(*key);
                return index < KeyCount && KeysPressed[index];
            }
            if (const MouseButton* button = binding.As<MouseButton>())
            {
                std::size_t index = static_cast<std::size_t>(*button);
                return index < ButtonCount && ButtonsPressed[index];
            }
            return false;
        }

        bool ControlsState::AnyHeld(std::span<const Binding> bindings, const Snapshot& snapshot) const
        {
            return std::any_of(bindings.begin(), bindings.end(),
                [&](const Binding& binding) { return Held(binding, snapshot); });
        }

        bool ControlsState::Pressed(std::span<const Binding> bindings) const
        {
            bool heldBefore = AnyHeld(bindings, Before);
            bool pressedDuring = std::any_of(bindings.begin(), bindings.end(),
                [&](const Binding& binding) { return PressedDuringFrame(binding); });
            // Each turn of the wheel is a new press, even in the next frame.
            bool wheelTurned = std::any_of(bindings.begin(), bindings.end(),
                [&](const Binding& binding) { return binding.As<WheelDirection>() && Held(binding, Now); });
            return wheelTurned || (!heldBefore && (AnyHeld(bindings, Now) || pressedDuring));
        }

        bool ControlsState::Released(std::span<const Binding> bindings) const
        {
            bool pressedDuring = std::any_of(bindings.begin(), bindings.end(),
                [&](const Binding& binding) { return PressedDuringFrame(binding); });
            return !AnyHeld(bindings, Now) && (AnyHeld(bindings, Before) || pressedDuring);
        }

        float ControlsState::Value(std::span<const Binding> bindings) const
        {
            float largest = 0.0f;
            for (const Binding& binding : bindings)
            {
                largest = Max(largest, Value(binding, Now));
            }
            return largest;
        }

        Vector2 ControlsState::Axis(std::span<const Binding> bindings) const
        {
            Vector2 total;
            for (const Binding& binding : bindings)
            {
                total += Direction(binding, Now);
            }
            float length = Length(total);
            return length > 1.0f ? total / length : total;
        }

        std::span<const Binding> ControlsState::BindingsOf(std::string_view action) const
        {
            auto found = Actions.find(action);
            if (found == Actions.end())
            {
                return {};
            }
            return found->second;
        }

        void ControlsState::StartRecording()
        {
            Recording = true;
            Recorded = {};
            // Keys and buttons already held when recording starts are pressed at the
            // start of the recording, so playing it back begins in the same state.
            std::vector<Event> held;
            for (std::size_t key = 0; key < KeyCount; ++key)
            {
                if (Now.Keys[key])
                {
                    Event event;
                    event.Type = EventType::KeyPressed;
                    event.Key = static_cast<Key>(key);
                    held.push_back(event);
                }
            }
            for (std::size_t button = 0; button < ButtonCount; ++button)
            {
                if (Now.MouseButtons[button])
                {
                    Event event;
                    event.Type = EventType::MouseButtonPressed;
                    event.Button = static_cast<MouseButton>(button);
                    event.Position = MousePosition;
                    held.push_back(event);
                }
            }
            Event position;
            position.Type = EventType::MouseMoved;
            position.Position = MousePosition;
            held.push_back(position);
            Pending.insert(Pending.begin(), held.begin(), held.end());
        }

        void ControlsState::Play(easyforge::Recording recording)
        {
            Playback = std::move(recording);
            PlaybackFrame = 0;
            Playing = !Playback.Frames.empty();
            Pending.clear();
            ResetState();
            MousePosition = {};
        }

        void ControlsState::Rumble(float low, float high, float seconds)
        {
            for (int index = 0; index < MaximumGamepads; ++index)
            {
                if (Settings.Gamepad == AnyGamepad || Settings.Gamepad == index)
                {
                    SetGamepadRumble(index, low, high);
                }
            }
            Rumbling = true;
            RumbleSeconds = Max(seconds, 0.0f);
            RumbleClock.Restart();
        }

        void ControlsState::StopRumble()
        {
            if (!Rumbling)
            {
                return;
            }
            Rumbling = false;
            for (int index = 0; index < MaximumGamepads; ++index)
            {
                if (Settings.Gamepad == AnyGamepad || Settings.Gamepad == index)
                {
                    SetGamepadRumble(index, 0.0f, 0.0f);
                }
            }
        }
    }

    namespace
    {
        using internal::ControlsState;

        ControlsState& StateOf(void* owner)
        {
            return *static_cast<ControlsState*>(owner);
        }

        const ControlsState& StateOf(const void* owner)
        {
            return *static_cast<const ControlsState*>(owner);
        }

        std::string_view Trim(std::string_view text)
        {
            std::size_t first = text.find_first_not_of(" \t\r");
            if (first == std::string_view::npos)
            {
                return {};
            }
            std::size_t last = text.find_last_not_of(" \t\r");
            return text.substr(first, last - first + 1);
        }

        std::vector<std::string_view> Words(std::string_view text)
        {
            std::vector<std::string_view> words;
            std::size_t position = 0;
            while (position < text.size())
            {
                std::size_t start = text.find_first_not_of(" \t", position);
                if (start == std::string_view::npos)
                {
                    break;
                }
                std::size_t end = text.find_first_of(" \t", start);
                if (end == std::string_view::npos)
                {
                    end = text.size();
                }
                words.push_back(text.substr(start, end - start));
                position = end;
            }
            return words;
        }

        std::string BindingText(const Binding& binding)
        {
            using namespace internal;
            if (const Key* key = binding.As<Key>())
            {
                return std::format("Key {}", KeyIdentifier(*key));
            }
            if (const MouseButton* button = binding.As<MouseButton>())
            {
                return std::format("MouseButton {}", MouseButtonIdentifier(*button));
            }
            if (const WheelDirection* direction = binding.As<WheelDirection>())
            {
                return std::format("Wheel {}", WheelDirectionIdentifier(*direction));
            }
            if (const GamepadButton* button = binding.As<GamepadButton>())
            {
                return std::format("GamepadButton {}", GamepadButtonIdentifier(*button));
            }
            if (const GamepadAxis* axis = binding.As<GamepadAxis>())
            {
                return std::format("GamepadAxis {}", GamepadAxisIdentifier(*axis));
            }
            if (const Stick* stick = binding.As<Stick>())
            {
                return std::format("Stick {}", StickIdentifier(*stick));
            }
            const KeyAxis& keys = *binding.As<KeyAxis>();
            auto name = [](Key key) { return key == Key::Unknown ? std::string_view("None") : KeyIdentifier(key); };
            return std::format("KeyAxis {} {} {} {}", name(keys.Left), name(keys.Right), name(keys.Down), name(keys.Up));
        }

        Result<Binding> ParseBinding(std::string_view text)
        {
            using namespace internal;
            std::vector<std::string_view> words = Words(text);
            if (words.empty())
            {
                return Failure("a binding is empty");
            }
            std::string_view kind = words[0];
            auto unknown = [&] { return Failure(std::format("'{}' is not a binding easyforge knows", text)); };

            if (kind == "KeyAxis")
            {
                if (words.size() != 5)
                {
                    return Failure(std::format("'{}' needs four keys: left, right, down, and up", text));
                }
                std::array<Key, 4> keys {};
                for (std::size_t index = 0; index < 4; ++index)
                {
                    if (words[index + 1] == "None")
                    {
                        keys[index] = Key::Unknown;
                        continue;
                    }
                    std::optional<Key> key = KeyFromIdentifier(words[index + 1]);
                    if (!key)
                    {
                        return Failure(std::format("'{}' is not a key easyforge knows", words[index + 1]));
                    }
                    keys[index] = *key;
                }
                return Binding(KeyAxis { keys[0], keys[1], keys[2], keys[3] });
            }
            if (words.size() != 2)
            {
                return unknown();
            }
            std::string_view name = words[1];
            if (kind == "Key")
            {
                if (std::optional<Key> key = KeyFromIdentifier(name))
                {
                    return Binding(*key);
                }
            }
            else if (kind == "MouseButton")
            {
                if (std::optional<MouseButton> button = MouseButtonFromIdentifier(name))
                {
                    return Binding(*button);
                }
            }
            else if (kind == "Wheel")
            {
                if (std::optional<WheelDirection> direction = WheelDirectionFromIdentifier(name))
                {
                    return Binding(*direction);
                }
            }
            else if (kind == "GamepadButton")
            {
                if (std::optional<GamepadButton> button = GamepadButtonFromIdentifier(name))
                {
                    return Binding(*button);
                }
            }
            else if (kind == "GamepadAxis")
            {
                if (std::optional<GamepadAxis> axis = GamepadAxisFromIdentifier(name))
                {
                    return Binding(*axis);
                }
            }
            else if (kind == "Stick")
            {
                if (std::optional<Stick> stick = StickFromIdentifier(name))
                {
                    return Binding(*stick);
                }
            }
            return unknown();
        }
    }

    Controls Controls::New(std::shared_ptr<Host> host, const ControlsSettings& settings)
    {
        auto state = std::make_shared<ControlsState>();
        state->Settings = settings;
        state->Settings.Gamepad = Clamp(settings.Gamepad, AnyGamepad, MaximumGamepads - 1);
        if (host)
        {
            state->TheHost = std::move(host);
            state->TheHost->AddListener(*state);
        }
        return Controls(state);
    }

    Controls Controls::New(const ControlsSettings& settings)
    {
        return New(nullptr, settings);
    }

    Controls::Controls() : Controls(std::make_shared<ControlsState>())
    {
    }

    Controls::Controls(const Controls& other) : Controls(other.State)
    {
    }

    Controls& Controls::operator=(const Controls& other)
    {
        if (this != &other)
        {
            State = other.State;
            RebindProperties();
        }
        return *this;
    }

    Controls::~Controls() = default;

    Controls::Controls(std::shared_ptr<internal::ControlsState> state)
        : Gamepad(state.get(),
              [](const void* owner) { return StateOf(owner).Settings.Gamepad; },
              [](void* owner, const int& value) {
                  StateOf(owner).Settings.Gamepad = Clamp(value, AnyGamepad, MaximumGamepads - 1);
              }),
          DeadZone(state.get(),
              [](const void* owner) { return StateOf(owner).Settings.DeadZone; },
              [](void* owner, const float& value) { StateOf(owner).Settings.DeadZone = Clamp(value, 0.0f, 1.0f); }),
          SkipHandledEvents(state.get(),
              [](const void* owner) { return StateOf(owner).Settings.SkipHandledEvents; },
              [](void* owner, const bool& value) { StateOf(owner).Settings.SkipHandledEvents = value; }),
          State(std::move(state))
    {
    }

    void Controls::RebindProperties()
    {
        Gamepad.Rebind(State.get());
        DeadZone.Rebind(State.get());
        SkipHandledEvents.Rebind(State.get());
    }

    void Controls::Bind(std::string_view action, Binding binding) const
    {
        auto found = State->Actions.find(action);
        if (found == State->Actions.end())
        {
            found = State->Actions.emplace(std::string(action), std::vector<Binding> {}).first;
        }
        if (std::find(found->second.begin(), found->second.end(), binding) == found->second.end())
        {
            found->second.push_back(binding);
        }
    }

    void Controls::Bind(std::string_view action, std::initializer_list<Binding> bindings) const
    {
        for (const Binding& binding : bindings)
        {
            Bind(action, binding);
        }
    }

    void Controls::Unbind(std::string_view action) const
    {
        auto found = State->Actions.find(action);
        if (found != State->Actions.end())
        {
            State->Actions.erase(found);
        }
    }

    void Controls::UnbindAll() const
    {
        State->Actions.clear();
    }

    std::vector<Binding> Controls::Bindings(std::string_view action) const
    {
        std::span<const Binding> bindings = State->BindingsOf(action);
        return { bindings.begin(), bindings.end() };
    }

    std::vector<std::string> Controls::Actions() const
    {
        std::vector<std::string> names;
        for (const auto& [name, bindings] : State->Actions)
        {
            names.push_back(name);
        }
        return names;
    }

    std::string Controls::BindingsText() const
    {
        std::string text;
        for (const auto& [name, bindings] : State->Actions)
        {
            text += name;
            text += " =";
            for (std::size_t index = 0; index < bindings.size(); ++index)
            {
                text += index == 0 ? " " : ", ";
                text += BindingText(bindings[index]);
            }
            text += '\n';
        }
        return text;
    }

    Result<> Controls::SetBindingsText(std::string_view text) const
    {
        std::map<std::string, std::vector<Binding>, std::less<>> actions;
        std::size_t lineNumber = 0;
        std::size_t position = 0;
        while (position <= text.size())
        {
            std::size_t end = text.find('\n', position);
            if (end == std::string_view::npos)
            {
                end = text.size();
            }
            std::string_view line = text.substr(position, end - position);
            position = end + 1;
            ++lineNumber;

            if (std::size_t comment = line.find('#'); comment != std::string_view::npos)
            {
                line = line.substr(0, comment);
            }
            line = Trim(line);
            if (line.empty())
            {
                continue;
            }
            std::size_t equals = line.find('=');
            std::string_view action = equals == std::string_view::npos ? std::string_view() : Trim(line.substr(0, equals));
            if (action.empty())
            {
                return Failure(std::format("line {}: expected an action, an equals sign, and its bindings", lineNumber));
            }
            std::vector<Binding>& bindings = actions[std::string(action)];
            std::string_view rest = line.substr(equals + 1);
            while (!Trim(rest).empty())
            {
                std::size_t comma = rest.find(',');
                std::string_view part = Trim(rest.substr(0, comma));
                rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
                Result<Binding> binding = ParseBinding(part);
                if (!binding)
                {
                    return Failure(std::format("line {}: {}", lineNumber, binding.Error()));
                }
                bindings.push_back(*binding);
            }
        }
        State->Actions = std::move(actions);
        return {};
    }

    Result<> Controls::SaveBindings(std::string_view path) const
    {
        std::ofstream file { std::string(path), std::ios::binary };
        file << BindingsText();
        if (!file)
        {
            return Failure(std::format("{}: the bindings could not be written", path));
        }
        return {};
    }

    Result<> Controls::LoadBindings(std::string_view path) const
    {
        std::ifstream file { std::string(path), std::ios::binary };
        if (!file)
        {
            return Failure(std::format("{}: the file could not be opened", path));
        }
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Result<> loaded = SetBindingsText(text);
        if (!loaded)
        {
            return Failure(std::format("{}: {}", path, loaded.Error()));
        }
        return {};
    }

    bool Controls::Held(std::string_view action) const
    {
        return State->AnyHeld(State->BindingsOf(action), State->Now);
    }

    bool Controls::Pressed(std::string_view action) const
    {
        return State->Pressed(State->BindingsOf(action));
    }

    bool Controls::Released(std::string_view action) const
    {
        return State->Released(State->BindingsOf(action));
    }

    float Controls::Value(std::string_view action) const
    {
        return State->Value(State->BindingsOf(action));
    }

    Vector2 Controls::Axis(std::string_view action) const
    {
        return State->Axis(State->BindingsOf(action));
    }

    bool Controls::Held(const Binding& binding) const
    {
        return State->Held(binding, State->Now);
    }

    bool Controls::Pressed(const Binding& binding) const
    {
        return State->Pressed(std::span<const Binding>(&binding, 1));
    }

    bool Controls::Released(const Binding& binding) const
    {
        return State->Released(std::span<const Binding>(&binding, 1));
    }

    float Controls::Value(const Binding& binding) const
    {
        return State->Value(binding, State->Now);
    }

    Vector2 Controls::Axis(const Binding& binding) const
    {
        return State->Axis(std::span<const Binding>(&binding, 1));
    }

    Vector2 Controls::MousePosition() const
    {
        return State->MousePosition;
    }

    Vector2 Controls::MouseMovement() const
    {
        return State->MouseMovement;
    }

    Vector2 Controls::Wheel() const
    {
        return State->Now.Wheel;
    }

    std::string Controls::Text() const
    {
        return State->Text;
    }

    std::array<GamepadState, MaximumGamepads> Controls::Gamepads() const
    {
        return State->Gamepads;
    }

    std::optional<Binding> Controls::LastPressed() const
    {
        return State->LastPressed;
    }

    void Controls::Rumble(float low, float high, float seconds) const
    {
        State->Rumble(low, high, seconds);
    }

    void Controls::Handle(const Event& event) const
    {
        State->Receive(event);
    }

    void Controls::NextFrame(float deltaSeconds) const
    {
        State->BeginFrame(deltaSeconds);
    }

    float Controls::FrameSeconds() const
    {
        return State->FrameSeconds;
    }

    void Controls::StartRecording() const
    {
        State->StartRecording();
    }

    Recording Controls::StopRecording() const
    {
        State->Recording = false;
        return std::move(State->Recorded);
    }

    bool Controls::IsRecording() const
    {
        return State->Recording;
    }

    void Controls::Play(Recording recording) const
    {
        State->Play(std::move(recording));
    }

    void Controls::StopPlaying() const
    {
        State->Playing = false;
        State->Playback = {};
    }

    bool Controls::IsPlaying() const
    {
        return State->Playing;
    }
}
