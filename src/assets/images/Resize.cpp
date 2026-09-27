#include <easyforge/assets/ImageData.h>

#include <array>
#include <cmath>
#include <format>

#include "ImageDecoders.h"

namespace easyforge
{
    namespace
    {
        // Enough steps that every byte survives the trip to linear light and back.
        constexpr int LinearSteps = 8191;

        const std::array<float, 256>& ByteToLinear()
        {
            static const std::array<float, 256> table = [] {
                std::array<float, 256> result {};
                for (int value = 0; value < 256; ++value)
                {
                    float component = static_cast<float>(value) / 255.0f;
                    result[static_cast<std::size_t>(value)] =
                        component <= 0.04045f ? component / 12.92f : std::pow((component + 0.055f) / 1.055f, 2.4f);
                }
                return result;
            }();
            return table;
        }

        const std::array<std::uint8_t, LinearSteps + 1>& LinearToByte()
        {
            static const std::array<std::uint8_t, LinearSteps + 1> table = [] {
                std::array<std::uint8_t, LinearSteps + 1> result {};
                for (int step = 0; step <= LinearSteps; ++step)
                {
                    float linear = static_cast<float>(step) / LinearSteps;
                    float component =
                        linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
                    result[static_cast<std::size_t>(step)] = static_cast<std::uint8_t>(std::lround(component * 255.0f));
                }
                return result;
            }();
            return table;
        }

        // The source pixels that make up one destination pixel, and how much each counts.
        struct Taps
        {
            int First = 0;
            std::vector<float> Weights;
        };

        std::vector<Taps> ComputeTaps(int sourceSize, int destinationSize)
        {
            std::vector<Taps> result(static_cast<std::size_t>(destinationSize));
            double scale = static_cast<double>(sourceSize) / destinationSize;
            for (int index = 0; index < destinationSize; ++index)
            {
                Taps& taps = result[static_cast<std::size_t>(index)];
                if (scale > 1.0)
                {
                    // Shrinking: each source pixel counts by how much of it the
                    // destination pixel covers.
                    double start = index * scale;
                    double end = start + scale;
                    taps.First = static_cast<int>(std::floor(start));
                    int last = Min(static_cast<int>(std::ceil(end)), sourceSize) - 1;
                    for (int source = taps.First; source <= last; ++source)
                    {
                        double covered = Min(end, source + 1.0) - Max(start, static_cast<double>(source));
                        taps.Weights.push_back(static_cast<float>(covered / scale));
                    }
                }
                else
                {
                    // Growing: blend the two nearest source pixels, repeating the edges.
                    double center = (index + 0.5) * scale - 0.5;
                    int left = static_cast<int>(std::floor(center));
                    float fraction = static_cast<float>(center - left);
                    int clampedLeft = Clamp(left, 0, sourceSize - 1);
                    int clampedRight = Clamp(left + 1, 0, sourceSize - 1);
                    taps.First = clampedLeft;
                    if (clampedRight == clampedLeft)
                    {
                        taps.Weights = { 1.0f };
                    }
                    else
                    {
                        taps.Weights = { 1.0f - fraction, fraction };
                    }
                }
            }
            return result;
        }
    }

    ImageData ImageData::Resized(int width, int height) const
    {
        ImageData failed;
        if (!*this)
        {
            failed.ErrorText = "cannot resize an empty image";
            return failed;
        }
        if (Result<> size = internal::CheckImageSize(width, height); !size)
        {
            failed.ErrorText = std::format("cannot resize to {} by {}: {}", width, height, size.Error());
            return failed;
        }
        if (width == Width && height == Height)
        {
            return *this;
        }

        // Linear light with alpha multiplied in, four floats a pixel.
        const std::array<float, 256>& toLinear = ByteToLinear();
        std::vector<float> source(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4);
        for (std::size_t pixel = 0; pixel < source.size(); pixel += 4)
        {
            float alpha = Pixels[pixel + 3] / 255.0f;
            source[pixel + 0] = toLinear[Pixels[pixel + 0]] * alpha;
            source[pixel + 1] = toLinear[Pixels[pixel + 1]] * alpha;
            source[pixel + 2] = toLinear[Pixels[pixel + 2]] * alpha;
            source[pixel + 3] = alpha;
        }

        // Across, then down.
        std::vector<Taps> across = ComputeTaps(Width, width);
        std::vector<float> between(static_cast<std::size_t>(width) * static_cast<std::size_t>(Height) * 4, 0.0f);
        for (int y = 0; y < Height; ++y)
        {
            const float* row = source.data() + static_cast<std::size_t>(y) * Width * 4;
            float* output = between.data() + static_cast<std::size_t>(y) * width * 4;
            for (int x = 0; x < width; ++x)
            {
                const Taps& taps = across[static_cast<std::size_t>(x)];
                for (std::size_t tap = 0; tap < taps.Weights.size(); ++tap)
                {
                    const float* input = row + (static_cast<std::size_t>(taps.First) + tap) * 4;
                    float weight = taps.Weights[tap];
                    for (int channel = 0; channel < 4; ++channel)
                    {
                        output[x * 4 + channel] += input[channel] * weight;
                    }
                }
            }
        }

        std::vector<Taps> down = ComputeTaps(Height, height);
        const std::array<std::uint8_t, LinearSteps + 1>& toByte = LinearToByte();
        ImageData result(width, height);
        std::vector<float> sum(static_cast<std::size_t>(width) * 4);
        for (int y = 0; y < height; ++y)
        {
            std::fill(sum.begin(), sum.end(), 0.0f);
            const Taps& taps = down[static_cast<std::size_t>(y)];
            for (std::size_t tap = 0; tap < taps.Weights.size(); ++tap)
            {
                const float* input = between.data() + (static_cast<std::size_t>(taps.First) + tap) * width * 4;
                float weight = taps.Weights[tap];
                for (std::size_t index = 0; index < sum.size(); ++index)
                {
                    sum[index] += input[index] * weight;
                }
            }

            std::uint8_t* output = result.Pixels.data() + static_cast<std::size_t>(y) * result.Stride();
            for (int x = 0; x < width; ++x)
            {
                const float* pixel = sum.data() + x * 4;
                float alpha = Clamp(pixel[3], 0.0f, 1.0f);
                std::uint8_t* written = output + x * 4;
                written[3] = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
                if (written[3] == 0)
                {
                    written[0] = written[1] = written[2] = 0;
                    continue;
                }
                for (int channel = 0; channel < 3; ++channel)
                {
                    float linear = Clamp(pixel[channel] / pixel[3], 0.0f, 1.0f);
                    written[channel] = toByte[static_cast<std::size_t>(std::lround(linear * LinearSteps))];
                }
            }
        }
        return result;
    }
}
