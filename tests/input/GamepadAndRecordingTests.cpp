#include <easyforge/core/Testing.h>
#include <easyforge/input.h>


using namespace easyforge;

namespace
{
    Event Press(Key key)
    {
        Event event;
        event.Type = EventType::KeyPressed;
        event.Key = key;
        return event;
    }

    Event Release(Key key)
    {
        Event event;
        event.Type = EventType::KeyReleased;
        event.Key = key;
        return event;
    }

    GamepadState Pad(std::initializer_list<GamepadButton> held = {}, Vector2 leftStick = {}, float rightTrigger = 0.0f)
    {
        GamepadState gamepad;
        gamepad.Connected = true;
        for (GamepadButton button : held)
        {
            gamepad.Buttons[static_cast<std::size_t>(button)] = true;
        }
        gamepad.LeftStick = leftStick;
        gamepad.RightTrigger = rightTrigger;
        gamepad.Buttons[static_cast<std::size_t>(GamepadButton::RightTrigger)] = rightTrigger > 0.25f;
        return gamepad;
    }

    // A recording with the given gamepad states for the first gamepad, one frame each.
    Recording GamepadFrames(std::initializer_list<GamepadState> states, int slot = 0)
    {
        Recording recording;
        for (const GamepadState& state : states)
        {
            RecordedFrame frame;
            frame.DeltaSeconds = 1.0f / 60.0f;
            frame.Gamepads[static_cast<std::size_t>(slot)] = state;
            recording.Frames.push_back(frame);
        }
        return recording;
    }
}

EASYFORGE_TEST(GamepadButtonsDriveActions)
{
    // An Xbox controller played back from a recording, frame by frame.
    Controls controls = Controls::New();
    controls.Bind("Jump", { Key::Space, GamepadButton::South });
    controls.Play(GamepadFrames({ Pad(), Pad({ GamepadButton::South }), Pad({ GamepadButton::South }), Pad() }));

    controls.NextFrame();
    EASYFORGE_EXPECT(controls.IsPlaying());
    EASYFORGE_EXPECT(!controls.Held("Jump"));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Pressed("Jump"));
    EASYFORGE_EXPECT(controls.LastPressed() == std::optional<Binding>(GamepadButton::South));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Held("Jump"));
    EASYFORGE_EXPECT(!controls.Pressed("Jump"));
    controls.NextFrame();
    EASYFORGE_EXPECT(controls.Released("Jump"));
    EASYFORGE_EXPECT_NEAR(controls.FrameSeconds(), 1.0f / 60.0f, 0.00001f);

    // Playing ends by itself after the last frame.
    controls.NextFrame(0.5f);
    EASYFORGE_EXPECT(!controls.IsPlaying());
    EASYFORGE_EXPECT_EQUAL(controls.FrameSeconds(), 0.5f);
}

EASYFORGE_TEST(SticksUseTheDeadZone)
{
    Controls controls = Controls::New({ .DeadZone = 0.2f });
    controls.Bind("Move", Stick::Left);
    controls.Play(GamepadFrames({ Pad({}, { 0.1f, 0.1f }), Pad({}, { 0.6f, 0.0f }), Pad({}, { 1.0f, 0.0f }) }));

    // Inside the dead zone: nothing.
    controls.NextFrame();
    EASYFORGE_EXPECT_EQUAL(controls.Axis("Move"), Vector2());

    // Past it, the rest of the travel is spread from 0 to 1.
    controls.NextFrame();
    EASYFORGE_EXPECT_NEAR(controls.Axis("Move").X, 0.5f, 0.0001f);
    EASYFORGE_EXPECT(controls.Held("Move"));
    controls.NextFrame();
    EASYFORGE_EXPECT_NEAR(controls.Axis("Move").X, 1.0f, 0.0001f);
    EASYFORGE_EXPECT_NEAR(controls.Gamepads()[0].LeftStick.X, 1.0f, 0.0001f);
}

EASYFORGE_TEST(TriggersAreAnalog)
{
    Controls controls = Controls::New();
    controls.Bind("Accelerate", GamepadButton::RightTrigger);
    controls.Bind("Throttle", GamepadAxis::RightTrigger);
    controls.Play(GamepadFrames({ Pad({}, {}, 0.1f), Pad({}, {}, 0.8f) }));

    controls.NextFrame();
    EASYFORGE_EXPECT_NEAR(controls.Value("Accelerate"), 0.1f, 0.0001f);
    EASYFORGE_EXPECT(!controls.Held("Accelerate"));
    controls.NextFrame();
    EASYFORGE_EXPECT_NEAR(controls.Value("Accelerate"), 0.8f, 0.0001f);
    EASYFORGE_EXPECT(controls.Pressed("Accelerate"));
    EASYFORGE_EXPECT_EQUAL(controls.Axis("Throttle"), Vector2(0.8f, 0));
}

EASYFORGE_TEST(EachPlayerReadsTheirOwnGamepad)
{
    Recording recording;
    RecordedFrame frame;
    frame.Gamepads[0] = Pad({ GamepadButton::South });
    frame.Gamepads[1] = Pad({}, { 0.0f, 1.0f });
    recording.Frames.push_back(frame);

    Controls first = Controls::New({ .Gamepad = 0 });
    Controls second = Controls::New({ .Gamepad = 1 });
    Controls anyone = Controls::New();
    for (Controls* controls : { &first, &second, &anyone })
    {
        controls->Bind("Jump", GamepadButton::South);
        controls->Bind("Move", Stick::Left);
        controls->Play(recording);
        controls->NextFrame();
    }
    EASYFORGE_EXPECT(first.Held("Jump"));
    EASYFORGE_EXPECT_EQUAL(first.Axis("Move"), Vector2());
    EASYFORGE_EXPECT(!second.Held("Jump"));
    EASYFORGE_EXPECT(second.Axis("Move").Y > 0.99f);
    EASYFORGE_EXPECT(anyone.Held("Jump"));
    EASYFORGE_EXPECT(anyone.Axis("Move").Y > 0.99f);

    second.Gamepad = 7;
    EASYFORGE_EXPECT_EQUAL(second.Gamepad.Get(), MaximumGamepads - 1);
}

