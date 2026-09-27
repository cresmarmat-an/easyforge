#include <easyforge/core/Testing.h>
#include <easyforge/input.h>

using namespace easyforge;

namespace
{
    Event KeyEvent(EventType type, Key key, bool repeat = false)
    {
        Event event;
        event.Type = type;
        event.Key = key;
        event.Repeat = repeat;
        return event;
    }

    Event Press(Key key) { return KeyEvent(EventType::KeyPressed, key); }
    Event Release(Key key) { return KeyEvent(EventType::KeyReleased, key); }

    Event MouseEvent(EventType type, MouseButton button = MouseButton::Left, Vector2 position = {})
    {
        Event event;
        event.Type = type;
        event.Button = button;
        event.Position = position;
        return event;
    }
}

EASYFORGE_TEST(ActionsPressHoldAndRelease)
{
    Controls controls = Controls::New();
    controls.Bind("Jump", { Key::Space, MouseButton::Left });

    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Held("Jump"));
    EASYFORGE_EXPECT(!controls.Pressed("Jump"));

    controls.Handle(Press(Key::Space));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Held("Jump"));
    EASYFORGE_EXPECT(controls.Pressed("Jump"));
    EASYFORGE_EXPECT_EQUAL(controls.Value("Jump"), 1.0f);

    // Held keys send repeated presses, which are not new presses.
    controls.Handle(KeyEvent(EventType::KeyPressed, Key::Space, true));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Held("Jump"));
    EASYFORGE_EXPECT(!controls.Pressed("Jump"));

    // A second binding going down while the first is held is not a new press.
    controls.Handle(MouseEvent(EventType::MouseButtonPressed));
    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Pressed("Jump"));

    controls.Handle(Release(Key::Space));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Held("Jump"));
    EASYFORGE_EXPECT(!controls.Released("Jump"));

    controls.Handle(MouseEvent(EventType::MouseButtonReleased));
    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Held("Jump"));
    EASYFORGE_EXPECT(controls.Released("Jump"));

    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Released("Jump"));
}

EASYFORGE_TEST(TapWithinOneFrameIsPressedAndReleased)
{
    Controls controls = Controls::New();
    controls.Bind("Fire", Key::F);
    controls.Handle(Press(Key::F));
    controls.Handle(Release(Key::F));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Pressed("Fire"));
    EASYFORGE_EXPECT(controls.Released("Fire"));
    EASYFORGE_EXPECT(!controls.Held("Fire"));
}

EASYFORGE_TEST(KeyAxisGivesADirection)
{
    Controls controls = Controls::New();
    controls.Bind("Move", KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W });
    controls.Bind("Steer", KeyAxis { .Left = Key::Left, .Right = Key::Right });

    controls.Handle(Press(Key::W));
    controls.NextFrame();
    EASYFORGE_EXPECT_EQUAL(controls.Axis("Move"), Vector2(0, 1));

    // Two keys at once point diagonally, no longer than one.
    controls.Handle(Press(Key::D));
    controls.NextFrame();
    EASYFORGE_EXPECT_NEAR(controls.Axis("Move").X, 0.7071f, 0.001f);
    EASYFORGE_EXPECT_NEAR(controls.Axis("Move").Y, 0.7071f, 0.001f);
    EASYFORGE_EXPECT_NEAR(controls.Value("Move"), 1.0f, 0.001f);

    // Opposite keys cancel.
    controls.Handle(Press(Key::A));
    controls.Handle(Release(Key::W));
    controls.NextFrame();
    EASYFORGE_EXPECT_EQUAL(controls.Axis("Move"), Vector2());
    EASYFORGE_EXPECT(!controls.Held("Move"));

    controls.Handle(Press(Key::Left));
    controls.NextFrame();
    EASYFORGE_EXPECT_EQUAL(controls.Axis("Steer"), Vector2(-1, 0));
}

