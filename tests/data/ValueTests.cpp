#include <easyforge/core/Testing.h>
#include <easyforge/data.h>

using namespace easyforge;

EASYFORGE_TEST(ValuesKeepTheirType)
{
    EASYFORGE_EXPECT(DataValue().Type() == DataType::Nothing);
    EASYFORGE_EXPECT(DataValue(true).Type() == DataType::Boolean);
    EASYFORGE_EXPECT(DataValue(100).Type() == DataType::Integer);
    EASYFORGE_EXPECT(DataValue(std::uint8_t { 7 }).Type() == DataType::Integer);
    EASYFORGE_EXPECT(DataValue(4.5f).Type() == DataType::Number);
    EASYFORGE_EXPECT(DataValue("Ari").Type() == DataType::Text);
    EASYFORGE_EXPECT(DataValue(Vector2 { 1, 2 }).Type() == DataType::Vector2);
    EASYFORGE_EXPECT(DataValue(Vector3 { 1, 2, 3 }).Type() == DataType::Vector3);
    EASYFORGE_EXPECT(DataValue(Vector4 { 1, 2, 3, 4 }).Type() == DataType::Vector4);
    EASYFORGE_EXPECT(DataValue(Color::Hex("#FF8000")).Type() == DataType::Color);
    EASYFORGE_EXPECT_EQUAL(DataTypeName(DataType::Vector3), std::string_view("vector3"));
}

EASYFORGE_TEST(ValuesReadAsOtherTypes)
{
    // Whole numbers and numbers read as each other, rounding to the nearest.
    EASYFORGE_EXPECT_EQUAL(DataValue(100).As<float>(), 100.0f);
    EASYFORGE_EXPECT_EQUAL(DataValue(2.6).As<int>(), 3);
    EASYFORGE_EXPECT_EQUAL(DataValue(-2.6).As<int>(), -3);

    // Anything else read as the wrong type is that type's empty value.
    EASYFORGE_EXPECT_EQUAL(DataValue("100").As<int>(), 0);
    EASYFORGE_EXPECT_EQUAL(DataValue(100).As<std::string>(), std::string());
    EASYFORGE_EXPECT_EQUAL(DataValue(1).As<bool>(), false);
    EASYFORGE_EXPECT_EQUAL(DataValue(Vector3 { 1, 2, 3 }).As<Vector2>(), Vector2());
    EASYFORGE_EXPECT_EQUAL(DataValue().As<std::string>(), std::string());
}

EASYFORGE_TEST(ValuesWriteAndReadAsText)
{
    auto roundTrip = [](const DataValue& value) {
        bool valid = false;
        DataValue read = DataValue::FromText(value.ToText(), &valid);
        return valid && read == value;
    };

    EASYFORGE_EXPECT_EQUAL(DataValue(100).ToText(), std::string("100"));
    EASYFORGE_EXPECT_EQUAL(DataValue(-7).ToText(), std::string("-7"));
    EASYFORGE_EXPECT_EQUAL(DataValue(4.5).ToText(), std::string("4.5"));
    EASYFORGE_EXPECT_EQUAL(DataValue(3.0).ToText(), std::string("3.0"));
    EASYFORGE_EXPECT_EQUAL(DataValue(true).ToText(), std::string("true"));
    EASYFORGE_EXPECT_EQUAL(DataValue("Ari").ToText(), std::string("\"Ari\""));
    EASYFORGE_EXPECT_EQUAL(DataValue(Vector2 { 10, 20.5f }).ToText(), std::string("10, 20.5"));
    EASYFORGE_EXPECT_EQUAL(DataValue(Color::Hex("#FF8000")).ToText(), std::string("#FF8000"));
    EASYFORGE_EXPECT_EQUAL(DataValue().ToText(), std::string("nothing"));

    // A number with nothing after the point stays a number, not a whole number.
    EASYFORGE_EXPECT(DataValue::FromText("3.0").Type() == DataType::Number);
    EASYFORGE_EXPECT(DataValue::FromText("3").Type() == DataType::Integer);

    EASYFORGE_EXPECT(roundTrip(DataValue(0.1)));
    EASYFORGE_EXPECT(roundTrip(DataValue(1.0e20)));
    EASYFORGE_EXPECT(roundTrip(DataValue(-123456789012345)));
    EASYFORGE_EXPECT(roundTrip(DataValue(false)));
    EASYFORGE_EXPECT(roundTrip(DataValue("a \"quote\", a \\ and\na new line\tand a tab")));
    EASYFORGE_EXPECT(roundTrip(DataValue("")));
    EASYFORGE_EXPECT(roundTrip(DataValue(Vector3 { -1.25f, 0, 1e-3f })));
    EASYFORGE_EXPECT(roundTrip(DataValue(Vector4 { 0.1f, 0.2f, 0.3f, 0.4f })));
    EASYFORGE_EXPECT(roundTrip(DataValue(Color::Hex("#15151A80"))));
    EASYFORGE_EXPECT(roundTrip(DataValue()));
}

EASYFORGE_TEST(TextThatIsNotAValueIsRefused)
{
    for (std::string_view text : { "", "Ari", "\"unfinished", "1, 2, 3, 4, 5", "1,", "#GG0000", "4.5.6", "truth" })
    {
        bool valid = true;
        DataValue read = DataValue::FromText(text, &valid);
        EASYFORGE_EXPECT(!valid);
        EASYFORGE_EXPECT(read.IsNothing());
    }
}

EASYFORGE_TEST(ValuesFormatForPeople)
{
    // Text is printed without quotes, so it can go straight into a sentence.
    EASYFORGE_EXPECT_EQUAL(std::format("Name: {}", DataValue("Ari")), std::string("Name: Ari"));
    EASYFORGE_EXPECT_EQUAL(std::format("Health: {}", DataValue(90)), std::string("Health: 90"));
    EASYFORGE_EXPECT_EQUAL(std::format("[{:>5}]", DataValue(7)), std::string("[    7]"));
}