EASYFORGE_TEST(RecordingPlaysBackExactly)
{
    // Record a short session, then play it back and check every frame reads the same.
    Controls live = Controls::New();
    live.Bind("Jump", Key::Space);
    live.Bind("Move", KeyAxis { .Left = Key::A, .Right = Key::D });

    std::vector<std::vector<Event>> session = {
        { Press(Key::D) },
        {},
        { Press(Key::Space), Release(Key::D) },
        { Release(Key::Space), Press(Key::A) },
        {},
        { Release(Key::A) },
    };

    struct Reading
    {
        bool Pressed = false;
        bool Held = false;
        Vector2 Move;
        bool operator==(const Reading&) const = default;
    };
    auto read = [](const Controls& controls) {
        return Reading { controls.Pressed("Jump"), controls.Held("Jump"), controls.Axis("Move") };
    };

    // A key already held when recording starts is part of the recording.
    live.Handle(Press(Key::LeftShift));
    live.NextFrame();

    live.StartRecording();
    EASYFORGE_EXPECT(live.IsRecording());
    std::vector<Reading> expected;
    for (std::size_t frame = 0; frame < session.size(); ++frame)
    {
        for (const Event& event : session[frame])
        {
            live.Handle(event);
        }
        live.NextFrame(0.01f * static_cast<float>(frame + 1));
        expected.push_back(read(live));
    }
    Recording recording = live.StopRecording();
    EASYFORGE_EXPECT(!live.IsRecording());
    EASYFORGE_REQUIRE(recording.Frames.size() == session.size());
    EASYFORGE_EXPECT_NEAR(recording.Duration(), 0.21f, 0.0001f);

    // Through a file and back, to prove saving keeps everything.
    std::string path = EASYFORGE_TEST_OUTPUT "session.recording";
    EASYFORGE_REQUIRE(recording.Save(path));
    Result<Recording> loaded = Recording::Load(path);
    EASYFORGE_REQUIRE(loaded);
    EASYFORGE_EXPECT(loaded->Encode() == recording.Encode());

    Controls replay = Controls::New();
    replay.Bind("Jump", Key::Space);
    replay.Bind("Move", KeyAxis { .Left = Key::A, .Right = Key::D });
    replay.Play(*loaded);
    for (std::size_t frame = 0; frame < session.size(); ++frame)
    {
        // Live events while playing are ignored.
        replay.Handle(Press(Key::Space));
        replay.NextFrame(123.0f);
        EASYFORGE_EXPECT(read(replay) == expected[frame]);
        EASYFORGE_EXPECT_NEAR(replay.FrameSeconds(), 0.01f * static_cast<float>(frame + 1), 0.00001f);
        if (frame == 0)
        {
            EASYFORGE_EXPECT(replay.Held(Key::LeftShift));
        }
    }
    replay.NextFrame();
    EASYFORGE_EXPECT(!replay.IsPlaying());
    // Nothing from the recording stays held afterwards.
    EASYFORGE_EXPECT(!replay.Held(Key::LeftShift));
}

EASYFORGE_TEST(DamagedRecordingsAreRefused)
{
    Recording recording;
    RecordedFrame frame;
    frame.DeltaSeconds = 0.016f;
    Event event;
    event.Type = EventType::FilesDropped;
    event.Files = { "one.txt", "two \xE2\x9C\x93.png" };
    event.Text = "text";
    frame.Events.push_back(event);
    frame.Gamepads[2] = Pad({ GamepadButton::North, GamepadButton::Right }, { -0.5f, 0.25f }, 1.0f);
    recording.Frames.push_back(frame);

    std::vector<std::uint8_t> bytes = recording.Encode();
    Result<Recording> decoded = Recording::Decode(bytes);
    EASYFORGE_REQUIRE(decoded);
    EASYFORGE_REQUIRE(decoded->Frames.size() == 1);
    EASYFORGE_EXPECT(decoded->Frames[0].Events[0].Files == event.Files);
    EASYFORGE_EXPECT(decoded->Frames[0].Gamepads[2] == frame.Gamepads[2]);

    // Every shorter piece of the file is refused, never read past its end.
    for (std::size_t length = 0; length < bytes.size(); ++length)
    {
        EASYFORGE_EXPECT(!Recording::Decode(std::span<const std::uint8_t>(bytes.data(), length)));
    }
    std::vector<std::uint8_t> wrongVersion = bytes;
    wrongVersion[8] = 9;
    Result<Recording> refused = Recording::Decode(wrongVersion);
    EASYFORGE_EXPECT(!refused);
    EASYFORGE_EXPECT(refused.Error().find("version 9") != std::string::npos);

    std::vector<std::uint8_t> hugeCount = bytes;
    hugeCount[12] = 0xFF;
    hugeCount[13] = 0xFF;
    hugeCount[14] = 0xFF;
    EASYFORGE_EXPECT(!Recording::Decode(hugeCount));
    EASYFORGE_EXPECT(!Recording::Load(EASYFORGE_TEST_OUTPUT "no such recording"));
}
