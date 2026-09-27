#include <array>
#include <cmath>
#include <format>
#include <vector>

#include "../ByteReader.h"
#include "ImageDecoders.h"

namespace easyforge::internal
{
    namespace
    {
        // Where each coefficient of the zigzag order goes in the 8 by 8 block.
        constexpr std::array<std::uint8_t, 64> Zigzag = {
            0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21,
            28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61,
            54, 47, 55, 62, 63,
        };

        constexpr int FastBits = 9;

        struct HuffmanTable
        {
            bool Defined = false;
            std::array<std::uint8_t, 256> Values {};
            std::array<std::int32_t, 18> MaximumCode {};
            std::array<std::int32_t, 17> ValueOffset {};
            // Symbol and length for codes of up to FastBits bits; length 0 means "use the slow path".
            std::array<std::uint16_t, 1 << FastBits> Fast {};

            bool Build(const std::array<std::uint8_t, 17>& counts, std::span<const std::uint8_t> values)
            {
                std::copy(values.begin(), values.end(), Values.begin());
                Fast.fill(0);

                int code = 0;
                int index = 0;
                for (int length = 1; length <= 16; ++length)
                {
                    ValueOffset[length] = index - code;
                    for (int count = 0; count < counts[length]; ++count)
                    {
                        // More codes than fit in this many bits: a damaged table.
                        if (code >= (1 << length))
                        {
                            return false;
                        }
                        if (length <= FastBits)
                        {
                            int shift = FastBits - length;
                            for (int fill = 0; fill < (1 << shift); ++fill)
                            {
                                Fast[static_cast<std::size_t>((code << shift) | fill)] =
                                    static_cast<std::uint16_t>((length << 8) | Values[static_cast<std::size_t>(index)]);
                            }
                        }
                        ++code;
                        ++index;
                    }
                    MaximumCode[length] = counts[length] > 0 ? code - 1 : -1;
                    code <<= 1;
                }
                MaximumCode[17] = 0x7FFFFFFF;
                Defined = true;
                return true;
            }
        };

        struct Component
        {
            int Identifier = 0;
            int Horizontal = 1;
            int Vertical = 1;
            int QuantizationTable = 0;
            int DcTable = 0;
            int AcTable = 0;
            int BlocksWide = 0;
            int BlocksHigh = 0;
            int Predictor = 0;
            std::vector<std::uint8_t> Samples;
        };

        // Reads the entropy-coded data between markers, most significant bit first.
        // A 0xFF data byte is followed by a stuffed 0x00; any other byte after 0xFF
        // is a marker, which stops the data and is left for the caller.
        class BitReader
        {
        public:
            BitReader(std::span<const std::uint8_t> bytes, std::size_t position) : Bytes(bytes), Position(position) {}

            std::uint32_t Peek(int count)
            {
                Fill(count);
                return static_cast<std::uint32_t>((Buffer >> (BufferedCount - count)) & ((1u << count) - 1));
            }

            void Consume(int count) { BufferedCount -= count; }

            std::uint32_t Read(int count)
            {
                if (count == 0)
                {
                    return 0;
                }
                std::uint32_t value = Peek(count);
                Consume(count);
                return value;
            }

            // Moves past a restart marker after dropping leftover bits. False if the
            // next marker is not the restart marker expected.
            bool Restart(int expected)
            {
                BufferedCount = 0;
                Buffer = 0;
                HitMarker = false;
                while (Position + 1 < Bytes.size() && !(Bytes[Position] == 0xFF && Bytes[Position + 1] != 0 &&
                                                        Bytes[Position + 1] != 0xFF))
                {
                    ++Position;
                }
                if (Position + 1 >= Bytes.size() || Bytes[Position + 1] != 0xD0 + expected)
                {
                    return false;
                }
                Position += 2;
                return true;
            }

            // Where the markers after this scan begin.
            std::size_t EndPosition() const { return Position; }

            bool PassedEnd() const { return Overrun > 8; }

