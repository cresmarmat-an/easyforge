#include <easyforge/assets/Compression.h>

#include <array>
#include <string>

namespace easyforge
{
    namespace
    {
        constexpr int MaximumCodeLength = 15;
        constexpr int FastBits = 10;

        // Reads bits least significant first, the order deflate stores them in.
        class BitReader
        {
        public:
            explicit BitReader(std::span<const std::uint8_t> data) : Data(data) {}

            // Makes sure at least `count` bits are buffered, if the data has them.
            void Fill(int count)
            {
                while (BufferedCount < count && Position < Data.size())
                {
                    Buffer |= static_cast<std::uint64_t>(Data[Position++]) << BufferedCount;
                    BufferedCount += 8;
                }
            }

            // Peeks at up to `count` bits. Bits past the end of the data read as zero.
            std::uint32_t Peek(int count)
            {
                Fill(count);
                return static_cast<std::uint32_t>(Buffer & ((1ull << count) - 1));
            }

            // Removes bits. Returns false when that many bits were not available.
            bool Consume(int count)
            {
                if (count > BufferedCount)
                {
                    return false;
                }
                Buffer >>= count;
                BufferedCount -= count;
                return true;
            }

            bool Read(int count, std::uint32_t& value)
            {
                if (count == 0)
                {
                    value = 0;
                    return true;
                }
                value = Peek(count);
                return Consume(count);
            }

            // Drops the bits left in the current byte, for stored blocks.
            void AlignToByte() { Consume(BufferedCount % 8); }

            // Hands back whole buffered bytes, then reads the rest from the data.
            bool ReadBytes(std::size_t count, std::vector<std::uint8_t>& output)
            {
                while (count > 0 && BufferedCount >= 8)
                {
                    output.push_back(static_cast<std::uint8_t>(Buffer & 0xFF));
                    Consume(8);
                    --count;
                }
                if (count > Data.size() - Position)
                {
                    return false;
                }
                output.insert(output.end(), Data.begin() + static_cast<std::ptrdiff_t>(Position),
                    Data.begin() + static_cast<std::ptrdiff_t>(Position + count));
                Position += count;
                return true;
            }

            // Bytes of the input not yet read into the bit buffer, plus whole buffered bytes.
            std::size_t UnreadBytes() const { return Data.size() - Position + static_cast<std::size_t>(BufferedCount / 8); }

        private:
            std::span<const std::uint8_t> Data;
            std::size_t Position = 0;
            std::uint64_t Buffer = 0;
            int BufferedCount = 0;
        };

        // A canonical Huffman code, decoded with a table for codes up to FastBits
        // long and one bit at a time for longer ones.
        class HuffmanCode
        {
        public:
            // Builds the code from each symbol's code length. False when the lengths
            // describe more codes than fit, which only damaged data does.
            bool Build(const std::uint8_t* lengths, int symbolCount)
            {
                Counts.fill(0);
                Fast.fill(0);
                for (int symbol = 0; symbol < symbolCount; ++symbol)
                {
                    ++Counts[lengths[symbol]];
                }
                Counts[0] = 0;

                int left = 1;
                for (int length = 1; length <= MaximumCodeLength; ++length)
                {
                    left = left * 2 - Counts[length];
                    if (left < 0)
                    {
                        return false;
                    }
                }

                std::array<int, MaximumCodeLength + 2> offsets {};
                for (int length = 1; length <= MaximumCodeLength; ++length)
                {
                    offsets[length + 1] = offsets[length] + Counts[length];
                }
                for (int symbol = 0; symbol < symbolCount; ++symbol)
                {
                    if (lengths[symbol] != 0)
                    {
                        Symbols[offsets[lengths[symbol]]++] = static_cast<std::uint16_t>(symbol);
                    }
                }

                // Every code of FastBits or fewer bits fills all table entries that
                // start with it. Codes are stored reversed because deflate sends
                // them most significant bit first inside a least-significant-first stream.
                int code = 0;
                int index = 0;
                for (int length = 1; length <= FastBits; ++length)
                {
                    for (int count = 0; count < Counts[length]; ++count)
                    {
                        int reversed = 0;
                        for (int bit = 0; bit < length; ++bit)
                        {
                            reversed |= ((code >> bit) & 1) << (length - 1 - bit);
                        }
                        for (int entry = reversed; entry < (1 << FastBits); entry += 1 << length)
                        {
                            Fast[entry] = static_cast<std::uint16_t>((length << 12) | Symbols[index]);
                        }
                        ++code;
                        ++index;
                    }
                    code <<= 1;
                }
                return true;
            }

