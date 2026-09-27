#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace easyforge::internal
{
    // Reads numbers from a block of bytes without ever reading past its end.
    // A read past the end returns zero and marks the reader as failed, so a
    // decoder can read a whole header and check Failed() once afterwards.
    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const std::uint8_t> bytes) : Bytes(bytes) {}

        std::size_t Size() const { return Bytes.size(); }
        std::size_t Position() const { return Offset; }
        std::size_t Remaining() const { return Offset <= Bytes.size() ? Bytes.size() - Offset : 0; }
        bool Failed() const { return HasFailed; }
        bool AtEnd() const { return Offset >= Bytes.size(); }

        bool Has(std::size_t count) const { return count <= Remaining(); }

        void Seek(std::size_t position)
        {
            if (position > Bytes.size())
            {
                HasFailed = true;
                Offset = Bytes.size();
                return;
            }
            Offset = position;
        }

        void Skip(std::size_t count)
        {
            if (!Has(count))
            {
                HasFailed = true;
                Offset = Bytes.size();
                return;
            }
            Offset += count;
        }

        // The next `count` bytes, or an empty span (and failure) if there are not that many.
        std::span<const std::uint8_t> Take(std::size_t count)
        {
            if (!Has(count))
            {
                HasFailed = true;
                Offset = Bytes.size();
                return {};
            }
            std::span<const std::uint8_t> result = Bytes.subspan(Offset, count);
            Offset += count;
            return result;
        }

        bool Matches(std::string_view text)
        {
            std::span<const std::uint8_t> taken = Take(text.size());
            if (taken.size() != text.size())
            {
                return false;
            }
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (taken[index] != static_cast<std::uint8_t>(text[index]))
                {
                    return false;
                }
            }
            return true;
        }

        std::uint8_t U8()
        {
            if (!Has(1))
            {
                HasFailed = true;
                return 0;
            }
            return Bytes[Offset++];
        }

        std::int8_t I8() { return static_cast<std::int8_t>(U8()); }

        std::uint16_t U16Big() { return static_cast<std::uint16_t>(ReadBig(2)); }
        std::uint32_t U24Big() { return static_cast<std::uint32_t>(ReadBig(3)); }
        std::uint32_t U32Big() { return static_cast<std::uint32_t>(ReadBig(4)); }
        std::uint64_t U64Big() { return ReadBig(8); }
        std::int16_t I16Big() { return static_cast<std::int16_t>(U16Big()); }
        std::int32_t I32Big() { return static_cast<std::int32_t>(U32Big()); }

        std::uint16_t U16Little() { return static_cast<std::uint16_t>(ReadLittle(2)); }
        std::uint32_t U24Little() { return static_cast<std::uint32_t>(ReadLittle(3)); }
        std::uint32_t U32Little() { return static_cast<std::uint32_t>(ReadLittle(4)); }
        std::uint64_t U64Little() { return ReadLittle(8); }
        std::int16_t I16Little() { return static_cast<std::int16_t>(U16Little()); }
        std::int32_t I32Little() { return static_cast<std::int32_t>(U32Little()); }

    private:
        std::uint64_t ReadBig(std::size_t count)
        {
            if (!Has(count))
            {
                HasFailed = true;
                Offset = Bytes.size();
                return 0;
            }
            std::uint64_t value = 0;
            for (std::size_t index = 0; index < count; ++index)
            {
                value = (value << 8) | Bytes[Offset + index];
            }
            Offset += count;
            return value;
        }

        std::uint64_t ReadLittle(std::size_t count)
        {
            if (!Has(count))
            {
                HasFailed = true;
                Offset = Bytes.size();
                return 0;
            }
            std::uint64_t value = 0;
            for (std::size_t index = 0; index < count; ++index)
            {
                value |= static_cast<std::uint64_t>(Bytes[Offset + index]) << (8 * index);
            }
            Offset += count;
            return value;
        }

        std::span<const std::uint8_t> Bytes;
        std::size_t Offset = 0;
        bool HasFailed = false;
    };
}
