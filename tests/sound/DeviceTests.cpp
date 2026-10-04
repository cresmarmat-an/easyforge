#include <chrono>
#include <thread>
#include <vector>

#include <easyforge/core/Log.h>
#include <easyforge/core/Testing.h>
#include <easyforge/sound.h>

#include "SoundTesting.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(TheDefaultDevicePlays)
{
    Mixer mixer = Mixer::New();
    if (!mixer)
    {
        // Computers without sound output, such as build servers, end here.
        EASYFORGE_EXPECT(!mixer.Error().empty());
        Log(LogLevel::Warning, "no sound device, so only offline mixing was tested: {}", mixer.Error());
        EASYFORGE_EXPECT(!mixer.Play(Steady(48000, { 0.5f }, 100)).IsPlaying());
        return;
    }
    EASYFORGE_EXPECT(mixer.Error().empty());
    EASYFORGE_EXPECT(mixer.SampleRate() >= 8000);
    EASYFORGE_EXPECT(mixer.ChannelCount() >= 1);
    EASYFORGE_EXPECT(!mixer.DeviceName().empty());

    // Played at no volume, so running the tests makes no noise.
    PlayingSound whole = mixer.Play(Steady(44100, { 0.5f }, 44100 * 2), { .Volume = 0.0f });

    std::vector<float> samples = ToneSamples(22050, 2, 200.0f, 2.0, 0.5f);
    std::string path = std::string(EASYFORGE_TEST_OUTPUT) + "device-stream.wav";
    EASYFORGE_REQUIRE(WriteWave(path, 22050, 2, samples));
    PlayingSound streamed = mixer.Play(Sound::Load(path, { .Stream = true }), { .Volume = 0.0f, .Bus = "Music" });

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EASYFORGE_EXPECT(whole.IsPlaying());
    EASYFORGE_EXPECT(streamed.IsPlaying());
    EASYFORGE_EXPECT(whole.Time() > 0.2 && whole.Time() < 1.5);
    EASYFORGE_EXPECT(streamed.Time() > 0.2 && streamed.Time() < 1.5);
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(2));

    // Render is only for offline mixers.
    std::vector<float> untouched(64, 0.25f);
    mixer.Render(untouched);
    EASYFORGE_EXPECT_EQUAL(untouched[10], 0.25f);

    mixer.StopAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EASYFORGE_EXPECT_EQUAL(mixer.PlayingCount(), std::size_t(0));
}