        private:
            void Fill(int count)
            {
                while (BufferedCount < count)
                {
                    std::uint32_t byte = 0;
                    if (!HitMarker && Position < Bytes.size())
                    {
                        byte = Bytes[Position];
                        if (byte == 0xFF)
                        {
                            std::uint8_t next = Position + 1 < Bytes.size() ? Bytes[Position + 1] : 0xD9;
                            if (next == 0x00)
                            {
                                Position += 2;
                            }
                            else
                            {
                                HitMarker = true;
                                byte = 0;
                                ++Overrun;
                            }
                        }
                        else
                        {
                            ++Position;
                        }
                    }
                    else
                    {
                        ++Overrun;
                    }
                    Buffer = (Buffer << 8) | byte;
                    BufferedCount += 8;
                }
            }

            std::span<const std::uint8_t> Bytes;
            std::size_t Position;
            std::uint64_t Buffer = 0;
            int BufferedCount = 0;
            bool HitMarker = false;
            int Overrun = 0;
        };

        int DecodeSymbol(BitReader& bits, const HuffmanTable& table)
        {
            std::uint16_t entry = table.Fast[bits.Peek(FastBits)];
            if ((entry >> 8) != 0)
            {
                bits.Consume(entry >> 8);
                return entry & 0xFF;
            }
            std::int32_t code = static_cast<std::int32_t>(bits.Read(1));
            for (int length = 1; length <= 16; ++length)
            {
                if (code <= table.MaximumCode[length])
                {
                    int index = table.ValueOffset[length] + code;
                    return index >= 0 && index < 256 ? table.Values[static_cast<std::size_t>(index)] : -1;
                }
                code = (code << 1) | static_cast<std::int32_t>(bits.Read(1));
            }
            return -1;
        }

        // A value of `size` bits read as the JPEG signed magnitude encoding.
        int Extend(std::uint32_t value, int size)
        {
            if (size == 0)
            {
                return 0;
            }
            if (value < (1u << (size - 1)))
            {
                return static_cast<int>(value) - (1 << size) + 1;
            }
            return static_cast<int>(value);
        }

        const std::array<std::array<float, 8>, 8>& CosineTable()
        {
            static const std::array<std::array<float, 8>, 8> table = [] {
                std::array<std::array<float, 8>, 8> built {};
                for (int position = 0; position < 8; ++position)
                {
                    for (int frequency = 0; frequency < 8; ++frequency)
                    {
                        double scale = frequency == 0 ? std::sqrt(0.5) : 1.0;
                        built[static_cast<std::size_t>(position)][static_cast<std::size_t>(frequency)] = static_cast<float>(
                            0.5 * scale * std::cos((2.0 * position + 1.0) * frequency * 3.14159265358979323846 / 16.0));
                    }
                }
                return built;
            }();
            return table;
        }