EASYFORGE_TEST(MouseWheelAndText)
{
    Controls controls = Controls::New();
    controls.Bind("Next weapon", WheelDirection::Up);

    Event moved;
    moved.Type = EventType::MouseMoved;
    moved.Position = { 10, 20 };
    moved.Movement = { 3, 4 };
    controls.Handle(moved);
    moved.Position = { 12, 20 };
    moved.Movement = { 2, 0 };
    controls.Handle(moved);

    Event wheel;
    wheel.Type = EventType::MouseWheel;
    wheel.Wheel = { 0, 1 };
    controls.Handle(wheel);

    Event typed;
    typed.Type = EventType::TextEntered;
    typed.Text = "h";
    controls.Handle(typed);
    typed.Text = "i";
    controls.Handle(typed);
    controls.NextFrame();

    EASYFORGE_EXPECT_EQUAL(controls.MousePosition(), Vector2(12, 20));
    EASYFORGE_EXPECT_EQUAL(controls.MouseMovement(), Vector2(5, 4));
    EASYFORGE_EXPECT_EQUAL(controls.Wheel(), Vector2(0, 1));
    EASYFORGE_EXPECT_EQUAL(controls.Text(), std::string("hi"));
    EASYFORGE_EXPECT(controls.Pressed("Next weapon"));

    // Turning the wheel again in the next frame is another press.
    controls.Handle(wheel);
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Pressed("Next weapon"));

    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Pressed("Next weapon"));
    EASYFORGE_EXPECT_EQUAL(controls.MouseMovement(), Vector2());
    EASYFORGE_EXPECT(controls.Text().empty());
    EASYFORGE_EXPECT_EQUAL(controls.MousePosition(), Vector2(12, 20));
}

EASYFORGE_TEST(HandledPressesAreSkipped)
{
    Controls controls = Controls::New();
    controls.Bind("Jump", Key::Space);
    Event handled = Press(Key::Space);
    handled.Handled = true;
    controls.Handle(handled);
    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Held("Jump"));

    controls.SkipHandledEvents = false;
    controls.Handle(handled);
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Held("Jump"));

    // Releases always count, so nothing stays stuck down.
    controls.SkipHandledEvents = true;
    Event release = Release(Key::Space);
    release.Handled = true;
    controls.Handle(release);
    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.Held("Jump"));
}

EASYFORGE_TEST(BindingsAddListAndRemove)
{
    Controls controls = Controls::New();
    controls.Bind("Jump", Key::Space);
    controls.Bind("Jump", { GamepadButton::South, Key::Space });
    controls.Bind("Crouch", Key::LeftControl);

    std::vector<Binding> jump = controls.Bindings("Jump");
    EASYFORGE_REQUIRE(jump.size() == 2);
    EASYFORGE_EXPECT(jump[0] == Binding(Key::Space));
    EASYFORGE_EXPECT(jump[1] == Binding(GamepadButton::South));
    EASYFORGE_EXPECT(controls.Actions() == std::vector<std::string>({ "Crouch", "Jump" }));

    controls.Unbind("Crouch");
    EASYFORGE_EXPECT(controls.Bindings("Crouch").empty());
    EASYFORGE_EXPECT(!controls.Held("Crouch"));
    controls.UnbindAll();
    EASYFORGE_EXPECT(controls.Actions().empty());

    // Handles share one set of bindings.
    Controls copy = controls;
    copy.Bind("Use", Key::E);
    EASYFORGE_EXPECT_EQUAL(controls.Bindings("Use").size(), std::size_t { 1 });
}

EASYFORGE_TEST(BindingsDirectAndLastPressed)
{
    Controls controls = Controls::New();
    controls.Handle(Press(Key::LeftShift));
    controls.Handle(Press(Key::G));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Held(Key::LeftShift));
    EASYFORGE_EXPECT(controls.Pressed(Key::G));
    EASYFORGE_EXPECT(!controls.Held(Key::H));
    std::optional<Binding> last = controls.LastPressed();
    EASYFORGE_REQUIRE(last.has_value());
    EASYFORGE_EXPECT(*last == Binding(Key::LeftShift));

    controls.NextFrame();
    EASYFORGE_EXPECT(!controls.LastPressed().has_value());
}

