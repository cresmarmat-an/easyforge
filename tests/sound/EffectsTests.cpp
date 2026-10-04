#include <cmath>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/sound.h>

#include "SoundTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(BusesTurnSoundsDownTogether)
{
    Mixer mixer = Mixer::NewOffline();
    Sound steady = Steady(48000, { 0.4f, 0.4f }, 48000 * 2);
    mixer.Play(steady, { .Bus = "Music" });
    mixer.Play(steady);

    MixerBus music = mixer.Bus("Music");
    EASYFORGE_EXPECT_EQUAL(music.Name(), std::string("Music"));
    std::vector<float> output = Render(mixer, 1000);
    EASYFORGE_EXPECT_NEAR(output[500 * 2], 0.8f, 1e-6f);

    music.Volume = 0.5f;
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], 0.6f, 1e-6f);

    music.Muted = true;
    EASYFORGE_EXPECT(music.Muted.Get());
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], 0.4f, 1e-6f);

    // Asking for the bus by name again gives the same bus.
    music.Muted = false;
    mixer.Bus("Music").FadeTo(0.0f, 0.05f);
    EASYFORGE_EXPECT_EQUAL(music.Volume.Get(), 0.0f);
    output = Render(mixer, 4800);
    EASYFORGE_EXPECT_NEAR(output[4000 * 2], 0.4f, 1e-6f);

    MixerBus none;
    none.Volume = 0.3f;
    EASYFORGE_EXPECT(none.Name().empty());
}

EASYFORGE_TEST(FiltersShapeTheSound)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    float unfiltered = 0.5f * std::sqrt(0.5f);

    PlayingSound high = mixer.Play(Tone(48000, 2, 8000.0f, 1.0), { .LowPass = 500.0f });
    float muffled = RootMeanSquare(Channel(Render(mixer, 9600), 0), 2048);
    EASYFORGE_EXPECT(muffled < 0.02f);
    high.Stop();
    Render(mixer, 1024);

    PlayingSound low = mixer.Play(Tone(48000, 2, 100.0f, 1.0), { .HighPass = 2000.0f });
    float thin = RootMeanSquare(Channel(Render(mixer, 9600), 0), 2048);
    EASYFORGE_EXPECT(thin < 0.02f);
    low.Stop();
    Render(mixer, 1024);

    // What is inside the band passes almost unchanged.
    PlayingSound kept = mixer.Play(Tone(48000, 2, 100.0f, 1.0), { .LowPass = 5000.0f, .HighPass = 20.0f });
    float passed = RootMeanSquare(Channel(Render(mixer, 9600), 0), 2048);
    EASYFORGE_EXPECT_NEAR(passed, unfiltered, 0.02f);
    kept.Stop();
    Render(mixer, 1024);

    // A bus filters everything playing through it, and turns off at 0.
    MixerBus wall = mixer.Bus("Wall");
    wall.LowPass = 500.0f;
    EASYFORGE_EXPECT_EQUAL(wall.LowPass.Get(), 500.0f);
    mixer.Play(Tone(48000, 2, 8000.0f, 2.0), { .Bus = "Wall" });
    float behindWall = RootMeanSquare(Channel(Render(mixer, 9600), 0), 2048);
    EASYFORGE_EXPECT(behindWall < 0.02f);
    wall.LowPass = 0.0f;
    float open = RootMeanSquare(Channel(Render(mixer, 9600), 0), 2048);
    EASYFORGE_EXPECT_NEAR(open, unfiltered, 0.02f);
}

