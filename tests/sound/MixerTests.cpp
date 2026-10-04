#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/sound.h>

#include "SoundTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(OfflineMixerPlaysASoundAsItIs)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000, .ChannelCount = 2 });
    EASYFORGE_REQUIRE(mixer);
    EASYFORGE_EXPECT_EQUAL(mixer.SampleRate(), 48000);
    EASYFORGE_EXPECT_EQUAL(mixer.ChannelCount(), 2);
    EASYFORGE_EXPECT(mixer.DeviceName().empty());

    std::vector<float> samples = ToneSamples(48000, 2, 440.0f, 0.1, 0.5f);
    for (std::size_t index = 1; index < samples.size(); index += 2)
    {
        samples[index] *= 0.5f;   // a quieter right channel tells the two apart
    }
    Sound tone = Sound::FromData(SoundData(48000, 2, samples));
    EASYFORGE_REQUIRE(tone);
    EASYFORGE_EXPECT_EQUAL(tone.FrameCount(), std::uint64_t(4800));
    EASYFORGE_EXPECT_NEAR(tone.Duration(), 0.1, 1e-9);

    PlayingSound playing = mixer.Play(tone);
    EASYFORGE_EXPECT(playing.IsPlaying());
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(1));

    // Same rate, no pitch, no pan: every sample comes out unchanged.
    std::vector<float> output = Render(mixer, 4800);
    bool same = true;
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        same = same && output[index] == samples[index];
    }
    EASYFORGE_EXPECT(same);

    std::vector<float> after = Render(mixer, 1024);
    EASYFORGE_EXPECT_EQUAL(RootMeanSquare(after), 0.0f);
    EASYFORGE_EXPECT(!playing.IsPlaying());
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(0));
}

EASYFORGE_TEST(SoundsAddUp)
{
    Mixer mixer = Mixer::NewOffline();
    mixer.Play(Steady(48000, { 0.2f, 0.1f }, 2000));
    mixer.Play(Steady(48000, { 0.3f, 0.4f }, 2000));
    std::vector<float> output = Render(mixer, 1000);
    EASYFORGE_EXPECT_NEAR(output[500 * 2], 0.5f, 1e-6f);
    EASYFORGE_EXPECT_NEAR(output[500 * 2 + 1], 0.5f, 1e-6f);
}

EASYFORGE_TEST(VolumeAndPanning)
{
    Mixer mixer = Mixer::NewOffline();
    Sound mono = Steady(48000, { 0.8f }, 48000);

    // A mono sound in the middle reaches each speaker at equal power.
    PlayingSound middle = mixer.Play(mono, { .Volume = 0.5f });
    std::vector<float> output = Render(mixer, 256);
    float half = 0.4f * std::sqrt(0.5f);
    EASYFORGE_EXPECT_NEAR(output[100 * 2], half, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[100 * 2 + 1], half, 1e-5f);
    middle.Stop();
    Render(mixer, 2048);

    PlayingSound left = mixer.Play(mono, { .Volume = 0.5f, .Pan = -1.0f });
    output = Render(mixer, 256);
    EASYFORGE_EXPECT_NEAR(output[100 * 2], 0.4f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[100 * 2 + 1], 0.0f, 1e-5f);

    // Changes slide over about ten milliseconds, then hold.
    left.Pan = 1.0f;
    left.Volume = 0.25f;
    EASYFORGE_EXPECT_EQUAL(left.Pan.Get(), 1.0f);
    EASYFORGE_EXPECT_EQUAL(left.Volume.Get(), 0.25f);
    output = Render(mixer, 2048);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2], 0.0f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[2000 * 2 + 1], 0.2f, 1e-5f);
    left.Stop();
    Render(mixer, 2048);

    // A stereo sound panned right moves its left channel into the right.
    mixer.Play(Steady(48000, { 0.4f, 0.2f }, 48000), { .Pan = 1.0f });
    output = Render(mixer, 256);
    EASYFORGE_EXPECT_NEAR(output[100 * 2], 0.0f, 1e-5f);
    EASYFORGE_EXPECT_NEAR(output[100 * 2 + 1], 0.6f, 1e-5f);
}

EASYFORGE_TEST(ResamplingKeepsThePitch)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    PlayingSound playing = mixer.Play(Tone(22050, 1, 1000.0f, 0.5));
    std::vector<float> left = Channel(Render(mixer, 48000), 0);

    // Half a second of a 1000 hertz tone, however many frames each second has.
    int crossings = RisingCrossings(left);
    EASYFORGE_EXPECT(crossings >= 499 && crossings <= 501);
    EASYFORGE_EXPECT(!playing.IsPlaying());
    float during = RootMeanSquare(std::vector<float>(left.begin() + 1000, left.begin() + 23000));
    float after = RootMeanSquare(std::vector<float>(left.begin() + 24100, left.end()));
    EASYFORGE_EXPECT_NEAR(during, 0.5f * std::sqrt(0.5f) * std::sqrt(0.5f), 0.01f);
    EASYFORGE_EXPECT_EQUAL(after, 0.0f);

    // Pitch 2 plays it an octave higher in half the time.
    mixer.Play(Tone(22050, 1, 1000.0f, 0.5), { .Pitch = 2.0f });
    left = Channel(Render(mixer, 48000), 0);
    crossings = RisingCrossings(left);
    EASYFORGE_EXPECT(crossings >= 499 && crossings <= 501);
    float lateHalf = RootMeanSquare(std::vector<float>(left.begin() + 12200, left.end()));
    EASYFORGE_EXPECT_EQUAL(lateHalf, 0.0f);
}