            // The next symbol, or -1 for a code that does not exist.
            int Decode(BitReader& bits) const
            {
                std::uint16_t entry = Fast[bits.Peek(FastBits)];
                if (entry != 0)
                {
                    return bits.Consume(entry >> 12) ? (entry & 0xFFF) : -1;
                }

                int code = 0;
                int first = 0;
                int index = 0;
                for (int length = 1; length <= MaximumCodeLength; ++length)
                {
                    std::uint32_t bit = 0;
                    if (!bits.Read(1, bit))
                    {
                        return -1;
                    }
                    code |= static_cast<int>(bit);
                    int count = Counts[length];
                    if (code - count < first)
                    {
                        return Symbols[index + (code - first)];
                    }
                    index += count;
                    first += count;
                    first <<= 1;
                    code <<= 1;
                }
                return -1;
            }

        private:
            std::array<std::uint16_t, MaximumCodeLength + 1> Counts {};
            std::array<std::uint16_t, 288> Symbols {};
            std::array<std::uint16_t, 1 << FastBits> Fast {};
        };

        constexpr std::array<std::uint16_t, 29> LengthBase = {
            3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
        };
        constexpr std::array<std::uint8_t, 29> LengthExtraBits = {
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
        };
        constexpr std::array<std::uint16_t, 30> DistanceBase = {
            1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
            4097, 6145, 8193, 12289, 16385, 24577,
        };
        constexpr std::array<std::uint8_t, 30> DistanceExtraBits = {
            0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
        };

        Failure Damaged(const char* reason)
        {
            return Failure(std::string("the compressed data is damaged: ") + reason);
        }

        // Decodes literals and copies until the end-of-block symbol.
        Result<> InflateBlock(BitReader& bits, const HuffmanCode& literals, const HuffmanCode& distances,
            std::vector<std::uint8_t>& output, std::size_t limit)
        {
            for (;;)
            {
                int symbol = literals.Decode(bits);
                if (symbol < 0)
                {
                    return Damaged("an unknown literal code");
                }
                if (symbol < 256)
                {
                    if (output.size() >= limit)
                    {
                        return Failure("the decompressed data is larger than expected");
                    }
                    output.push_back(static_cast<std::uint8_t>(symbol));
                    continue;
                }
                if (symbol == 256)
                {
                    return {};
                }

                symbol -= 257;
                if (symbol >= static_cast<int>(LengthBase.size()))
                {
                    return Damaged("a length code out of range");
                }
                std::uint32_t extra = 0;
                if (!bits.Read(LengthExtraBits[static_cast<std::size_t>(symbol)], extra))
                {
                    return Damaged("it ends in the middle of a length");
                }
                std::size_t length = LengthBase[static_cast<std::size_t>(symbol)] + extra;

                int distanceSymbol = distances.Decode(bits);
                if (distanceSymbol < 0 || distanceSymbol >= static_cast<int>(DistanceBase.size()))
                {
                    return Damaged("an unknown distance code");
                }
                if (!bits.Read(DistanceExtraBits[static_cast<std::size_t>(distanceSymbol)], extra))
                {
                    return Damaged("it ends in the middle of a distance");
                }
                std::size_t distance = DistanceBase[static_cast<std::size_t>(distanceSymbol)] + extra;
                if (distance > output.size())
                {
                    return Damaged("a copy reaches back before the start");
                }
                if (length > limit - output.size())
                {
                    return Failure("the decompressed data is larger than expected");
                }

                // Copies byte by byte because a copy may overlap what it is writing.
                std::size_t from = output.size() - distance;
                for (std::size_t index = 0; index < length; ++index)
                {
                    output.push_back(output[from + index]);
                }
            }
        }

