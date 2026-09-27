#include <array>
#include <cstring>
#include <format>
#include <vector>

#include "../ByteReader.h"
#include "SoundDecoders.h"

namespace easyforge::internal
{
    namespace
    {
        // QOA, the Quite OK Audio format: 20 samples at a time are predicted from the
        // last four with a small adaptive filter, and the difference is stored in 3
        // bits with one of 16 scale factors.
        constexpr int SamplesPerSlice = 20;
        constexpr int SlicesPerFrame = 256;
        constexpr int FrameSamples = SamplesPerSlice * SlicesPerFrame;
        constexpr int MaximumChannels = 8;

        constexpr std::array<int, 16> ScaleFactors = {
            1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048,
        };

        // Each scale factor times 0.75, 2.5, 4.5, and 7, rounded half away from zero,
        // for the eight 3-bit codes (positive and negative).
        const std::array<std::array<int, 8>, 16>& DequantizationTable()
        {
            static const std::array<std::array<int, 8>, 16> table = [] {
                std::array<std::array<int, 8>, 16> built {};
                for (std::size_t index = 0; index < 16; ++index)
                {
                    int scale = ScaleFactors[index];
                    int quarter = (scale * 3 + 2) / 4;
                    int twoAndHalf = (scale * 5 + 1) / 2;
                    int fourAndHalf = (scale * 9 + 1) / 2;
                    int seven = scale * 7;
                    built[index] = { quarter, -quarter, twoAndHalf, -twoAndHalf, fourAndHalf, -fourAndHalf, seven, -seven };
                }
                return built;
            }();
            return table;
        }

        struct Filter
        {
            std::array<int, 4> History {};
            std::array<int, 4> Weights {};

            int Predict() const
            {
                int prediction = 0;
                for (std::size_t index = 0; index < 4; ++index)
                {
                    prediction += Weights[index] * History[index];
                }
                return prediction >> 13;
            }

            void Update(int sample, int residual)
            {
                int delta = residual >> 4;
                for (std::size_t index = 0; index < 4; ++index)
                {
                    Weights[index] += History[index] < 0 ? -delta : delta;
                }
                History = { History[1], History[2], History[3], sample };
            }
        };

        class QoaDecoder final : public SoundDecoder
        {
        public:
            std::shared_ptr<ByteInput> Input;
            std::uint64_t FullFrameSize = 0;

            // The frame being played: its samples, interleaved, and how many were used.
            std::vector<float> Decoded;
            std::size_t DecodedFrames = 0;
            std::size_t UsedFrames = 0;
            std::uint64_t NextFrameIndex = 0;
            bool Broken = false;

            std::size_t Read(std::span<float> output) override
            {
                std::size_t channels = static_cast<std::size_t>(ChannelCount);
                std::size_t wanted = output.size() / channels;
                std::size_t written = 0;
                while (written < wanted)
                {
                    if (UsedFrames == DecodedFrames && !DecodeFrame(NextFrameIndex))
                    {
                        break;
                    }
                    std::size_t count = DecodedFrames - UsedFrames;
                    if (count > wanted - written)
                    {
                        count = wanted - written;
                    }
                    std::memcpy(output.data() + written * channels, Decoded.data() + UsedFrames * channels,
                        count * channels * sizeof(float));
                    UsedFrames += count;
                    written += count;
                }
                return written;
            }

            Result<> Seek(std::uint64_t frame) override
            {
                if (frame > FrameCount)
                {
                    return Failure(std::format("frame {} is past the end, at {}", frame, FrameCount));
                }
                Broken = false;
                DecodedFrames = 0;
                UsedFrames = 0;
                NextFrameIndex = frame / FrameSamples;
                if (frame == FrameCount)
                {
                    NextFrameIndex = (FrameCount + FrameSamples - 1) / FrameSamples;
                    return {};
                }
                if (!DecodeFrame(NextFrameIndex))
                {
                    return Failure("the frame to seek to is damaged");
                }
                UsedFrames = static_cast<std::size_t>(frame % FrameSamples);
                return {};
            }