        // Turns one block of dequantized coefficients into 8 by 8 samples.
        void InverseTransform(const std::array<float, 64>& coefficients, std::uint8_t* output, std::size_t stride)
        {
            const auto& cosine = CosineTable();
            std::array<float, 64> rows {};
            for (int row = 0; row < 8; ++row)
            {
                const float* input = coefficients.data() + row * 8;
                // Most rows of a block are zero after their first coefficient, which
                // makes the whole row one value.
                if (input[1] == 0.0f && input[2] == 0.0f && input[3] == 0.0f && input[4] == 0.0f &&
                    input[5] == 0.0f && input[6] == 0.0f && input[7] == 0.0f)
                {
                    float value = input[0] * cosine[0][0];
                    for (int x = 0; x < 8; ++x)
                    {
                        rows[static_cast<std::size_t>(row * 8 + x)] = value;
                    }
                    continue;
                }
                for (int x = 0; x < 8; ++x)
                {
                    const auto& weights = cosine[static_cast<std::size_t>(x)];
                    float sum = 0.0f;
                    for (int frequency = 0; frequency < 8; ++frequency)
                    {
                        sum += weights[static_cast<std::size_t>(frequency)] * input[frequency];
                    }
                    rows[static_cast<std::size_t>(row * 8 + x)] = sum;
                }
            }
            for (int x = 0; x < 8; ++x)
            {
                for (int y = 0; y < 8; ++y)
                {
                    const auto& weights = cosine[static_cast<std::size_t>(y)];
                    float sum = 0.0f;
                    for (int frequency = 0; frequency < 8; ++frequency)
                    {
                        sum += weights[static_cast<std::size_t>(frequency)] * rows[static_cast<std::size_t>(frequency * 8 + x)];
                    }
                    float shifted = sum + 128.5f;
                    output[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(
                        shifted <= 0.0f ? 0 : shifted >= 255.0f ? 255 : static_cast<int>(shifted));
                }
            }
        }

        struct Decoder
        {
            std::array<std::array<std::uint16_t, 64>, 4> Quantization {};
            std::array<bool, 4> QuantizationDefined {};
            std::array<HuffmanTable, 4> DcTables;
            std::array<HuffmanTable, 4> AcTables;
            std::vector<Component> Components;
            int Width = 0;
            int Height = 0;
            int MaximumHorizontal = 1;
            int MaximumVertical = 1;
            int RestartInterval = 0;
            bool HaveFrame = false;
            int AdobeTransform = -1;

            Result<> DecodeBlock(BitReader& bits, Component& component, int blockColumn, int blockRow)
            {
                const HuffmanTable& dc = DcTables[static_cast<std::size_t>(component.DcTable)];
                const HuffmanTable& ac = AcTables[static_cast<std::size_t>(component.AcTable)];
                const auto& quantization = Quantization[static_cast<std::size_t>(component.QuantizationTable)];

                std::array<float, 64> coefficients {};
                int size = DecodeSymbol(bits, dc);
                if (size < 0 || size > 11)
                {
                    return Failure("its image data has an unknown DC code");
                }
                component.Predictor += Extend(bits.Read(size), size);
                coefficients[0] = static_cast<float>(component.Predictor * quantization[0]);

                for (int index = 1; index < 64;)
                {
                    int symbol = DecodeSymbol(bits, ac);
                    if (symbol < 0)
                    {
                        return Failure("its image data has an unknown AC code");
                    }
                    int run = symbol >> 4;
                    int valueSize = symbol & 15;
                    if (valueSize == 0)
                    {
                        if (run != 15)
                        {
                            break;
                        }
                        index += 16;
                        continue;
                    }
                    index += run;
                    if (index > 63)
                    {
                        return Failure("a block has more than 64 coefficients");
                    }
                    int value = Extend(bits.Read(valueSize), valueSize);
                    coefficients[Zigzag[static_cast<std::size_t>(index)]] =
                        static_cast<float>(value * quantization[static_cast<std::size_t>(index)]);
                    ++index;
                }
                if (bits.PassedEnd())
                {
                    return Failure("its image data is cut short");
                }

                std::size_t stride = static_cast<std::size_t>(component.BlocksWide) * 8;
                std::uint8_t* output = component.Samples.data() + static_cast<std::size_t>(blockRow) * 8 * stride +
                                       static_cast<std::size_t>(blockColumn) * 8;
                InverseTransform(coefficients, output, stride);
                return {};
            }

            Result<> DecodeScan(std::span<const std::uint8_t> bytes, std::size_t& position, std::vector<int> scan)
            {
                for (int index : scan)
                {
                    Component& component = Components[static_cast<std::size_t>(index)];
                    component.Predictor = 0;
                    if (!DcTables[static_cast<std::size_t>(component.DcTable)].Defined ||
                        !AcTables[static_cast<std::size_t>(component.AcTable)].Defined)
                    {
                        return Failure("a scan uses a Huffman table that was never defined");
                    }
                    if (!QuantizationDefined[static_cast<std::size_t>(component.QuantizationTable)])
                    {
                        return Failure("a component uses a quantization table that was never defined");
                    }
                }

                BitReader bits(bytes, position);
                int restartsDone = 0;
                int unitsSinceRestart = 0;

                auto restartIfDue = [&](bool moreFollow) -> Result<> {
                    if (RestartInterval == 0 || !moreFollow)
                    {
                        return {};
                    }
                    if (++unitsSinceRestart < RestartInterval)
                    {
                        return {};
                    }
                    unitsSinceRestart = 0;
                    if (!bits.Restart(restartsDone % 8))
                    {
                        return Failure("a restart marker is missing");
                    }
                    ++restartsDone;
                    for (int index : scan)
                    {
                        Components[static_cast<std::size_t>(index)].Predictor = 0;
                    }
                    return {};
                };

                if (scan.size() == 1)
                {
                    // One component on its own: its blocks in plain order, covering only the image.
                    Component& component = Components[static_cast<std::size_t>(scan[0])];
                    int samplesWide = (Width * component.Horizontal + MaximumHorizontal - 1) / MaximumHorizontal;
                    int samplesHigh = (Height * component.Vertical + MaximumVertical - 1) / MaximumVertical;
                    int blocksWide = (samplesWide + 7) / 8;
                    int blocksHigh = (samplesHigh + 7) / 8;
                    for (int row = 0; row < blocksHigh; ++row)
                    {
                        for (int column = 0; column < blocksWide; ++column)
                        {
                            Result<> block = DecodeBlock(bits, component, column, row);
                            if (!block)
                            {
                                return block;
                            }
                            Result<> restarted = restartIfDue(row + 1 < blocksHigh || column + 1 < blocksWide);
                            if (!restarted)
                            {
                                return restarted;
                            }
                        }
                    }
                }
                else
                {
                    int unitsWide = (Width + 8 * MaximumHorizontal - 1) / (8 * MaximumHorizontal);
                    int unitsHigh = (Height + 8 * MaximumVertical - 1) / (8 * MaximumVertical);
                    for (int unitRow = 0; unitRow < unitsHigh; ++unitRow)
                    {
                        for (int unitColumn = 0; unitColumn < unitsWide; ++unitColumn)
                        {
                            for (int index : scan)
                            {
                                Component& component = Components[static_cast<std::size_t>(index)];
                                for (int y = 0; y < component.Vertical; ++y)
                                {
                                    for (int x = 0; x < component.Horizontal; ++x)
                                    {
                                        Result<> block = DecodeBlock(bits, component,
                                            unitColumn * component.Horizontal + x, unitRow * component.Vertical + y);
                                        if (!block)
                                        {
                                            return block;
                                        }
                                    }
                                }
                            }
                            Result<> restarted = restartIfDue(unitRow + 1 < unitsHigh || unitColumn + 1 < unitsWide);
                            if (!restarted)
                            {
                                return restarted;
                            }
                        }
                    }
                }

                position = bits.EndPosition();
                return {};
            }

            // For each output pixel along one direction of a subsampled component: the
            // two nearest samples and how far to blend between them. Sample centers
            // sit in the middle of the pixels they cover.
            struct Taps
            {
                std::vector<int> First;
                std::vector<int> Second;
                std::vector<float> Blend;
            };

            static Taps MakeTaps(int outputCount, int factor, int sourceCount)
            {
                Taps taps;
                taps.First.resize(static_cast<std::size_t>(outputCount));
                taps.Second.resize(static_cast<std::size_t>(outputCount));
                taps.Blend.resize(static_cast<std::size_t>(outputCount));
                for (int index = 0; index < outputCount; ++index)
                {
                    float source = (static_cast<float>(index) + 0.5f) / static_cast<float>(factor) - 0.5f;
                    int left = static_cast<int>(std::floor(source));
                    std::size_t at = static_cast<std::size_t>(index);
                    taps.Blend[at] = source - static_cast<float>(left);
                    taps.First[at] = Clamp(left, 0, sourceCount - 1);
                    taps.Second[at] = Clamp(left + 1, 0, sourceCount - 1);
                }
                return taps;
            }

            // Upsamples each component to full size, one row at a time, and converts to RGBA.
            ImageData Finish() const
            {
                ImageData image(Width, Height);
                std::size_t count = Components.size();
                std::size_t width = static_cast<std::size_t>(Width);

                std::vector<Taps> columns(count);
                std::vector<Taps> rows(count);
                for (std::size_t index = 0; index < count; ++index)
                {
                    const Component& component = Components[index];
                    int factorX = MaximumHorizontal / component.Horizontal;
                    int factorY = MaximumVertical / component.Vertical;
                    int sourceWidth = (Width * component.Horizontal + MaximumHorizontal - 1) / MaximumHorizontal;
                    int sourceHeight = (Height * component.Vertical + MaximumVertical - 1) / MaximumVertical;
                    if (factorX > 1)
                    {
                        columns[index] = MakeTaps(Width, factorX, sourceWidth);
                    }
                    if (factorY > 1)
                    {
                        rows[index] = MakeTaps(Height, factorY, sourceHeight);
                    }
                }

                // Adobe's marker says whether three components are RGB; without it, JFIF means YCbCr.
                bool convert = count == 3 && AdobeTransform != 0 &&
                               !(Components[0].Identifier == 'R' && Components[1].Identifier == 'G' &&
                                   Components[2].Identifier == 'B');

                auto toByte = [](float value) {
                    value += 0.5f;
                    return static_cast<std::uint8_t>(value <= 0.0f ? 0 : value >= 255.0f ? 255 : static_cast<int>(value));
                };

                std::vector<std::vector<float>> line(count, std::vector<float>(width));
                for (int y = 0; y < Height; ++y)
                {
                    for (std::size_t index = 0; index < count; ++index)
                    {
                        const Component& component = Components[index];
                        std::size_t stride = static_cast<std::size_t>(component.BlocksWide) * 8;
                        float* values = line[index].data();
                        std::size_t row = static_cast<std::size_t>(y);
                        const Taps& across = columns[index];
                        const Taps& down = rows[index];

                        if (down.First.empty())
                        {
                            const std::uint8_t* source = component.Samples.data() + row * stride;
                            if (across.First.empty())
                            {
                                for (std::size_t x = 0; x < width; ++x)
                                {
                                    values[x] = source[x];
                                }
                            }
                            else
                            {
                                for (std::size_t x = 0; x < width; ++x)
                                {
                                    float left = source[across.First[x]];
                                    values[x] = left + (static_cast<float>(source[across.Second[x]]) - left) * across.Blend[x];
                                }
                            }
                            continue;
                        }

                        const std::uint8_t* upper = component.Samples.data() + static_cast<std::size_t>(down.First[row]) * stride;
                        const std::uint8_t* lower = component.Samples.data() + static_cast<std::size_t>(down.Second[row]) * stride;
                        float blendY = down.Blend[row];
                        for (std::size_t x = 0; x < width; ++x)
                        {
                            std::size_t first = across.First.empty() ? x : static_cast<std::size_t>(across.First[x]);
                            std::size_t second = across.First.empty() ? x : static_cast<std::size_t>(across.Second[x]);
                            float blendX = across.First.empty() ? 0.0f : across.Blend[x];
                            float top = upper[first] + (static_cast<float>(upper[second]) - upper[first]) * blendX;
                            float bottom = lower[first] + (static_cast<float>(lower[second]) - lower[first]) * blendX;
                            values[x] = top + (bottom - top) * blendY;
                        }
                    }

                    std::uint8_t* pixel = image.Pixels.data() + static_cast<std::size_t>(y) * image.Stride();
                    for (std::size_t x = 0; x < width; ++x, pixel += 4)
                    {
                        if (count == 1)
                        {
                            pixel[0] = pixel[1] = pixel[2] = toByte(line[0][x]);
                        }
                        else if (convert)
                        {
                            float luma = line[0][x];
                            float blue = line[1][x] - 128.0f;
                            float red = line[2][x] - 128.0f;
                            pixel[0] = toByte(luma + 1.402f * red);
                            pixel[1] = toByte(luma - 0.344136f * blue - 0.714136f * red);
                            pixel[2] = toByte(luma + 1.772f * blue);
                        }
                        else
                        {
                            pixel[0] = toByte(line[0][x]);
                            pixel[1] = toByte(line[1][x]);
                            pixel[2] = toByte(line[2][x]);
                        }
                        pixel[3] = 255;
                    }
                }
                return image;
            }
        };

        // Moves to the next marker and returns its code, or -1 at the end of the data.
        int NextMarker(std::span<const std::uint8_t> bytes, std::size_t& position)
        {
            while (position + 1 < bytes.size())
            {
                if (bytes[position] == 0xFF && bytes[position + 1] != 0x00 && bytes[position + 1] != 0xFF)
                {
                    int marker = bytes[position + 1];
                    position += 2;
                    return marker;
                }
                ++position;
            }
            return -1;
        }
    }

