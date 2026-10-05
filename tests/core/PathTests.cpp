#include <easyforge/core/Paths.h>
#include <easyforge/core/Testing.h>

#include <string>

using namespace easyforge;

EASYFORGE_TEST(PathsKeepTextInAnyLanguage)
{
    for (const char* text : { "saves/Café.tree", "脚本/main.script", "κείμενο/😀 smile.png", "plain/ascii.txt", "" })
    {
        EASYFORGE_EXPECT_EQUAL(TextFromPath(PathFromText(text)), std::string(text));
    }
    // Parts are joined with forward slashes on every platform.
    EASYFORGE_EXPECT_EQUAL(TextFromPath(PathFromText("folder") / PathFromText("file.txt")), std::string("folder/file.txt"));
}

EASYFORGE_TEST(PathsNeverThrowOnBrokenText)
{
    // Bytes that are not UTF-8 become the replacement character instead of
    // making the standard library throw.
    std::string broken = "a\xFF" "b\xC0\xAF" "c\xED\xA0\x80" "d\xF4\x90\x80\x80" "e\xE2\x82";
    std::string text = TextFromPath(PathFromText(broken));
    EASYFORGE_EXPECT(text.starts_with("a\xEF\xBF\xBD" "b"));
    EASYFORGE_EXPECT(text.find('e') != std::string::npos);
    for (char character : text)
    {
        EASYFORGE_EXPECT(character != '\xFF');
    }
}
