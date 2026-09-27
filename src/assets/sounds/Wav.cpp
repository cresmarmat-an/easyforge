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
        constexpr std::uint16_t Pcm = 1;
        constexpr std::uint16_t FloatingPoint = 3;
        constexpr std::uint16_t Extensible = 0xFFFE;

        class WavDecoder final : public SoundDecoder
        {
        public:
            std::shared_ptr<ByteInput> Input;
            std::uint64_t DataStart = 0;
            int BitsPerSample = 0;
            bool IsFloat = false;
            int BlockSize = 0;
            std::uint64_t Position = 0;
            std::vector<std::uint8_t> Buffer;

            std::size_t Read(std::span<float> output) override
            {
                std::size_t channels = static_cast<std::size_t>(ChannelCount);
                std::size_t wanted = output.size() / channels;
                std::size_t left = static_cast<std::size_t>(FrameCount - Position);
                std::size_t frames = wanted < left ? wanted : left;
                if (frames == 0)
                {
                    return 0;
                }

                Buffer.resize(frames * static_cast<std::size_t>(BlockSize));
                std::size_t got = Input->ReadAt(DataStart + Position * static_cast<std::uint64_t>(BlockSize), Buffer);
                frames = got / static_cast<std::size_t>(BlockSize);

                std::size_t bytesPerSample = static_cast<std::size_t>(BitsPerSample / 8);
                for (std::size_t sample = 0; sample < frames * channels; ++sample)
                {
                    const std::uint8_t* bytes = Buffer.data() + sample * bytesPerSample;
                    output[sample] = Convert(bytes);
                }
                Position += frames;
                return frames;
            }

            Result<> Seek(std::uint64_t frame) override
            {
                if (frame > FrameCount)
                {
                    return Failure(std::format("frame {} is past the end, at {}", frame, FrameCount));
                }
                Position = frame;
                return {};
            }

        private:
            float Convert(const std::uint8_t* bytes) const
            {
                if (IsFloat)
                {
                    if (BitsPerSample == 32)
                    {
                        std::uint32_t bits = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
                                             (static_cast<std::uint32_t>(bytes[2]) << 16) |
                                             (static_cast<std::uint32_t>(bytes[3]) << 24);
                        float value = 0.0f;
                        std::memcpy(&value, &bits, sizeof(value));
                        return value;
                    }
                    std::uint64_t bits = 0;
                    for (int index = 0; index < 8; ++index)
                    {
                        bits |= static_cast<std::uint64_t>(bytes[index]) << (8 * index);
                    }
                    double value = 0.0;
                    std::memcpy(&value, &bits, sizeof(value));
                    return static_cast<float>(value);
                }
                switch (BitsPerSample)
                {
                case 8:
                    return (static_cast<float>(bytes[0]) - 128.0f) / 128.0f;
                case 16:
                    return static_cast<float>(static_cast<std::int16_t>(bytes[0] | (bytes[1] << 8))) / 32768.0f;
                case 24:
                {
                    std::int32_t value = static_cast<std::int32_t>(
                        (static_cast<std::uint32_t>(bytes[0]) << 8) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
                        (static_cast<std::uint32_t>(bytes[2]) << 24));
                    return static_cast<float>(value >> 8) / 8388608.0f;
                }
                default:
                {
                    std::int32_t value = static_cast<std::int32_t>(
                        static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
                        (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24));
                    return static_cast<float>(static_cast<double>(value) / 2147483648.0);
                }
                }
            }
        };
    }

    Result<std::unique_ptr<SoundDecoder>> OpenWav(std::shared_ptr<ByteInput> input)
    {
        std::array<std::uint8_t, 12> header {};
        if (input->ReadAt(0, header) != header.size() || std::memcmp(header.data(), "RIFF", 4) != 0 ||
            std::memcmp(header.data() + 8, "WAVE", 4) != 0)
        {
            return Failure("it is not a WAV file");
        }

        auto decoder = std::make_unique<WavDecoder>();
        bool haveFormat = false;
        bool haveData = false;
        std::uint64_t dataSize = 0;
        std::uint16_t format = 0;
        int blockAlign = 0;

        std::uint64_t position = 12;
        std::uint64_t size = input->Size();
        while (position + 8 <= size && !(haveFormat && haveData))
        {
            std::array<std::uint8_t, 8> chunkHeader {};
            input->ReadAt(position, chunkHeader);
            ByteReader reader(chunkHeader);
            std::string identifier(reinterpret_cast<const char*>(chunkHeader.data()), 4);
            reader.Skip(4);
            std::uint64_t chunkSize = reader.U32Little();
            std::uint64_t chunkStart = position + 8;

            if (identifier == "fmt ")
            {
                std::vector<std::uint8_t> bytes(static_cast<std::size_t>(chunkSize < 64 ? chunkSize : 64));
                input->ReadAt(chunkStart, bytes);
                ByteReader formatReader(bytes);
                format = formatReader.U16Little();
                decoder->ChannelCount = formatReader.U16Little();
                decoder->SampleRate = static_cast<int>(formatReader.U32Little());
                formatReader.Skip(4);
                blockAlign = formatReader.U16Little();
                decoder->BitsPerSample = formatReader.U16Little();
                if (format == Extensible && formatReader.Has(10))
                {
                    formatReader.Skip(8);
                    format = formatReader.U16Little();
                }
                if (formatReader.Failed())
                {
                    return Failure("its fmt chunk is too short");
                }
                haveFormat = true;
            }
            else if (identifier == "data")
            {
                decoder->DataStart = chunkStart;
                dataSize = chunkSize;
                haveData = true;
            }
            // Chunks are padded to an even number of bytes.
            position = chunkStart + chunkSize + (chunkSize & 1);
        }

        if (!haveFormat || !haveData)
        {
            return Failure(haveFormat ? "it has no data chunk" : "it has no fmt chunk");
        }
        if (format != Pcm && format != FloatingPoint)
        {
            return Failure(std::format("it uses format {}; only PCM and floating point WAV are supported", format));
        }
        decoder->IsFloat = format == FloatingPoint;
        int bits = decoder->BitsPerSample;
        bool validBits = decoder->IsFloat ? (bits == 32 || bits == 64) : (bits == 8 || bits == 16 || bits == 24 || bits == 32);
        if (!validBits)
        {
            return Failure(std::format("it uses {}-bit {} samples, which are not supported", bits,
                decoder->IsFloat ? "floating point" : "integer"));
        }
        if (decoder->ChannelCount <= 0 || decoder->ChannelCount > 16 || decoder->SampleRate <= 0)
        {
            return Failure("its channel count or sample rate is not valid");
        }
        decoder->BlockSize = decoder->ChannelCount * bits / 8;
        if (blockAlign != decoder->BlockSize)
        {
            return Failure("its block size does not match its channel count and sample size");
        }

        // A data chunk that claims more than the file holds was cut off; use what is there.
        std::uint64_t available = size > decoder->DataStart ? size - decoder->DataStart : 0;
        if (dataSize > available)
        {
            dataSize = available;
        }
        decoder->FrameCount = dataSize / static_cast<std::uint64_t>(decoder->BlockSize);
        decoder->Input = std::move(input);
        return std::unique_ptr<SoundDecoder>(std::move(decoder));
    }
}