EASYFORGE_TEST(StartingPartWayAndLooping)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    Sound brief = Steady(48000, { 0.5f, 0.5f }, 4800);

    PlayingSound looping = mixer.Play(brief, { .Loop = true });
    std::vector<float> output = Render(mixer, 4800 * 3 + 1200);
    EASYFORGE_EXPECT(looping.IsPlaying());
    EASYFORGE_EXPECT_NEAR(looping.Time(), 0.025, 1e-6);
    EASYFORGE_EXPECT_EQUAL(output[(4800 * 2 + 10) * 2], 0.5f);
    looping.Stop();
    Render(mixer, 1024);
    EASYFORGE_EXPECT(!looping.IsPlaying());

    PlayingSound later = mixer.Play(brief, { .Start = 0.075 });
    Render(mixer, 600);
    EASYFORGE_EXPECT(later.IsPlaying());
    EASYFORGE_EXPECT_NEAR(later.Time(), 0.0875, 1e-6);
    Render(mixer, 1024);
    EASYFORGE_EXPECT(!later.IsPlaying());
}

EASYFORGE_TEST(FadingStoppingAndPausing)
{
    Mixer mixer = Mixer::NewOffline({ .SampleRate = 48000 });
    Sound steady = Steady(48000, { 0.5f, 0.5f }, 48000 * 4);

    // Fading in starts from silence.
    PlayingSound playing = mixer.Play(steady, { .FadeIn = 0.1f });
    std::vector<float> output = Render(mixer, 4800);
    EASYFORGE_EXPECT(output[0] < 0.01f);
    EASYFORGE_EXPECT_NEAR(output[2400 * 2], 0.25f, 0.01f);
    output = Render(mixer, 100);
    EASYFORGE_EXPECT_NEAR(output[50 * 2], 0.5f, 1e-6f);

    // Fading to a volume gets there and stays.
    playing.FadeTo(0.0f, 0.05f);
    output = Render(mixer, 4800);
    EASYFORGE_EXPECT_NEAR(output[4000 * 2], 0.0f, 1e-6f);
    EASYFORGE_EXPECT(playing.IsPlaying());
    playing.Volume = 1.0f;

    // Paused, time stands still and nothing is heard.
    playing.Pause();
    EASYFORGE_EXPECT(playing.IsPaused());
    Render(mixer, 2048);
    double pausedAt = playing.Time();
    output = Render(mixer, 4800);
    EASYFORGE_EXPECT_EQUAL(RootMeanSquare(output), 0.0f);
    EASYFORGE_EXPECT_EQUAL(playing.Time(), pausedAt);
    playing.Resume();
    EASYFORGE_EXPECT(!playing.IsPaused());
    output = Render(mixer, 4800);
    EASYFORGE_EXPECT_NEAR(output[4000 * 2], 0.5f, 1e-6f);
    EASYFORGE_EXPECT(playing.Time() > pausedAt);

    // Stopping with a fade goes quiet over the fade, then ends.
    playing.Stop(0.05f);
    EASYFORGE_EXPECT(!playing.IsPlaying());
    output = Render(mixer, 4800);
    EASYFORGE_EXPECT(output[100 * 2] > 0.4f);
    EASYFORGE_EXPECT_EQUAL(output[3000 * 2], 0.0f);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(0));

    // Stopping everything at once.
    mixer.Play(steady);
    mixer.Play(steady);
    Render(mixer, 100);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(2));
    mixer.StopAll();
    Render(mixer, 1024);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(0));
}

