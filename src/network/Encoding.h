#pragma once

// Little-endian reading and writing of the values packets and messages carry.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace easyforge::internal::networking
{
    class ByteWriter
    {
    public:
        explicit ByteWriter(std::vector<std::uint8_t>& bytes) : Bytes(bytes) {}

        void Write8(std::uint8_t value) { Bytes.push_back(value); }

        void Write16(std::uint16_t value)
        {
            Write8(static_cast<std::uint8_t>(value));
            Write8(static_cast<std::uint8_t>(value >> 8));
        }

        void Write32(std::uint32_t value)
        {
            Write16(static_cast<std::uint16_t>(value));
            Write16(static_cast<std::uint16_t>(value >> 16));
        }

        void Write64(std::uint64_t value)
        {
            Write32(static_cast<std::uint32_t>(value));
            Write32(static_cast<std::uint32_t>(value >> 32));
        }

        void WriteFloat(float value) { Write32(std::bit_cast<std::uint32_t>(value)); }
        void WriteDouble(double value) { Write64(std::bit_cast<std::uint64_t>(value)); }

        void WriteBytes(std::span<const std::uint8_t> bytes) { Bytes.insert(Bytes.end(), bytes.begin(), bytes.end()); }

        void WriteBytes(std::string_view text)
        {
            const auto* start = reinterpret_cast<const std::uint8_t*>(text.data());
            Bytes.insert(Bytes.end(), start, start + text.size());
        }

        // Text of up to 255 bytes behind its length; longer text is cut short.
        void WriteShortText(std::string_view text)
        {
            text = text.substr(0, 255);
            Write8(static_cast<std::uint8_t>(text.size()));
            WriteBytes(text);
        }

        std::size_t Size() const { return Bytes.size(); }

    private:
        std::vector<std::uint8_t>& Bytes;
    };

    // Reads values in order. Reading past the end gives zeros and marks the
    // reader as failed, so a reader checks Failed() once at the end.
    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const std::uint8_t> bytes) : Bytes(bytes) {}

        std::uint8_t Read8()
        {
            if (!Has(1))
            {
                return 0;
            }
            return Bytes[Position++];
        }

        std::uint16_t Read16()
        {
            std::uint16_t low = Read8();
            std::uint16_t high = Read8();
            return static_cast<std::uint16_t>(low | (high << 8));
        }

        std::uint32_t Read32()
        {
            std::uint32_t low = Read16();
            std::uint32_t high = Read16();
            return low | (high << 16);
        }

        std::uint64_t Read64()
        {
            std::uint64_t low = Read32();
            std::uint64_t high = Read32();
            return low | (high << 32);
        }

        float ReadFloat() { return std::bit_cast<float>(Read32()); }
        double ReadDouble() { return std::bit_cast<double>(Read64()); }

        std::span<const std::uint8_t> ReadBytes(std::size_t count)
        {
            if (!Has(count))
            {
                return {};
            }
            std::span<const std::uint8_t> bytes = Bytes.subspan(Position, count);
            Position += count;
            return bytes;
        }

        std::string ReadText(std::size_t count)
        {
            std::span<const std::uint8_t> bytes = ReadBytes(count);
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        std::string ReadShortText() { return ReadText(Read8()); }

        std::size_t Remaining() const { return Bytes.size() - Position; }
        bool IsAtEnd() const { return Position == Bytes.size(); }
        bool Failed() const { return Broken; }

    private:
        bool Has(std::size_t count)
        {
            if (Broken || Bytes.size() - Position < count)
            {
                Broken = true;
                return false;
            }
            return true;
        }

        std::span<const std::uint8_t> Bytes;
        std::size_t Position = 0;
        bool Broken = false;
    };
}
