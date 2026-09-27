#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(KeyNamesForPeople)
{
    EASYFORGE_EXPECT_EQUAL(KeyName(Key::A), std::string_view("A"));
    EASYFORGE_EXPECT_EQUAL(KeyName(Key::Digit7), std::string_view("7"));
    EASYFORGE_EXPECT_EQUAL(KeyName(Key::LeftShift), std::string_view("Left Shift"));
    EASYFORGE_EXPECT_EQUAL(KeyName(Key::PageDown), std::string_view("Page Down"));
    EASYFORGE_EXPECT_EQUAL(KeyName(Key::NumberPadEnter), std::string_view("Number Pad Enter"));
    EASYFORGE_EXPECT_EQUAL(KeyName(Key::Count), std::string_view("Unknown"));
    for (int key = 1; key < static_cast<int>(Key::Count); ++key)
    {
        std::string_view name = KeyName(static_cast<Key>(key));
        EASYFORGE_EXPECT(!name.empty() && name != "Unknown");
    }
}

EASYFORGE_TEST(KeyModifiersCompare)
{
    KeyModifiers none;
    KeyModifiers shift { .Shift = true };
    EASYFORGE_EXPECT(none == KeyModifiers {});
    EASYFORGE_EXPECT(!(none == shift));
}