EASYFORGE_TEST(StreamedSoundsMatchWholeOnes)
{
    std::vector<float> samples = ToneSamples(44100, 2, 330.0f, 2.5, 0.4f);
    for (std::size_t index = 0; index < samples.size(); index += 7)
    {
        samples[index] *= 0.5f;
    }
    std::string path = std::string(EASYFORGE_TEST_OUTPUT) + "stream.wav";
    EASYFORGE_REQUIRE(WriteWave(path, 44100, 2, samples));

    Sound whole = Sound::Load(path);
    Sound streamed = Sound::Load(path, { .Stream = true });
    EASYFORGE_REQUIRE(whole);
    EASYFORGE_REQUIRE(streamed);
    EASYFORGE_EXPECT(streamed.IsStreamed());
    EASYFORGE_EXPECT(!whole.IsStreamed());
    EASYFORGE_EXPECT_EQUAL(streamed.FrameCount(), whole.FrameCount());

    Mixer first = Mixer::NewOffline({ .SampleRate = 48000 });
    Mixer second = Mixer::NewOffline({ .SampleRate = 48000 });
    PlayingSound wholePlaying = first.Play(whole, { .Pitch = 1.3f });
    PlayingSound streamPlaying = second.Play(streamed, { .Pitch = 1.3f });
    std::vector<float> wholeOutput = Render(first, 48000 * 3);
    std::vector<float> streamOutput = Render(second, 48000 * 3);
    EASYFORGE_EXPECT(wholeOutput == streamOutput);
    EASYFORGE_EXPECT(!streamPlaying.IsPlaying());
    EASYFORGE_EXPECT(RootMeanSquare(streamOutput) > 0.1f);

    // A looping stream goes round, starting where it was asked to.
    PlayingSound looping = second.Play(streamed, { .Loop = true, .Start = 2.0 });
    Render(second, 48000);
    EASYFORGE_EXPECT(looping.IsPlaying());
    EASYFORGE_EXPECT_NEAR(looping.Time(), 0.5, 0.01);

    Sound missing = Sound::Load(std::string(EASYFORGE_TEST_OUTPUT) + "missing.wav", { .Stream = true });
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(!missing.Error().empty());
}

EASYFORGE_TEST(EmptyThingsDoNothing)
{
    Mixer empty;
    EASYFORGE_EXPECT(!empty);
    EASYFORGE_EXPECT(empty.Error().empty());
    PlayingSound nothing = empty.Play(Steady(48000, { 0.5f }, 100));
    EASYFORGE_EXPECT(!nothing.IsPlaying());
    nothing.Volume = 0.5f;
    nothing.Stop();
    EASYFORGE_EXPECT_EQUAL(nothing.Volume.Get(), 0.5f);
    empty.Volume = 0.5f;
    empty.Listener.Position = Vector3 { 1, 2, 3 };
    EASYFORGE_EXPECT_EQUAL(empty.Volume.Get(), 1.0f);

    Mixer mixer = Mixer::NewOffline();
    Sound none;
    EASYFORGE_EXPECT(!none);
    EASYFORGE_EXPECT(!mixer.Play(none).IsPlaying());
    Sound broken = Sound::FromData(SoundData());
    EASYFORGE_EXPECT(!broken);
    EASYFORGE_EXPECT(!broken.Error().empty());

    // A handle outlives its mixer harmlessly.
    PlayingSound orphan;
    {
        Mixer brief = Mixer::NewOffline();
        orphan = brief.Play(Steady(48000, { 0.5f }, 48000));
        EASYFORGE_EXPECT(orphan.IsPlaying());
    }
    EASYFORGE_EXPECT(!orphan.IsPlaying());
    orphan.Volume = 0.1f;
    orphan.Pause();
}

EASYFORGE_TEST(TooManySoundsAreRefused)
{
    Mixer mixer = Mixer::NewOffline();
    Sound steady = Steady(48000, { 0.0f }, 48000);
    int playing = 0;
    for (int index = 0; index < 600; ++index)
    {
        playing += mixer.Play(steady).IsPlaying() ? 1 : 0;
    }
    EASYFORGE_EXPECT_EQUAL(playing, 512);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(512));
    mixer.StopAll();
    Render(mixer, 1024);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(0));
    EASYFORGE_EXPECT(mixer.Play(steady).IsPlaying());
}

EASYFORGE_TEST(SoundsCanStartFromAnyThread)
{
    Mixer mixer = Mixer::NewOffline();
    Sound blip = Steady(48000, { 0.01f, 0.01f }, 480);
    std::atomic<int> started { 0 };
    std::atomic<bool> done { false };
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread)
    {
        threads.emplace_back([&] {
            for (int index = 0; index < 100; ++index)
            {
                PlayingSound playing = mixer.Play(blip, { .Pan = 0.5f });
                playing.Volume = 0.5f;
                ++started;
            }
        });
    }
    std::thread renderer([&] {
        while (!done)
        {
            Render(mixer, 256);
        }
    });
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    done = true;
    renderer.join();
    Render(mixer, 2048);
    EASYFORGE_EXPECT_EQUAL(started.load(), 400);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(0));
}

EASYFORGE_TEST(LoudPeaksBendBelowOne)
{
    Mixer mixer = Mixer::NewOffline();
    Sound loud = Steady(48000, { 0.5f, 0.5f }, 4800);
    for (int index = 0; index < 4; ++index)
    {
        mixer.Play(loud);
    }
    std::vector<float> output = Render(mixer, 1000);
    EASYFORGE_EXPECT(output[500] > 0.95f);
    EASYFORGE_EXPECT(output[500] <= 1.0f);

    // The mixer's own volume, after everything else.
    mixer.Volume = 0.1f;
    output = Render(mixer, 2000);
    EASYFORGE_EXPECT_NEAR(output[1500 * 2], 0.2f, 1e-5f);
}