        Result<> ReadDynamicCodes(BitReader& bits, HuffmanCode& literals, HuffmanCode& distances)
        {
            std::uint32_t literalCount = 0;
            std::uint32_t distanceCount = 0;
            std::uint32_t codeLengthCount = 0;
            if (!bits.Read(5, literalCount) || !bits.Read(5, distanceCount) || !bits.Read(4, codeLengthCount))
            {
                return Damaged("it ends inside a block header");
            }
            literalCount += 257;
            distanceCount += 1;
            codeLengthCount += 4;
            if (literalCount > 286 || distanceCount > 30)
            {
                return Damaged("a block declares too many codes");
            }

            constexpr std::array<int, 19> order = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            std::array<std::uint8_t, 19> codeLengthLengths {};
            for (std::uint32_t index = 0; index < codeLengthCount; ++index)
            {
                std::uint32_t length = 0;
                if (!bits.Read(3, length))
                {
                    return Damaged("it ends inside a block header");
                }
                codeLengthLengths[static_cast<std::size_t>(order[index])] = static_cast<std::uint8_t>(length);
            }

            HuffmanCode codeLengths;
            if (!codeLengths.Build(codeLengthLengths.data(), 19))
            {
                return Damaged("the code length code is invalid");
            }

            std::array<std::uint8_t, 286 + 30> lengths {};
            std::uint32_t total = literalCount + distanceCount;
            std::uint32_t index = 0;
            while (index < total)
            {
                int symbol = codeLengths.Decode(bits);
                if (symbol < 0)
                {
                    return Damaged("an unknown code length code");
                }
                if (symbol < 16)
                {
                    lengths[index++] = static_cast<std::uint8_t>(symbol);
                    continue;
                }

                std::uint8_t repeated = 0;
                std::uint32_t repeat = 0;
                std::uint32_t extra = 0;
                if (symbol == 16)
                {
                    if (index == 0)
                    {
                        return Damaged("it repeats a code length before any was given");
                    }
                    repeated = lengths[index - 1];
                    if (!bits.Read(2, extra))
                    {
                        return Damaged("it ends inside a block header");
                    }
                    repeat = 3 + extra;
                }
                else if (symbol == 17)
                {
                    if (!bits.Read(3, extra))
                    {
                        return Damaged("it ends inside a block header");
                    }
                    repeat = 3 + extra;
                }
                else
                {
                    if (!bits.Read(7, extra))
                    {
                        return Damaged("it ends inside a block header");
                    }
                    repeat = 11 + extra;
                }
                if (index + repeat > total)
                {
                    return Damaged("code lengths run past the end of the table");
                }
                for (std::uint32_t count = 0; count < repeat; ++count)
                {
                    lengths[index++] = repeated;
                }
            }

            if (lengths[256] == 0)
            {
                return Damaged("a block has no end-of-block code");
            }
            if (!literals.Build(lengths.data(), static_cast<int>(literalCount)) ||
                !distances.Build(lengths.data() + literalCount, static_cast<int>(distanceCount)))
            {
                return Damaged("a Huffman code is invalid");
            }
            return {};
        }

        const HuffmanCode& FixedLiterals()
        {
            static const HuffmanCode code = [] {
                std::array<std::uint8_t, 288> lengths {};
                for (std::size_t symbol = 0; symbol < 288; ++symbol)
                {
                    lengths[symbol] = symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
                }
                HuffmanCode built;
                built.Build(lengths.data(), 288);
                return built;
            }();
            return code;
        }

        const HuffmanCode& FixedDistances()
        {
            static const HuffmanCode code = [] {
                std::array<std::uint8_t, 30> lengths {};
                lengths.fill(5);
                HuffmanCode built;
                built.Build(lengths.data(), 30);
                return built;
            }();
            return code;
        }

