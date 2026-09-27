#include <easyforge/core/Testing.h>
#include <easyforge/input.h>

#include <algorithm>
#include <functional>
#include <map>

#ifdef _WIN32
#include "../../src/input/windows/XInputGamepads.h"
#endif

using namespace easyforge;

namespace
{
    // A host with no window, which delivers events and frames when the test says.
    class TestHost final : public Host
    {
    public:
        Surface NativeSurface() const override { return {}; }
        Vector2 Size() const override { return { 800, 600 }; }
        Vector2 PixelSize() const override { return { 800, 600 }; }
        float Scale() const override { return 1.0f; }
        bool IsOpen() const override { return true; }
        bool IsFocused() const override { return true; }
        WindowMode Mode() const override { return WindowMode::Normal; }
        bool IsTransparent() const override { return false; }
        bool VerticalSync() const override { return true; }
        bool ClaimSurface() override { return true; }
        void ReleaseSurface() override {}
        void SetCursor(Cursor) override {}
        void SetTextInput(bool, easyforge::Rectangle) override {}
        ColorScheme SystemColorScheme() const override { return ColorScheme::Light; }
        std::string ClipboardText() const override { return {}; }
        void SetClipboardText(std::string_view) override {}
        void AddListener(HostListener& listener) override { Listeners.push_back(&listener); }
        void RemoveListener(HostListener& listener) override
        {
            Listeners.erase(std::remove(Listeners.begin(), Listeners.end(), &listener), Listeners.end());
        }
        std::shared_ptr<void>& Shared(std::string_view name) override { return SharedObjects[std::string(name)]; }

        void Send(Event event)
        {
            for (HostListener* listener : Listeners)
            {
                listener->HandleEvent(event);
            }
        }

        void RunFrame(float deltaSeconds, const std::function<void()>& onFrame)
        {
            for (HostListener* listener : Listeners)
            {
                listener->FrameStarted(deltaSeconds);
            }
            onFrame();
            for (HostListener* listener : Listeners)
            {
                listener->FrameEnded();
            }
        }

        std::vector<HostListener*> Listeners;
        std::map<std::string, std::shared_ptr<void>> SharedObjects;
    };
}

EASYFORGE_TEST(ControlsFollowAHost)
{
    auto host = std::make_shared<TestHost>();
    {
        Controls controls = Controls::New(host);
        controls.Bind("Jump", Key::Space);
        EASYFORGE_EXPECT_EQUAL(host->Listeners.size(), std::size_t { 1 });

        Event press;
        press.Type = EventType::KeyPressed;
        press.Key = Key::Space;
        host->Send(press);

        bool pressedInFrame = false;
        host->RunFrame(0.016f, [&] { pressedInFrame = controls.Pressed("Jump"); });
        EASYFORGE_EXPECT(pressedInFrame);
        EASYFORGE_EXPECT_NEAR(controls.FrameSeconds(), 0.016f, 0.00001f);

        bool pressedAgain = true;
        host->RunFrame(0.016f, [&] { pressedAgain = controls.Pressed("Jump"); });
        EASYFORGE_EXPECT(!pressedAgain);
        EASYFORGE_EXPECT(controls.Held("Jump"));

        // A press the interface used does not reach the game.
        Event handled;
        handled.Type = EventType::KeyPressed;
        handled.Key = Key::E;
        handled.Handled = true;
        host->Send(handled);
        host->RunFrame(0.016f, [] {});
        EASYFORGE_EXPECT(!controls.Held(Key::E));
    }
    // The last handle going away stops listening.
    EASYFORGE_EXPECT(host->Listeners.empty());
}

EASYFORGE_TEST(ControlsWithoutGamepadsReadNothing)
{
    // Whatever is plugged in, reading must be safe and give a sane result.
    Controls controls = Controls::New();
    controls.NextFrame();
    for (const GamepadState& gamepad : controls.Gamepads())
    {
        EASYFORGE_EXPECT(gamepad.LeftTrigger >= 0.0f && gamepad.LeftTrigger <= 1.0f);
        EASYFORGE_EXPECT(Length(gamepad.LeftStick) <= 1.5f);
    }
    controls.Rumble(0.0f, 0.0f, 0.0f);
    controls.NextFrame();
}

#ifdef _WIN32
EASYFORGE_TEST(XInputReadingsBecomeGamepadStates)
{
    using internal::XInputReading;
    // A held, the directional pad up, the right trigger all the way, the left
    // stick fully up and to the left.
    XInputReading reading;
    reading.Buttons = 0x1000 | 0x0001;
    reading.RightTrigger = 255;
    reading.LeftTrigger = 40;
    reading.LeftStickX = -32768;
    reading.LeftStickY = 32767;
    GamepadState gamepad = internal::GamepadFromXInput(reading);

    EASYFORGE_EXPECT(gamepad.Connected);
    EASYFORGE_EXPECT(gamepad.Held(GamepadButton::South));
    EASYFORGE_EXPECT(gamepad.Held(GamepadButton::Up));
    EASYFORGE_EXPECT(!gamepad.Held(GamepadButton::East));
    EASYFORGE_EXPECT(gamepad.Held(GamepadButton::RightTrigger));
    EASYFORGE_EXPECT(!gamepad.Held(GamepadButton::LeftTrigger));
    EASYFORGE_EXPECT_EQUAL(gamepad.RightTrigger, 1.0f);
    EASYFORGE_EXPECT_EQUAL(gamepad.LeftStick, Vector2(-1, 1));
    EASYFORGE_EXPECT_EQUAL(gamepad.Value(GamepadAxis::LeftStickY), 1.0f);
}
#endif
