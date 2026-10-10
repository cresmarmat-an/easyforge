#include <easyforge/core/Paths.h>
#include <easyforge/core/Testing.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace easyforge;

namespace
{
    std::string Output(std::string_view name)
    {
        return std::string(EASYFORGE_TEST_OUTPUT) + std::string(name);
    }

    std::string ReadAll(std::string_view path)
    {
        std::ifstream file(PathFromText(path), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
}

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

EASYFORGE_TEST(WholeFilesTakeTheOldOnesPlace)
{
    std::string path = Output("whole café.txt");
    EASYFORGE_REQUIRE(WriteWholeFile(path, "first"));
    EASYFORGE_EXPECT_EQUAL(ReadAll(path), std::string("first"));
    EASYFORGE_REQUIRE(WriteWholeFile(path, std::string(100'000, 'x')));
    EASYFORGE_EXPECT_EQUAL(ReadAll(path), std::string(100'000, 'x'));
    EASYFORGE_REQUIRE(WriteWholeFile(path, ""));
    EASYFORGE_EXPECT_EQUAL(ReadAll(path), std::string());
    EASYFORGE_EXPECT(!std::filesystem::exists(internal::WritingPath(PathFromText(path))));
}

EASYFORGE_TEST(AFileThatCannotBeWrittenKeepsTheOldOne)
{
    Result<> missing = WriteWholeFile(Output("no such folder/file.txt"), "text");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(!missing.Error().empty());

#ifdef _WIN32
    // On Windows a read-only file cannot be replaced.
    std::string path = Output("read-only.txt");
    std::filesystem::path location = PathFromText(path);
    std::error_code ignored;
    std::filesystem::permissions(location, std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ignored);
    EASYFORGE_REQUIRE(WriteWholeFile(path, "kept"));
    std::filesystem::permissions(location, std::filesystem::perms::all, std::filesystem::perm_options::remove);
    Result<> refused = WriteWholeFile(path, "lost");
    std::filesystem::permissions(location, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
    EASYFORGE_EXPECT(!refused);
    EASYFORGE_EXPECT_EQUAL(ReadAll(path), std::string("kept"));
    EASYFORGE_EXPECT(!std::filesystem::exists(internal::WritingPath(location)));
#endif
}
