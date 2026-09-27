#include <easyforge/assets/SoundData.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <format>

#include <easyforge/assets/Files.h>

#include "SoundDecoders.h"

namespace easyforge
{
    namespace internal
    {
        namespace
        {
            class MemoryInput final : public ByteInput
            {
            public:
                explicit MemoryInput(std::span<const std::uint8_t> bytes) : Bytes(bytes) {}

                std::uint64_t Size() const override { return Bytes.size(); }

                std::size_t ReadAt(std::uint64_t offset, std::span<std::uint8_t> output) override
                {
                    if (offset >= Bytes.size())
                    {
                        return 0;
                    }
                    std::size_t count = std::min<std::size_t>(output.size(), Bytes.size() - static_cast<std::size_t>(offset));
                    std::memcpy(output.data(), Bytes.data() + offset, count);
                    return count;
                }

            private:
                std::span<const std::uint8_t> Bytes;
            };

            class ReaderInput final : public ByteInput
            {
            public:
                explicit ReaderInput(FileReader reader) : Reader(std::move(reader)) {}

                std::uint64_t Size() const override { return Reader.Size(); }

                std::size_t ReadAt(std::uint64_t offset, std::span<std::uint8_t> output) override
                {
                    return Reader.ReadAt(offset, output);
                }

            private:
                FileReader Reader;
            };
        }

        Result<std::unique_ptr<SoundDecoder>> OpenSound(std::shared_ptr<ByteInput> input)
        {
            std::array<std::uint8_t, 4> signature {};
            if (input->ReadAt(0, signature) != signature.size())
            {
                return Failure("it is too short to be a sound file");
            }
            if (std::memcmp(signature.data(), "RIFF", 4) == 0)
            {
                return OpenWav(std::move(input));
            }
            if (std::memcmp(signature.data(), "qoaf", 4) == 0)
            {
                return OpenQoa(std::move(input));
            }
            return Failure("it is not a sound format easyforge can read (WAV or QOA)");
        }
    }

    SoundData::SoundData(int sampleRate, int channelCount, std::vector<float> samples)
        : SampleRate(sampleRate), ChannelCount(channelCount), Samples(std::move(samples))
    {
        if (sampleRate <= 0 || channelCount <= 0 || Samples.size() % static_cast<std::size_t>(channelCount) != 0)
        {
            ErrorText = std::format("a sound needs a positive sample rate and channel count, and whole frames of "
                                    "samples; got {} Hz, {} channels, {} samples",
                sampleRate, channelCount, Samples.size());
            SampleRate = 0;
            ChannelCount = 0;
            Samples.clear();
        }
    }

    SoundData SoundData::Load(std::string_view path)
    {
        Result<std::vector<std::uint8_t>> bytes = Files::Read(path);
        if (!bytes)
        {
            SoundData failed;
            failed.ErrorText = bytes.Error();
            return failed;
        }
        return Decode(*bytes, path);
    }

    SoundData SoundData::Decode(std::span<const std::uint8_t> bytes, std::string_view name)
    {
        SoundData result;
        Result<std::unique_ptr<internal::SoundDecoder>> decoder =
            internal::OpenSound(std::make_shared<internal::MemoryInput>(bytes));
        if (!decoder)
        {
            result.ErrorText = std::format("{}: {}", name, decoder.Error());
            return result;
        }

        internal::SoundDecoder& source = **decoder;
        std::uint64_t total = source.FrameCount * static_cast<std::uint64_t>(source.ChannelCount);
        result.Samples.resize(static_cast<std::size_t>(total));
        std::size_t frames = source.Read(result.Samples);
        result.Samples.resize(frames * static_cast<std::size_t>(source.ChannelCount));
        if (frames != source.FrameCount)
        {
            result.ErrorText = std::format("{}: it is cut short; {} of {} frames could be decoded", name, frames,
                source.FrameCount);
            result.Samples.clear();
            return result;
        }
        result.SampleRate = source.SampleRate;
        result.ChannelCount = source.ChannelCount;
        return result;
    }

    Pending<SoundData> SoundData::LoadInBackground(std::string_view path)
    {
        return Pending<SoundData>(Jobs::Shared().Run([path = std::string(path)] { return Load(path); }));
    }

    SoundStream SoundStream::Open(std::string_view path)
    {
        SoundStream stream;
        FileReader reader = Files::Open(path);
        if (!reader)
        {
            stream.ErrorText = reader.Error();
            return stream;
        }
        Result<std::unique_ptr<internal::SoundDecoder>> decoder =
            internal::OpenSound(std::make_shared<internal::ReaderInput>(std::move(reader)));
        if (!decoder)
        {
            stream.ErrorText = std::format("{}: {}", path, decoder.Error());
            return stream;
        }
        stream.Decoder = std::move(decoder).Get();
        return stream;
    }

    int SoundStream::SampleRate() const
    {
        return Decoder ? Decoder->SampleRate : 0;
    }

    int SoundStream::ChannelCount() const
    {
        return Decoder ? Decoder->ChannelCount : 0;
    }

    std::uint64_t SoundStream::FrameCount() const
    {
        return Decoder ? Decoder->FrameCount : 0;
    }

    std::size_t SoundStream::Read(std::span<float> output)
    {
        return Decoder ? Decoder->Read(output) : 0;
    }

    Result<> SoundStream::Seek(std::uint64_t frame)
    {
        if (!Decoder)
        {
            return Failure("the stream is not open");
        }
        return Decoder->Seek(frame);
    }
}