        Result<> Inflate(BitReader& bits, std::vector<std::uint8_t>& output, std::size_t limit)
        {
            for (;;)
            {
                std::uint32_t last = 0;
                std::uint32_t type = 0;
                if (!bits.Read(1, last) || !bits.Read(2, type))
                {
                    return Damaged("it ends before the last block");
                }

                if (type == 0)
                {
                    bits.AlignToByte();
                    std::uint32_t length = 0;
                    std::uint32_t complement = 0;
                    if (!bits.Read(16, length) || !bits.Read(16, complement))
                    {
                        return Damaged("it ends inside a stored block header");
                    }
                    if ((length ^ 0xFFFF) != complement)
                    {
                        return Damaged("a stored block's length check does not match");
                    }
                    if (length > limit - output.size())
                    {
                        return Failure("the decompressed data is larger than expected");
                    }
                    if (!bits.ReadBytes(length, output))
                    {
                        return Damaged("a stored block is cut short");
                    }
                }
                else if (type == 1)
                {
                    Result<> block = InflateBlock(bits, FixedLiterals(), FixedDistances(), output, limit);
                    if (!block)
                    {
                        return block;
                    }
                }
                else if (type == 2)
                {
                    HuffmanCode literals;
                    HuffmanCode distances;
                    Result<> codes = ReadDynamicCodes(bits, literals, distances);
                    if (!codes)
                    {
                        return codes;
                    }
                    Result<> block = InflateBlock(bits, literals, distances, output, limit);
                    if (!block)
                    {
                        return block;
                    }
                }
                else
                {
                    return Damaged("a block has the reserved type 3");
                }

                if (last != 0)
                {
                    return {};
                }
            }
        }

        const std::array<std::uint32_t, 256>& CrcTable()
        {
            static const std::array<std::uint32_t, 256> table = [] {
                std::array<std::uint32_t, 256> built {};
                for (std::uint32_t index = 0; index < 256; ++index)
                {
                    std::uint32_t value = index;
                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value & 1) ? 0xEDB88320u ^ (value >> 1) : value >> 1;
                    }
                    built[index] = value;
                }
                return built;
            }();
            return table;
        }
    }

    Result<std::vector<std::uint8_t>> Decompress(
        std::span<const std::uint8_t> data, CompressedFormat format, std::size_t expectedSize, std::size_t maximumSize)
    {
        std::vector<std::uint8_t> output;
        output.reserve(expectedSize < maximumSize ? expectedSize : maximumSize);

        if (format == CompressedFormat::Deflate)
        {
            BitReader bits(data);
            Result<> inflated = Inflate(bits, output, maximumSize);
            if (!inflated)
            {
                return Failure(inflated.Error());
            }
            return output;
        }

        if (data.size() < 6)
        {
            return Damaged("it is too short to be zlib data");
        }
        std::uint8_t method = data[0];
        std::uint8_t flags = data[1];
        if ((method & 0x0F) != 8 || (method >> 4) > 7)
        {
            return Failure("the data is not zlib data compressed with deflate");
        }
        if (((method << 8) | flags) % 31 != 0)
        {
            return Damaged("the zlib header check does not match");
        }
        if ((flags & 0x20) != 0)
        {
            return Failure("zlib data that needs a preset dictionary is not supported");
        }

        BitReader bits(data.subspan(2, data.size() - 6));
        Result<> inflated = Inflate(bits, output, maximumSize);
        if (!inflated)
        {
            return Failure(inflated.Error());
        }

        std::span<const std::uint8_t> stored = data.subspan(data.size() - 4);
        std::uint32_t expected = (static_cast<std::uint32_t>(stored[0]) << 24) |
                                 (static_cast<std::uint32_t>(stored[1]) << 16) |
                                 (static_cast<std::uint32_t>(stored[2]) << 8) | stored[3];
        if (Adler32(output) != expected)
        {
            return Damaged("the Adler-32 checksum does not match");
        }
        return output;
    }

    std::uint32_t Crc32(std::span<const std::uint8_t> data, std::uint32_t previous)
    {
        const std::array<std::uint32_t, 256>& table = CrcTable();
        std::uint32_t crc = previous ^ 0xFFFFFFFFu;
        for (std::uint8_t byte : data)
        {
            crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }

    std::uint32_t Adler32(std::span<const std::uint8_t> data, std::uint32_t previous)
    {
        constexpr std::uint32_t modulus = 65521;
        std::uint32_t low = previous & 0xFFFF;
        std::uint32_t high = previous >> 16;

        // 5552 bytes is the most that can be added before the sums could overflow.
        std::size_t position = 0;
        while (position < data.size())
        {
            std::size_t end = position + 5552 < data.size() ? position + 5552 : data.size();
            for (; position < end; ++position)
            {
                low += data[position];
                high += low;
            }
            low %= modulus;
            high %= modulus;
        }
        return (high << 16) | low;
    }
}