    Result<ImageData> DecodeJpeg(std::span<const std::uint8_t> bytes)
    {
        Decoder decoder;
        std::size_t position = 2;
        bool scanned = false;

        for (;;)
        {
            int marker = NextMarker(bytes, position);
            if (marker < 0 || marker == 0xD9)
            {
                break;
            }
            if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            {
                continue;
            }

            ByteReader reader(bytes.subspan(position));
            std::size_t length = reader.U16Big();
            if (reader.Failed() || length < 2 || !reader.Has(length - 2))
            {
                return Failure("it is cut short in the middle of a marker segment");
            }
            ByteReader segment(bytes.subspan(position + 2, length - 2));
            position += length;

            switch (marker)
            {
            case 0xDB:
                while (!segment.AtEnd())
                {
                    int information = segment.U8();
                    int precision = information >> 4;
                    int table = information & 15;
                    if (table > 3 || precision > 1)
                    {
                        return Failure("it has a quantization table it cannot use");
                    }
                    for (auto& value : decoder.Quantization[static_cast<std::size_t>(table)])
                    {
                        value = precision == 0 ? segment.U8() : segment.U16Big();
                    }
                    decoder.QuantizationDefined[static_cast<std::size_t>(table)] = true;
                }
                break;

            case 0xC4:
                while (!segment.AtEnd())
                {
                    int information = segment.U8();
                    int tableClass = information >> 4;
                    int table = information & 15;
                    if (tableClass > 1 || table > 3)
                    {
                        return Failure("it has a Huffman table it cannot use");
                    }
                    std::array<std::uint8_t, 17> counts {};
                    int total = 0;
                    for (int codeLength = 1; codeLength <= 16; ++codeLength)
                    {
                        counts[static_cast<std::size_t>(codeLength)] = segment.U8();
                        total += counts[static_cast<std::size_t>(codeLength)];
                    }
                    if (total > 256)
                    {
                        return Failure("a Huffman table has more than 256 codes");
                    }
                    std::span<const std::uint8_t> values = segment.Take(static_cast<std::size_t>(total));
                    HuffmanTable& target = tableClass == 0 ? decoder.DcTables[static_cast<std::size_t>(table)]
                                                           : decoder.AcTables[static_cast<std::size_t>(table)];
                    if (segment.Failed() || !target.Build(counts, values))
                    {
                        return Failure("a Huffman table is damaged");
                    }
                }
                break;

            case 0xC0:
            case 0xC1:
            {
                if (decoder.HaveFrame)
                {
                    return Failure("it has more than one frame");
                }
                int precision = segment.U8();
                decoder.Height = segment.U16Big();
                decoder.Width = segment.U16Big();
                int count = segment.U8();
                if (precision != 8)
                {
                    return Failure(std::format("it uses {}-bit samples; only 8-bit JPEG is supported", precision));
                }
                if (decoder.Height == 0)
                {
                    return Failure("it gives its height at the end of the image (DNL), which is not supported");
                }
                Result<> size = CheckImageSize(decoder.Width, decoder.Height);
                if (!size)
                {
                    return Failure(size.Error());
                }
                if (count != 1 && count != 3)
                {
                    return Failure(std::format("it has {} color components; only grayscale and color JPEG are supported",
                        count));
                }
                for (int index = 0; index < count; ++index)
                {
                    Component component;
                    component.Identifier = segment.U8();
                    int sampling = segment.U8();
                    component.Horizontal = sampling >> 4;
                    component.Vertical = sampling & 15;
                    component.QuantizationTable = segment.U8();
                    if (component.Horizontal < 1 || component.Horizontal > 4 || component.Vertical < 1 ||
                        component.Vertical > 4 || component.QuantizationTable > 3)
                    {
                        return Failure("a component has invalid sampling factors or tables");
                    }
                    decoder.MaximumHorizontal = Max(decoder.MaximumHorizontal, component.Horizontal);
                    decoder.MaximumVertical = Max(decoder.MaximumVertical, component.Vertical);
                    decoder.Components.push_back(component);
                }
                if (segment.Failed())
                {
                    return Failure("its frame header is too short");
                }

                int unitsWide = (decoder.Width + 8 * decoder.MaximumHorizontal - 1) / (8 * decoder.MaximumHorizontal);
                int unitsHigh = (decoder.Height + 8 * decoder.MaximumVertical - 1) / (8 * decoder.MaximumVertical);

                // Every block takes at least two bits, so the data bounds how many there can be.
                std::uint64_t blocks = 0;
                for (const Component& component : decoder.Components)
                {
                    blocks += static_cast<std::uint64_t>(unitsWide) * static_cast<std::uint64_t>(unitsHigh) *
                              static_cast<std::uint64_t>(component.Horizontal * component.Vertical);
                }
                if (blocks > static_cast<std::uint64_t>(bytes.size() - position) * 4)
                {
                    return Failure("its image data is cut short");
                }
                for (Component& component : decoder.Components)
                {
                    if (decoder.MaximumHorizontal % component.Horizontal != 0 ||
                        decoder.MaximumVertical % component.Vertical != 0)
                    {
                        return Failure("its chroma subsampling is not a whole-number ratio, which is not supported");
                    }
                    component.BlocksWide = unitsWide * component.Horizontal;
                    component.BlocksHigh = unitsHigh * component.Vertical;
                    component.Samples.assign(static_cast<std::size_t>(component.BlocksWide) * 64 *
                                                 static_cast<std::size_t>(component.BlocksHigh), 0);
                }
                decoder.HaveFrame = true;
                break;
            }

            case 0xC2:
            case 0xC6:
            case 0xCA:
            case 0xCE:
                return Failure("it is a progressive JPEG, which easyforge does not read yet");

            case 0xC3:
            case 0xC5:
            case 0xC7:
            case 0xC9:
            case 0xCB:
            case 0xCD:
            case 0xCF:
                return Failure("it uses lossless, hierarchical, or arithmetic-coded JPEG, which is not supported");

            case 0xDD:
                decoder.RestartInterval = segment.U16Big();
                break;

            case 0xEE:
            {
                // Adobe's APP14 segment: "Adobe", version, two flag words, then the color transform.
                if (segment.Matches("Adobe") && segment.Has(7))
                {
                    segment.Skip(6);
                    decoder.AdobeTransform = segment.U8();
                }
                break;
            }

            case 0xDA:
            {
                if (!decoder.HaveFrame)
                {
                    return Failure("its image data comes before its frame header");
                }
                int count = segment.U8();
                std::vector<int> scan;
                for (int index = 0; index < count; ++index)
                {
                    int identifier = segment.U8();
                    int tables = segment.U8();
                    int found = -1;
                    for (std::size_t component = 0; component < decoder.Components.size(); ++component)
                    {
                        if (decoder.Components[component].Identifier == identifier)
                        {
                            found = static_cast<int>(component);
                        }
                    }
                    if (found < 0 || (tables >> 4) > 3 || (tables & 15) > 3)
                    {
                        return Failure("a scan names a component or table that does not exist");
                    }
                    decoder.Components[static_cast<std::size_t>(found)].DcTable = tables >> 4;
                    decoder.Components[static_cast<std::size_t>(found)].AcTable = tables & 15;
                    scan.push_back(found);
                }
                if (segment.Failed() || scan.empty())
                {
                    return Failure("a scan header is damaged");
                }
                Result<> decoded = decoder.DecodeScan(bytes, position, std::move(scan));
                if (!decoded)
                {
                    return Failure(decoded.Error());
                }
                scanned = true;
                break;
            }

            default:
                // Application data, comments, and other segments do not change the pixels.
                break;
            }
        }

        if (!decoder.HaveFrame || !scanned)
        {
            return Failure("it has no image data");
        }
        return decoder.Finish();
    }
}
