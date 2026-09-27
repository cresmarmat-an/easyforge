#include <easyforge/input/Recording.h>

#include <bit>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace easyforge
{
    namespace
    {
        // A recording file: "EFRECORD", a version, the frame count, then each
        // frame. Numbers are little-endian; floats keep their exact bits, so a
        // saved recording plays back exactly as it was made.
        constexpr char Signature[8] = { 'E', 'F', 'R', 'E', 'C', 'O', 'R', 'D' };
        constexpr std::uint32_t FormatVersion = 1;

        class Writer
        {
        public:
            void Byte(std::uint8_t value) { Bytes.push_back(value); }

            void Number(std::uint32_t value)
            {
                for (int shift = 0; shift < 32; shift += 8)
                {
                    Bytes.push_back(static_cast<std::uint8_t>(value >> shift));
                }
            }

            void Signed(std::int32_t value) { Number(static_cast<std::uint32_t>(value)); }
            void Float(float value) { Number(std::bit_cast<std::uint32_t>(value)); }

            void Pair(Vector2 value)
            {
                Float(value.X);
                Float(value.Y);
            }

            void Text(const std::string& text)
            {
                Number(static_cast<std::uint32_t>(text.size()));
                Bytes.insert(Bytes.end(), text.begin(), text.end());
            }

            std::vector<std::uint8_t> Bytes;
        };

        class Reader
        {
        public:
            explicit Reader(std::span<const std::uint8_t> bytes) : Bytes(bytes) {}

            bool Has(std::size_t count) const { return Bytes.size() - Offset >= count; }

            std::optional<std::uint8_t> Byte()
            {
                if (!Has(1))
                {
                    return std::nullopt;
                }
                return Bytes[Offset++];
            }

            std::optional<std::uint32_t> Number()
            {
                if (!Has(4))
                {
                    return std::nullopt;
                }
                std::uint32_t value = 0;
                for (int index = 0; index < 4; ++index)
                {
                    value |= static_cast<std::uint32_t>(Bytes[Offset++]) << (index * 8);
                }
                return value;
            }

            std::optional<float> Float()
            {
                std::optional<std::uint32_t> bits = Number();
                return bits ? std::optional<float>(std::bit_cast<float>(*bits)) : std::nullopt;
            }

            std::optional<Vector2> Pair()
            {
                std::optional<float> x = Float();
                std::optional<float> y = Float();
                return x && y ? std::optional<Vector2>(Vector2 { *x, *y }) : std::nullopt;
            }

            std::optional<std::string> Text()
            {
                std::optional<std::uint32_t> length = Number();
                if (!length || !Has(*length))
                {
                    return std::nullopt;
                }
                std::string text(reinterpret_cast<const char*>(Bytes.data() + Offset), *length);
                Offset += *length;
                return text;
            }

            std::span<const std::uint8_t> Bytes;
            std::size_t Offset = 0;
        };

        std::uint8_t ModifierBits(const KeyModifiers& modifiers)
        {
            return static_cast<std::uint8_t>((modifiers.Shift ? 1 : 0) | (modifiers.Control ? 2 : 0) |
                                             (modifiers.Alt ? 4 : 0) | (modifiers.Meta ? 8 : 0));
        }

        void WriteEvent(Writer& writer, const Event& event)
        {
            writer.Byte(static_cast<std::uint8_t>(event.Type));
            writer.Number(static_cast<std::uint32_t>(event.Key));
            writer.Byte(static_cast<std::uint8_t>((event.Repeat ? 1 : 0) | (event.Handled ? 2 : 0)));
            writer.Byte(ModifierBits(event.Modifiers));
            writer.Text(event.Text);
            writer.Pair(event.Position);
            writer.Pair(event.Movement);
            writer.Byte(static_cast<std::uint8_t>(event.Button));
            writer.Signed(event.ClickCount);
            writer.Pair(event.Wheel);
            writer.Signed(event.Touch);
            writer.Pair(event.Size);
            writer.Float(event.Scale);
            writer.Number(static_cast<std::uint32_t>(event.Files.size()));
            for (const std::string& file : event.Files)
            {
                writer.Text(file);
            }
        }

        std::optional<Event> ReadEvent(Reader& reader)
        {
            Event event;
            std::optional<std::uint8_t> type = reader.Byte();
            std::optional<std::uint32_t> key = reader.Number();
            std::optional<std::uint8_t> flags = reader.Byte();
            std::optional<std::uint8_t> modifiers = reader.Byte();
            std::optional<std::string> text = reader.Text();
            if (!type || !key || !flags || !modifiers || !text || *type > static_cast<std::uint8_t>(EventType::Resumed) ||
                *key >= static_cast<std::uint32_t>(Key::Count))
            {
                return std::nullopt;
            }
            event.Type = static_cast<EventType>(*type);
            event.Key = static_cast<Key>(*key);
            event.Repeat = (*flags & 1) != 0;
            event.Handled = (*flags & 2) != 0;
            event.Modifiers = { .Shift = (*modifiers & 1) != 0,
                .Control = (*modifiers & 2) != 0,
                .Alt = (*modifiers & 4) != 0,
                .Meta = (*modifiers & 8) != 0 };
            event.Text = std::move(*text);

            std::optional<Vector2> position = reader.Pair();
            std::optional<Vector2> movement = reader.Pair();
            std::optional<std::uint8_t> button = reader.Byte();
            std::optional<std::uint32_t> clicks = reader.Number();
            std::optional<Vector2> wheel = reader.Pair();
            std::optional<std::uint32_t> touch = reader.Number();
            std::optional<Vector2> size = reader.Pair();
            std::optional<float> scale = reader.Float();
            std::optional<std::uint32_t> fileCount = reader.Number();
            if (!position || !movement || !button || !clicks || !wheel || !touch || !size || !scale || !fileCount ||
                *button >= static_cast<std::uint8_t>(MouseButton::Count) || !reader.Has(static_cast<std::size_t>(*fileCount) * 4))
            {
                return std::nullopt;
            }
            event.Position = *position;
            event.Movement = *movement;
            event.Button = static_cast<MouseButton>(*button);
            event.ClickCount = static_cast<int>(*clicks);
            event.Wheel = *wheel;
            event.Touch = static_cast<int>(*touch);
            event.Size = *size;
            event.Scale = *scale;
            for (std::uint32_t index = 0; index < *fileCount; ++index)
            {
                std::optional<std::string> file = reader.Text();
                if (!file)
                {
                    return std::nullopt;
                }
                event.Files.push_back(std::move(*file));
            }
            return event;
        }

        void WriteGamepad(Writer& writer, const GamepadState& gamepad)
        {
            writer.Byte(gamepad.Connected ? 1 : 0);
            std::uint32_t buttons = 0;
            for (std::size_t index = 0; index < gamepad.Buttons.size(); ++index)
            {
                buttons |= gamepad.Buttons[index] ? (1u << index) : 0u;
            }
            writer.Number(buttons);
            writer.Pair(gamepad.LeftStick);
            writer.Pair(gamepad.RightStick);
            writer.Float(gamepad.LeftTrigger);
            writer.Float(gamepad.RightTrigger);
        }

        std::optional<GamepadState> ReadGamepad(Reader& reader)
        {
            std::optional<std::uint8_t> connected = reader.Byte();
            std::optional<std::uint32_t> buttons = reader.Number();
            std::optional<Vector2> left = reader.Pair();
            std::optional<Vector2> right = reader.Pair();
            std::optional<float> leftTrigger = reader.Float();
            std::optional<float> rightTrigger = reader.Float();
            if (!connected || !buttons || !left || !right || !leftTrigger || !rightTrigger)
            {
                return std::nullopt;
            }
            GamepadState gamepad;
            gamepad.Connected = *connected != 0;
            for (std::size_t index = 0; index < gamepad.Buttons.size(); ++index)
            {
                gamepad.Buttons[index] = (*buttons & (1u << index)) != 0;
            }
            gamepad.LeftStick = *left;
            gamepad.RightStick = *right;
            gamepad.LeftTrigger = *leftTrigger;
            gamepad.RightTrigger = *rightTrigger;
            return gamepad;
        }
    }

    float Recording::Duration() const
    {
        float total = 0.0f;
        for (const RecordedFrame& frame : Frames)
        {
            total += frame.DeltaSeconds;
        }
        return total;
    }

    std::vector<std::uint8_t> Recording::Encode() const
    {
        Writer writer;
        writer.Bytes.insert(writer.Bytes.end(), std::begin(Signature), std::end(Signature));
        writer.Number(FormatVersion);
        writer.Number(static_cast<std::uint32_t>(Frames.size()));
        for (const RecordedFrame& frame : Frames)
        {
            writer.Float(frame.DeltaSeconds);
            writer.Number(static_cast<std::uint32_t>(frame.Events.size()));
            for (const Event& event : frame.Events)
            {
                WriteEvent(writer, event);
            }
            writer.Byte(static_cast<std::uint8_t>(frame.Gamepads.size()));
            for (const GamepadState& gamepad : frame.Gamepads)
            {
                WriteGamepad(writer, gamepad);
            }
        }
        return std::move(writer.Bytes);
    }

    Result<Recording> Recording::Decode(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() < sizeof(Signature) || std::memcmp(bytes.data(), Signature, sizeof(Signature)) != 0)
        {
            return Failure("it is not an easyforge recording");
        }
        Reader reader(bytes.subspan(sizeof(Signature)));
        std::optional<std::uint32_t> version = reader.Number();
        std::optional<std::uint32_t> frameCount = reader.Number();
        if (!version || *version != FormatVersion)
        {
            return Failure(std::format("it is a recording of version {}, and this easyforge reads version {}",
                version.value_or(0), FormatVersion));
        }
        // Each frame takes at least nine bytes, so a damaged count cannot ask for
        // more frames than the data could hold.
        if (!frameCount || !reader.Has(static_cast<std::size_t>(*frameCount) * 9))
        {
            return Failure("the recording is damaged: it ends before its frames");
        }

        Recording recording;
        recording.Frames.reserve(*frameCount);
        for (std::uint32_t frameIndex = 0; frameIndex < *frameCount; ++frameIndex)
        {
            RecordedFrame frame;
            std::optional<float> delta = reader.Float();
            std::optional<std::uint32_t> eventCount = reader.Number();
            if (!delta || !eventCount)
            {
                return Failure(std::format("the recording is damaged in frame {}", frameIndex));
            }
            frame.DeltaSeconds = *delta;
            for (std::uint32_t eventIndex = 0; eventIndex < *eventCount; ++eventIndex)
            {
                std::optional<Event> event = ReadEvent(reader);
                if (!event)
                {
                    return Failure(std::format("the recording is damaged in frame {}, event {}", frameIndex, eventIndex));
                }
                frame.Events.push_back(std::move(*event));
            }
            std::optional<std::uint8_t> gamepadCount = reader.Byte();
            if (!gamepadCount || *gamepadCount > MaximumGamepads)
            {
                return Failure(std::format("the recording is damaged in frame {}", frameIndex));
            }
            for (std::uint8_t gamepadIndex = 0; gamepadIndex < *gamepadCount; ++gamepadIndex)
            {
                std::optional<GamepadState> gamepad = ReadGamepad(reader);
                if (!gamepad)
                {
                    return Failure(std::format("the recording is damaged in frame {}", frameIndex));
                }
                frame.Gamepads[gamepadIndex] = *gamepad;
            }
            recording.Frames.push_back(std::move(frame));
        }
        return recording;
    }

    Result<> Recording::Save(std::string_view path) const
    {
        std::vector<std::uint8_t> bytes = Encode();
        std::ofstream file(std::string(path), std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file)
        {
            return Failure(std::format("{}: the recording could not be written", path));
        }
        return {};
    }

    Result<Recording> Recording::Load(std::string_view path)
    {
        std::ifstream file(std::string(path), std::ios::binary);
        if (!file)
        {
            return Failure(std::format("{}: the file could not be opened", path));
        }
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Result<Recording> recording = Decode(bytes);
        if (!recording)
        {
            return Failure(std::format("{}: {}", path, recording.Error()));
        }
        return recording;
    }
}