EASYFORGE_TEST(EchoRepeatsAfterTheDelay)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    MixerBus hall = mixer.Bus("Hall");
    hall.Echo = { .Delay = 0.1f, .Feedback = 0.5f, .Mix = 0.5f };
    EASYFORGE_EXPECT_NEAR(hall.Echo.Get().Delay, 0.1f, 1e-6f);

    Sound click = Steady(48000, { 0.8f, 0.8f }, 48);
    mixer.Play(click, { .Bus = "Hall" });
    std::vector<float> left = Channel(Render(mixer, 16800), 0);
    EASYFORGE_EXPECT_NEAR(left[10], 0.8f, 1e-6f);
    EASYFORGE_EXPECT_EQUAL(left[2400], 0.0f);
    EASYFORGE_EXPECT_NEAR(left[4800 + 10], 0.4f, 1e-6f);
    EASYFORGE_EXPECT_NEAR(left[9600 + 10], 0.2f, 1e-6f);
    EASYFORGE_EXPECT_NEAR(left[14400 + 10], 0.1f, 1e-6f);

    // Without echo, a click is just a click.
    hall.Echo = { .Delay = 0.1f, .Feedback = 0.5f, .Mix = 0.0f };
    Render(mixer, 4800);
    mixer.Play(click, { .Bus = "Hall" });
    left = Channel(Render(mixer, 9600), 0);
    EASYFORGE_EXPECT_NEAR(left[10], 0.8f, 1e-6f);
    EASYFORGE_EXPECT_EQUAL(left[4800 + 10], 0.0f);
}

EASYFORGE_TEST(PositionalSoundsPanAndFade)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    Sound mono = Steady(48000, { 0.5f }, 48000 * 4);
    PlayingSound placed = mixer.Play(mono, { .Position = Vector3 { 5, 0, 0 }, .MinimumDistance = 5.0f, .MaximumDistance = 100.0f });

    // To the right of a listener facing -Z.
    std::vector<float> output = Render(mixer, 512);
    EASYFORGE_EXPECT_NEAR(output[100 * 2], 0.0f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[100 * 2 + 1], 0.5f, 1e-5f);

    placed.Position = Vector3 { -5, 0, 0 };
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], 0.5f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2 + 1], 0.0f, 1e-5f);

    // Straight ahead at twice the minimum distance: half as loud, and even.
    placed.Position = Vector3 { 0, 0, -10 };
    output = Render(mixer, 2048);
    float even = 0.25f * std::sqrt(0.5f);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], even, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2 + 1], even, 1e-5f);

    // Turned to face +X, the same place is on the left.
    mixer.Listener.Forward = Vector3 { 1, 0, 0 };
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], 0.25f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2 + 1], 0.0f, 1e-5f);

    // Past the maximum distance, nothing.
    mixer.Listener.Position = Vector3 { 0, 0, 200 };
    EASYFORGE_EXPECT_EQUAL(mixer.Listener.Position.Get().Z, 200.0f);
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_EQUAL(output[2000 * 2], 0.0f);
    EASYFORGE_EXPECT_EQUAL(output[2000 * 2 + 1], 0.0f);
    EASYFORGE_EXPECT(placed.IsPlaying());

    // A sound given a place later becomes positional then.
    mixer.Listener.Position = Vector3 {};
    mixer.Listener.Forward = Vector3 { 0, 0, -1 };
    placed.Stop();
    PlayingSound later = mixer.Play(mono);
    Render(mixer, 1024);
    later.Position = Vector3 { 5, 0, 0 };
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], 0.0f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2 + 1], 0.1f, 1e-5f);
}

EASYFORGE_TEST(PitchCanChangeWhilePlaying)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    PlayingSound playing = mixer.Play(Tone(48000, 1, 500.0f, 4.0));
    int before = RisingCrossings(Channel(Render(mixer, 48000), 0));
    playing.Pitch = 2.0f;
    EASYFORGE_EXPECT_EQUAL(playing.Pitch.Get(), 2.0f);
    Render(mixer, 1024);
    int after = RisingCrossings(Channel(Render(mixer, 48000), 0));
    EASYFORGE_EXPECT(before >= 499 && before <= 501);
    EASYFORGE_EXPECT(after >= 999 && after <= 1001);

    // Pitch stays within what the mixer can play.
    playing.Pitch = 100.0f;
    EASYFORGE_EXPECT_EQUAL(playing.Pitch.Get(), 8.0f);
}