            // Decodes frame `index` into Decoded. False at the end or when it is damaged.
            bool DecodeFrame(std::uint64_t index)
            {
                if (Broken || index * FrameSamples >= FrameCount)
                {
                    return false;
                }
                std::uint64_t offset = 8 + index * FullFrameSize;
                std::vector<std::uint8_t> bytes(static_cast<std::size_t>(FullFrameSize));
                std::size_t got = Input->ReadAt(offset, bytes);
                bytes.resize(got);
                ByteReader reader(bytes);

                int channels = reader.U8();
                reader.Skip(3);
                int frameSamples = reader.U16Big();
                std::size_t frameSize = reader.U16Big();
                if (reader.Failed() || channels != ChannelCount || frameSamples > FrameSamples || frameSize > got)
                {
                    Broken = true;
                    return false;
                }

                std::array<Filter, MaximumChannels> filters {};
                for (int channel = 0; channel < channels; ++channel)
                {
                    Filter& filter = filters[static_cast<std::size_t>(channel)];
                    for (int& value : filter.History)
                    {
                        value = reader.I16Big();
                    }
                    for (int& value : filter.Weights)
                    {
                        value = reader.I16Big();
                    }
                }

                const auto& dequantize = DequantizationTable();
                Decoded.assign(static_cast<std::size_t>(frameSamples) * static_cast<std::size_t>(channels), 0.0f);
                for (int start = 0; start < frameSamples; start += SamplesPerSlice)
                {
                    for (int channel = 0; channel < channels; ++channel)
                    {
                        std::uint64_t slice = reader.U64Big();
                        Filter& filter = filters[static_cast<std::size_t>(channel)];
                        std::size_t scale = static_cast<std::size_t>(slice >> 60);
                        slice <<= 4;
                        int end = start + SamplesPerSlice < frameSamples ? start + SamplesPerSlice : frameSamples;
                        for (int sample = start; sample < end; ++sample)
                        {
                            int quantized = static_cast<int>(slice >> 61);
                            slice <<= 3;
                            int residual = dequantize[scale][static_cast<std::size_t>(quantized)];
                            int reconstructed = filter.Predict() + residual;
                            reconstructed = reconstructed < -32768 ? -32768 : reconstructed > 32767 ? 32767 : reconstructed;
                            filter.Update(reconstructed, residual);
                            Decoded[static_cast<std::size_t>(sample) * static_cast<std::size_t>(channels) +
                                    static_cast<std::size_t>(channel)] = static_cast<float>(reconstructed) / 32768.0f;
                        }
                    }
                }
                if (reader.Failed())
                {
                    Broken = true;
                    return false;
                }

                DecodedFrames = static_cast<std::size_t>(frameSamples);
                UsedFrames = 0;
                NextFrameIndex = index + 1;
                return true;
            }
        };
    }

    Result<std::unique_ptr<SoundDecoder>> OpenQoa(std::shared_ptr<ByteInput> input)
    {
        std::array<std::uint8_t, 16> header {};
        if (input->ReadAt(0, header) != header.size() || std::memcmp(header.data(), "qoaf", 4) != 0)
        {
            return Failure("it is not a QOA file");
        }
        ByteReader reader(header);
        reader.Skip(4);
        std::uint32_t samples = reader.U32Big();
        int channels = reader.U8();
        int sampleRate = static_cast<int>(reader.U24Big());

        if (samples == 0)
        {
            return Failure("it is a streaming QOA file without a length, which is not supported");
        }
        if (channels < 1 || channels > MaximumChannels || sampleRate <= 0)
        {
            return Failure("its first frame has an invalid channel count or sample rate");
        }

        // Each 20 samples of each channel take 8 bytes.
        std::uint64_t size = input->Size();
        std::uint64_t slices = size > 8 ? (size - 8) / (8 * static_cast<std::uint64_t>(channels)) : 0;
        if (samples > slices * SamplesPerSlice)
        {
            return Failure("it claims more samples than the file holds");
        }

        auto decoder = std::make_unique<QoaDecoder>();
        decoder->ChannelCount = channels;
        decoder->SampleRate = sampleRate;
        decoder->FrameCount = samples;
        decoder->FullFrameSize = 8 + 16 * static_cast<std::uint64_t>(channels) +
                                 static_cast<std::uint64_t>(SlicesPerFrame) * 8 * static_cast<std::uint64_t>(channels);
        decoder->Input = std::move(input);
        return std::unique_ptr<SoundDecoder>(std::move(decoder));
    }
}
