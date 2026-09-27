#include <easyforge/core/Testing.h>

#include <filesystem>
#include <fstream>

#include "TestData.h"

using namespace easyforge;

namespace
{
    void WriteText(const std::string& path, const std::string& text)
    {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream stream(std::filesystem::path(path), std::ios::binary);
        stream << text;
    }

    std::string AsText(const std::vector<std::uint8_t>& bytes)
    {
        return std::string(bytes.begin(), bytes.end());
    }
}

EASYFORGE_TEST(FilesReadAbsolutePaths)
{
    Result<std::vector<std::uint8_t>> bytes = Files::Read(testdata::Path("models/cube.obj"));
    EASYFORGE_REQUIRE(bytes);
    EASYFORGE_EXPECT(AsText(*bytes).starts_with("# A unit cube"));
    EASYFORGE_EXPECT(Files::Exists(testdata::Path("models/cube.obj")));
    EASYFORGE_EXPECT(!Files::Exists(testdata::Path("models/missing.obj")));

    Result<std::vector<std::uint8_t>> missing = Files::Read(testdata::Path("models/missing.obj"));
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(missing.Error().find("was not found") != std::string::npos);
}

EASYFORGE_TEST(FilesSearchMountedFolders)
{
    std::string first = testdata::OutputFolder("mount_first");
    std::string second = testdata::OutputFolder("mount_second");
    WriteText(first + "shared.txt", "first");
    WriteText(second + "shared.txt", "second");
    WriteText(first + "sub/only.txt", "only in first");

    Files::UnmountAll();
    EASYFORGE_REQUIRE(Files::Mount(first));
    EASYFORGE_REQUIRE(Files::Mount(second));

    // The most recent mount wins.
    EASYFORGE_EXPECT_EQUAL(AsText(Files::Read("shared.txt").GetOr({})), std::string("second"));
    EASYFORGE_EXPECT_EQUAL(AsText(Files::Read("sub/only.txt").GetOr({})), std::string("only in first"));
    EASYFORGE_EXPECT_EQUAL(AsText(Files::Read("./sub/../sub/only.txt").GetOr({})), std::string("only in first"));

    Files::Unmount(second);
    EASYFORGE_EXPECT_EQUAL(AsText(Files::Read("shared.txt").GetOr({})), std::string("first"));

    Files::UnmountAll();
    EASYFORGE_EXPECT(!Files::Exists("sub/only.txt"));
    EASYFORGE_EXPECT(!Files::Mount(first + "nowhere"));
}

EASYFORGE_TEST(FilesPacks)
{
    std::string folder = testdata::OutputFolder("pack_source");
    WriteText(folder + "readme.txt", "hello from a pack");
    WriteText(folder + "textures/wood.txt", std::string(100000, 'w'));
    std::string pack = testdata::OutputFolder("pack_output") + "game.pack";
    EASYFORGE_REQUIRE(Files::CreatePack(folder, pack));

    Files::UnmountAll();
    EASYFORGE_REQUIRE(Files::Mount(pack));
    EASYFORGE_EXPECT_EQUAL(AsText(Files::Read("readme.txt").GetOr({})), std::string("hello from a pack"));
    EASYFORGE_EXPECT_EQUAL(Files::Read("textures/wood.txt").GetOr({}).size(), std::size_t { 100000 });

    // Reading part of a file inside a pack stays inside that file.
    FileReader reader = Files::Open("readme.txt");
    EASYFORGE_REQUIRE(reader);
    EASYFORGE_EXPECT_EQUAL(reader.Size(), std::uint64_t { 17 });
    std::vector<std::uint8_t> part(100);
    std::size_t count = reader.ReadAt(11, part);
    part.resize(count);
    EASYFORGE_EXPECT_EQUAL(AsText(part), std::string("a pack"));
    EASYFORGE_EXPECT_EQUAL(reader.ReadAt(17, part), std::size_t { 0 });

    Files::UnmountAll();

    // A file that is not a pack is refused with a reason.
    Result<> notPack = Files::Mount(testdata::Path("models/cube.obj"));
    EASYFORGE_EXPECT(!notPack);
    EASYFORGE_EXPECT(notPack.Error().find("neither a folder nor") != std::string::npos);
}

EASYFORGE_TEST(FilesPathHelpers)
{
    EASYFORGE_EXPECT_EQUAL(Files::FolderOf("models/ship.obj"), std::string("models/"));
    EASYFORGE_EXPECT_EQUAL(Files::FolderOf("ship.obj"), std::string(""));
    EASYFORGE_EXPECT_EQUAL(Files::FolderOf("C:\\art\\ship.obj"), std::string("C:\\art\\"));
    EASYFORGE_EXPECT_EQUAL(Files::ExtensionOf("Ship.OBJ"), std::string(".obj"));
    EASYFORGE_EXPECT_EQUAL(Files::ExtensionOf("folder.with.dots/file"), std::string(""));
    EASYFORGE_EXPECT_EQUAL(Files::ExtensionOf("archive.tar.gz"), std::string(".gz"));
    EASYFORGE_EXPECT(Files::ProgramFolder().ends_with("/"));
}
