#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(ColorFromHex)
{
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#FFFFFF"), Color::White);
    EASYFORGE_EXPECT_EQUAL(Color::Hex("000"), Color::Black);
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#15151A"), Color::FromBytes(0x15, 0x15, 0x1A));
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#f0a"), Color::FromBytes(0xFF, 0x00, 0xAA));
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#11223344"), Color::FromBytes(0x11, 0x22, 0x33, 0x44));
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#1234"), Color::FromBytes(0x11, 0x22, 0x33, 0x44));

    static_assert(Color::Hex("#FF0000").Red == 1.0f, "Hex works at compile time");
}

EASYFORGE_TEST(ColorInvalidHexIsMagenta)
{
    Color magenta = Color::Hex("#FF00FF");
    EASYFORGE_EXPECT(!Color::IsHex("#12345"));
    EASYFORGE_EXPECT(!Color::IsHex("#GGGGGG"));
    EASYFORGE_EXPECT(!Color::IsHex(""));
    EASYFORGE_EXPECT(Color::IsHex("abc"));
    EASYFORGE_EXPECT_EQUAL(Color::Hex("not a color"), magenta);
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#12345"), magenta);
}

EASYFORGE_TEST(ColorToHex)
{
    EASYFORGE_EXPECT_EQUAL(Color::Hex("#15151A").ToHex(), std::string("#15151A"));
    EASYFORGE_EXPECT_EQUAL(Color::Black.WithAlpha(0.5f).ToHex(), std::string("#00000080"));
    EASYFORGE_EXPECT_EQUAL((Color { 2.0f, -1.0f, 0.5f }).ToHex(), std::string("#FF0080"));
    EASYFORGE_EXPECT_EQUAL(std::format("{}", Color::White), std::string("#FFFFFF"));
}

EASYFORGE_TEST(ColorLinearRoundTrip)
{
    Color color = Color::Hex("#3A7BD5");
    Color linear = color.ToLinear();

    EASYFORGE_EXPECT(linear.Red < color.Red);
    EASYFORGE_EXPECT_NEAR(Color::FromLinear(linear), color, 0.0001f);
    EASYFORGE_EXPECT_NEAR((Color { 0.5f, 0.5f, 0.5f }).ToLinear().Red, 0.2140f, 0.0001f);
}

EASYFORGE_TEST(ColorLerp)
{
    Color middle = Lerp(Color::Black, Color::White.WithAlpha(0.0f), 0.5f);
    EASYFORGE_EXPECT_NEAR(middle, (Color { 0.5f, 0.5f, 0.5f, 0.5f }), 0.00001f);
}
