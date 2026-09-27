#include <easyforge/core/Testing.h>

#include <algorithm>
#include <cmath>

#include "TestData.h"

using namespace easyforge;

namespace
{
    float LargestDifference(const std::vector<float>& first, const std::vector<float>& second)
    {
        if (first.size() != second.size())
        {
            return 1e9f;
        }
        float largest = 0.0f;
        for (std::size_t index = 0; index < first.size(); ++index)
        {
            largest = Max(largest, std::abs(first[index] - second[index]));
        }
        return largest;
    }
}

EASYFORGE_TEST(WavEveryFormat)
{
    struct Case
    {
        const char* Name;
        int Channels;
    };
    for (Case test : { Case { "pcm16_stereo", 2 }, Case { "pcm8_mono", 1 }, Case { "pcm24_stereo", 2 },
             Case { "pcm32_mono", 1 }, Case { "float32_stereo", 2 }, Case { "float64_mono", 1 } })
    {
        std::string name = test.Name;
        SoundData sound = SoundData::Load(testdata::Path("sounds/" + name + ".wav"));
        if (!sound)
        {
            testing::ReportFailure(__FILE__, __LINE__, name + ": " + sound.Error());
            continue;
        }
        EASYFORGE_EXPECT_EQUAL(sound.SampleRate, 8000);
        EASYFORGE_EXPECT_EQUAL(sound.ChannelCount, test.Channels);
        EASYFORGE_EXPECT_EQUAL(sound.FrameCount(), std::size_t { 300 });
        float difference = LargestDifference(sound.Samples, testdata::Floats("sounds/" + name + ".f32"));
        if (difference > 1e-6f)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::format("{}: samples differ by up to {}", name, difference));
        }
    }
}

EASYFORGE_TEST(QoaMatchesReferenceDecoding)
{
    SoundData sound = SoundData::Load(testdata::Path("sounds/music.qoa"));
    EASYFORGE_REQUIRE(sound);
    EASYFORGE_EXPECT_EQUAL(sound.SampleRate, 22050);
    EASYFORGE_EXPECT_EQUAL(sound.ChannelCount, 2);
    EASYFORGE_EXPECT_EQUAL(sound.FrameCount(), std::size_t { 12000 });
    EASYFORGE_EXPECT_NEAR(sound.Duration(), 12000.0 / 22050.0, 1e-9);

    // Exactly what the encoder's own model of the decoder produced.
    EASYFORGE_EXPECT(LargestDifference(sound.Samples, testdata::Floats("sounds/music_qoa_decoded.f32")) < 1e-7f);

    // And close to the original sound. QOA loses a little, most at the very start
    // while its predictor adapts.
    std::vector<float> source = testdata::Floats("sounds/music_qoa_source.f32");
    EASYFORGE_REQUIRE(source.size() == sound.Samples.size());
    double squares = 0.0;
    float largestAfterStart = 0.0f;
    for (std::size_t index = 0; index < source.size(); ++index)
    {
        float difference = std::abs(sound.Samples[index] - source[index]);
        squares += static_cast<double>(difference) * difference;
        if (index >= 400)
        {
            largestAfterStart = Max(largestAfterStart, difference);
        }
    }
    EASYFORGE_EXPECT(std::sqrt(squares / static_cast<double>(source.size())) < 0.005);
    EASYFORGE_EXPECT(largestAfterStart < 0.02f);
}

EASYFORGE_TEST(SoundStreamReadsInPieces)
{
    SoundData whole = SoundData::Load(testdata::Path("sounds/music.qoa"));
    SoundStream stream = SoundStream::Open(testdata::Path("sounds/music.qoa"));
    EASYFORGE_REQUIRE(whole && stream);
    EASYFORGE_EXPECT_EQUAL(stream.FrameCount(), std::uint64_t { 12000 });

    std::vector<float> collected;
    std::vector<float> buffer(777 * 2);
    for (;;)
    {
        std::size_t frames = stream.Read(buffer);
        if (frames == 0)
        {
            break;
        }
        collected.insert(collected.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(frames * 2));
    }
    EASYFORGE_EXPECT(collected == whole.Samples);

    // Seeking into the second QOA frame and reading gives the same samples.
    EASYFORGE_REQUIRE(stream.Seek(6000));
    std::vector<float> part(100 * 2);
    EASYFORGE_EXPECT_EQUAL(stream.Read(part), std::size_t { 100 });
    EASYFORGE_EXPECT(std::equal(part.begin(), part.end(), whole.Samples.begin() + 12000));

    EASYFORGE_EXPECT(!stream.Seek(12001));
    EASYFORGE_REQUIRE(stream.Seek(12000));
    EASYFORGE_EXPECT_EQUAL(stream.Read(part), std::size_t { 0 });
}

EASYFORGE_TEST(SoundStreamWav)
{
    SoundStream stream = SoundStream::Open(testdata::Path("sounds/pcm16_stereo.wav"));
    EASYFORGE_REQUIRE(stream);
    EASYFORGE_EXPECT_EQUAL(stream.ChannelCount(), 2);
    EASYFORGE_EXPECT_EQUAL(stream.SampleRate(), 8000);

    EASYFORGE_REQUIRE(stream.Seek(250));
    std::vector<float> buffer(1000);
    EASYFORGE_EXPECT_EQUAL(stream.Read(buffer), std::size_t { 50 });

    std::vector<float> expected = testdata::Floats("sounds/pcm16_stereo.f32");
    EASYFORGE_EXPECT_NEAR(buffer[0], expected[500], 1e-7f);
}

EASYFORGE_TEST(SoundErrors)
{
    std::vector<std::uint8_t> text = { 'n', 'o', 't', ' ', 'a', ' ', 's', 'o', 'u', 'n', 'd' };
    SoundData unknown = SoundData::Decode(text, "notes.txt");
    EASYFORGE_EXPECT(!unknown);
    EASYFORGE_EXPECT(unknown.Error().starts_with("notes.txt: "));

    SoundStream missing = SoundStream::Open(testdata::Path("sounds/missing.qoa"));
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(!missing.Error().empty());
    std::vector<float> buffer(10);
    EASYFORGE_EXPECT_EQUAL(missing.Read(buffer), std::size_t { 0 });

    SoundData made(44100, 2, std::vector<float>(3));
    EASYFORGE_EXPECT(!made);

    SoundData good(44100, 1, std::vector<float>(441));
    EASYFORGE_EXPECT(good);
    EASYFORGE_EXPECT_NEAR(good.Duration(), 0.01, 1e-12);
}
