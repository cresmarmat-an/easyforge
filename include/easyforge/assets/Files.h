#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Result.h>

namespace easyforge
{
    namespace internal
    {
        class FileSource;
    }

    // An open file whose parts can be read in any order, for data too large to
    // read at once, such as music. Made by Files::Open. Copies share the file.
    class FileReader
    {
    public:
        FileReader() = default;

        explicit operator bool() const { return Source != nullptr; }
        const std::string& Error() const { return ErrorText; }

        // The file's size in bytes.
        std::uint64_t Size() const;

        // Reads up to `output.size()` bytes starting at `offset` and returns how many were read.
        std::size_t ReadAt(std::uint64_t offset, std::span<std::uint8_t> output) const;

    private:
        friend class Files;

        std::shared_ptr<internal::FileSource> Source;
        std::string ErrorText;
    };

    // Where files are read from. Paths use forward slashes on every platform and
    // are UTF-8.
    //
    // A relative path is searched for in this order: the folders and packs given
    // to Mount, most recent first; then the working directory; then the folder the
    // program is in. An absolute path is read as it is.
    //
    // The list of mounted places belongs to the whole program. It is safe to read
    // files from several threads at once, including while mounting.
    class Files
    {
    public:
        // Adds a folder, or a pack made by CreatePack, to search first.
        static Result<> Mount(std::string_view folderOrPack);

        // Removes one place added by Mount. Does nothing if it was not mounted.
        static void Unmount(std::string_view folderOrPack);

        static void UnmountAll();

        // Reads a whole file.
        static Result<std::vector<std::uint8_t>> Read(std::string_view path);

        // Opens a file for reading parts of it.
        static FileReader Open(std::string_view path);

        static bool Exists(std::string_view path);

        // The folder the running program is in, with a trailing slash.
        static std::string ProgramFolder();

        // Writes every file under `folder` into one pack file, keeping their paths
        // relative to `folder`. Packs are not compressed.
        static Result<> CreatePack(std::string_view folder, std::string_view packPath);

        // The folder part of a path, with its trailing slash: "models/ship.obj"
        // gives "models/". Empty when the path has no folder.
        static std::string FolderOf(std::string_view path);

        // The extension in lowercase, with its dot: "Ship.OBJ" gives ".obj".
        static std::string ExtensionOf(std::string_view path);
    };
}
