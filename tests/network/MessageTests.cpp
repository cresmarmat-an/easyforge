#include <cstdint>
#include <string>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/network.h>

using namespace easyforge;

EASYFORGE_TEST(MessagesHoldEveryKindOfValue)
{
    Message message {
        { "Nothing", MessageValue() },
        { "Ready", true },
        { "Score", 1250 },
        { "Speed", 4.5f },
        { "Name", "Ari" },
        { "Data", std::vector<std::uint8_t> { 1, 2, 3, 250 } },
        { "Position", Vector2 { 10.0f, 20.0f } },
        { "Spot", Vector3 { 1.0f, 2.0f, 3.0f } },
        { "Corner", Vector4 { 1.0f, 2.0f, 3.0f, 4.0f } },
        { "Tint", Color::Hex("#66CCFF") },
    };
    EASYFORGE_EXPECT_EQUAL(message.Size(), std::size_t { 10 });
    EASYFORGE_EXPECT(message["Nothing"].IsNothing());
    EASYFORGE_EXPECT(message["Ready"].Kind() == MessageValueKind::Boolean);

    bool ready = message["Ready"];
    int score = message["Score"];
    float speed = message["Speed"];
    std::string name = message["Name"];
    std::vector<std::uint8_t> data = message["Data"];
    Vector2 position = message["Position"];
    Color tint = message["Tint"];
    EASYFORGE_EXPECT(ready);
    EASYFORGE_EXPECT_EQUAL(score, 1250);
    EASYFORGE_EXPECT_EQUAL(speed, 4.5f);
    EASYFORGE_EXPECT_EQUAL(name, std::string("Ari"));
    EASYFORGE_EXPECT(data == (std::vector<std::uint8_t> { 1, 2, 3, 250 }));
    EASYFORGE_EXPECT(position == (Vector2 { 10.0f, 20.0f }));
    EASYFORGE_EXPECT(tint == Color::Hex("#66CCFF"));
    EASYFORGE_EXPECT(message["Spot"].AsVector3() == (Vector3 { 1.0f, 2.0f, 3.0f }));
    EASYFORGE_EXPECT(message["Corner"].As<Vector4>() == (Vector4 { 1.0f, 2.0f, 3.0f, 4.0f }));

    std::optional<Message> decoded = Message::Decode(message.Encode());
    EASYFORGE_REQUIRE(decoded.has_value());
    EASYFORGE_EXPECT(*decoded == message);
    EASYFORGE_EXPECT(decoded->Names() == message.Names());
}

EASYFORGE_TEST(MessageValuesReadAsTheirTypes)
{
    const Message message { { "Whole", 7 }, { "Number", 2.75 }, { "Text", "7" } };

    // Whole numbers and numbers read as each other.
    EASYFORGE_EXPECT_EQUAL(message["Whole"].AsNumber(), 7.0);
    EASYFORGE_EXPECT_EQUAL(message["Number"].As<int>(), 2);

    // Anything else reads as the empty value.
    EASYFORGE_EXPECT_EQUAL(message["Text"].AsInteger(), std::int64_t { 0 });
    EASYFORGE_EXPECT_EQUAL(message["Whole"].AsText(), std::string());
    EASYFORGE_EXPECT(message["Missing"].IsNothing());
    EASYFORGE_EXPECT_EQUAL(message["Missing"].As<int>(), 0);

    // Reading a const message, or with Get, never adds a name; reading one
    // that can change does, as std::map does.
    EASYFORGE_EXPECT(!message.Has("Missing"));
    EASYFORGE_EXPECT_EQUAL(message.Size(), std::size_t { 3 });
    Message changing = message;
    EASYFORGE_EXPECT(changing.Get("Other").IsNothing());
    EASYFORGE_EXPECT_EQUAL(changing.Size(), std::size_t { 3 });
    EASYFORGE_EXPECT(changing["Other"].IsNothing());
    EASYFORGE_EXPECT_EQUAL(changing.Size(), std::size_t { 4 });
}

EASYFORGE_TEST(MessagesKeepNamesInOrder)
{
    Message message { { "First", 1 }, { "Second", 2 }, { "First", 3 } };
    EASYFORGE_EXPECT_EQUAL(message.Size(), std::size_t { 2 });
    EASYFORGE_EXPECT_EQUAL(message["First"].As<int>(), 3);

    message["Third"] = "added";
    message.Set("Second", 20);
    EASYFORGE_EXPECT((message.Names() == std::vector<std::string> { "First", "Second", "Third" }));
    EASYFORGE_EXPECT_EQUAL(message["Second"].As<int>(), 20);

    EASYFORGE_EXPECT(message.Remove("First"));
    EASYFORGE_EXPECT(!message.Remove("First"));
    EASYFORGE_EXPECT((message.Names() == std::vector<std::string> { "Second", "Third" }));

    Message empty;
    EASYFORGE_EXPECT(empty.IsEmpty());
    std::optional<Message> decoded = Message::Decode(empty.Encode());
    EASYFORGE_REQUIRE(decoded.has_value());
    EASYFORGE_EXPECT(decoded->IsEmpty());
}

EASYFORGE_TEST(DamagedBytesAreNotMessages)
{
    Message message { { "Name", "Ari" }, { "Score", 10 } };
    std::vector<std::uint8_t> bytes = message.Encode();

    for (std::size_t size = 0; size < bytes.size(); ++size)
    {
        std::vector<std::uint8_t> cut(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
        EASYFORGE_EXPECT(!Message::Decode(cut).has_value());
    }

    std::vector<std::uint8_t> longer = bytes;
    longer.push_back(0);
    EASYFORGE_EXPECT(!Message::Decode(longer).has_value());

    // The kind of the first value, after the count and the name "Name".
    std::vector<std::uint8_t> unknownKind = bytes;
    unknownKind[2 + 1 + 4] = 200;
    EASYFORGE_EXPECT(!Message::Decode(unknownKind).has_value());
}
