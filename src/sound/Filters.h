#pragma once

#include <cmath>
#include <numbers>

namespace easyforge::internal
{
    // A second-order filter for one channel, with the low-pass and high-pass
    // formulas from Robert Bristow-Johnson's audio filter cookbook, run in
    // transposed direct form II.
    struct Biquad
    {
        float B0 = 1.0f;
        float B1 = 0.0f;
        float B2 = 0.0f;
        float A1 = 0.0f;
        float A2 = 0.0f;
        float Z1 = 0.0f;
        float Z2 = 0.0f;

        void SetLowPass(float cutoff, float sampleRate) { Set(cutoff, sampleRate, false); }
        void SetHighPass(float cutoff, float sampleRate) { Set(cutoff, sampleRate, true); }

        void Reset()
        {
            Z1 = 0.0f;
            Z2 = 0.0f;
        }

        float Process(float input)
        {
            float output = B0 * input + Z1;
            Z1 = B1 * input - A1 * output + Z2;
            Z2 = B2 * input - A2 * output;
            return output;
        }

    private:
        void Set(float cutoff, float sampleRate, bool high)
        {
            constexpr double quality = std::numbers::sqrt2 / 2.0;
            double angle = 2.0 * std::numbers::pi * cutoff / sampleRate;
            double cosine = std::cos(angle);
            double alpha = std::sin(angle) / (2.0 * quality);
            double a0 = 1.0 + alpha;
            double side = high ? (1.0 + cosine) / 2.0 : (1.0 - cosine) / 2.0;
            B0 = static_cast<float>(side / a0);
            B1 = static_cast<float>((high ? -2.0 * side : 2.0 * side) / a0);
            B2 = B0;
            A1 = static_cast<float>(-2.0 * cosine / a0);
            A2 = static_cast<float>((1.0 - alpha) / a0);
        }
    };

    // A low-pass and a high-pass filter for two channels, each off at 0 hertz.
    struct StereoFilters
    {
        float LowPass = 0.0f;
        float HighPass = 0.0f;
        Biquad Low[2];
        Biquad High[2];

        bool Active() const { return LowPass > 0.0f || HighPass > 0.0f; }

        // Cutoffs at or past most of the highest frequency the rate can hold do
        // nothing, so they turn the filter off.
        void Set(float lowPass, float highPass, float sampleRate)
        {
            float highest = sampleRate * 0.45f;
            lowPass = lowPass > 0.0f && lowPass < highest ? lowPass : 0.0f;
            highPass = highPass > 0.0f && highPass < highest ? highPass : 0.0f;
            if (lowPass > 0.0f)
            {
                if (LowPass == 0.0f)
                {
                    Low[0].Reset();
                    Low[1].Reset();
                }
                Low[0].SetLowPass(lowPass, sampleRate);
                Low[1].SetLowPass(lowPass, sampleRate);
            }
            if (highPass > 0.0f)
            {
                if (HighPass == 0.0f)
                {
                    High[0].Reset();
                    High[1].Reset();
                }
                High[0].SetHighPass(highPass, sampleRate);
                High[1].SetHighPass(highPass, sampleRate);
            }
            LowPass = lowPass;
            HighPass = highPass;
        }

        void Process(float& left, float& right)
        {
            if (LowPass > 0.0f)
            {
                left = Low[0].Process(left);
                right = Low[1].Process(right);
            }
            if (HighPass > 0.0f)
            {
                left = High[0].Process(left);
                right = High[1].Process(right);
            }
        }
    };

    // Cubic Hermite interpolation between `at` and `next`, `amount` of the way,
    // using the points either side for the curve.
    inline float Hermite(float before, float at, float next, float after, float amount)
    {
        float slope0 = (next - before) * 0.5f;
        float slope1 = (after - at) * 0.5f;
        float difference = at - next;
        float cubic = slope0 + slope1 + 2.0f * difference;
        float square = -(difference + slope0 + cubic);
        return ((cubic * amount + square) * amount + slope0) * amount + at;
    }
}