EASYFORGE_TEST(BindingNamesForPeople)
{
    EASYFORGE_EXPECT_EQUAL(Binding(Key::Space).Name(), std::string("Space"));
    EASYFORGE_EXPECT_EQUAL(Binding(MouseButton::Right).Name(), std::string("Right Mouse Button"));
    EASYFORGE_EXPECT_EQUAL(Binding(WheelDirection::Down).Name(), std::string("Wheel Down"));
    EASYFORGE_EXPECT_EQUAL(Binding(GamepadButton::South).Name(), std::string("South Button"));
    EASYFORGE_EXPECT_EQUAL(Binding(GamepadButton::Up).Name(), std::string("Directional Pad Up"));
    EASYFORGE_EXPECT_EQUAL(Binding(Stick::Left).Name(), std::string("Left Stick"));
    EASYFORGE_EXPECT_EQUAL(Binding(GamepadAxis::RightTrigger).Name(), std::string("Right Trigger"));
    Binding keys = KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W };
    EASYFORGE_EXPECT_EQUAL(keys.Name(), std::string("W A S D"));
    EASYFORGE_EXPECT(keys.IsDirection());
    EASYFORGE_EXPECT(!Binding(Key::A).IsDirection());
    EASYFORGE_EXPECT(Binding(Key::A).As<Key>() != nullptr);
    EASYFORGE_EXPECT(Binding(Key::A).As<Stick>() == nullptr);
}

EASYFORGE_TEST(BindingsTextRoundTrip)
{
    Controls controls = Controls::New();
    controls.Bind("Jump", { Key::Space, GamepadButton::South });
    controls.Bind("Move", { Stick::Left, KeyAxis { .Left = Key::A, .Right = Key::D, .Down = Key::S, .Up = Key::W } });
    controls.Bind("Steer", KeyAxis { .Left = Key::Left, .Right = Key::Right });
    controls.Bind("Zoom", { WheelDirection::Up, GamepadAxis::RightTrigger, MouseButton::Middle });

    std::string text = controls.BindingsText();
    EASYFORGE_EXPECT_EQUAL(text, std::string(
        "Jump = Key Space, GamepadButton South\n"
        "Move = Stick Left, KeyAxis A D S W\n"
        "Steer = KeyAxis Left Right None None\n"
        "Zoom = Wheel Up, GamepadAxis RightTrigger, MouseButton Middle\n"));

    Controls loaded = Controls::New();
    EASYFORGE_REQUIRE(loaded.SetBindingsText("# player one\n\n" + text));
    EASYFORGE_EXPECT_EQUAL(loaded.BindingsText(), text);
}

EASYFORGE_TEST(BindingsTextErrorsNameTheLine)
{
    Controls controls = Controls::New();
    controls.Bind("Keep", Key::K);

    Result<> result = controls.SetBindingsText("Jump = Key Space\nFire = Key Spacebar\n");
    EASYFORGE_EXPECT(!result);
    EASYFORGE_EXPECT(result.Error().find("line 2") != std::string::npos);
    EASYFORGE_EXPECT(result.Error().find("Spacebar") != std::string::npos);
    // A failed load changes nothing.
    EASYFORGE_EXPECT(controls.Actions() == std::vector<std::string>({ "Keep" }));

    EASYFORGE_EXPECT(!controls.SetBindingsText("Jump Key Space"));
    EASYFORGE_EXPECT(!controls.SetBindingsText("Move = KeyAxis A D S"));
    EASYFORGE_EXPECT(!controls.SetBindingsText("Move = Joystick Left"));
    EASYFORGE_EXPECT(controls.SetBindingsText(""));
    EASYFORGE_EXPECT(controls.Actions().empty());
}

EASYFORGE_TEST(BindingsSaveAndLoadFiles)
{
    std::string path = EASYFORGE_TEST_OUTPUT "bindings.txt";
    Controls controls = Controls::New();
    controls.Bind("Jump", { Key::Space, GamepadButton::South });
    EASYFORGE_REQUIRE(controls.SaveBindings(path));

    Controls loaded = Controls::New();
    EASYFORGE_REQUIRE(loaded.LoadBindings(path));
    EASYFORGE_EXPECT(loaded.Bindings("Jump") == controls.Bindings("Jump"));

    Result<> missing = loaded.LoadBindings(EASYFORGE_TEST_OUTPUT "missing.txt");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(missing.Error().find("missing.txt") != std::string::npos);
}
