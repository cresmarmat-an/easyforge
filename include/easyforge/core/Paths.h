#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <easyforge/core/Result.h>

namespace easyforge
{
    // easyforge takes and gives file paths as UTF-8 text on every platform.
    // These turn such text into a std::filesystem::path and back, so file and
    // folder names in any language work. On Windows, a path made straight from
    // a std::string is read in the system's code page instead, which breaks
    // names outside it, and one made from text that is not valid UTF-8 throws.
    //
    //     std::ifstream file(PathFromText("saves/Café.tree"), std::ios::binary);
    //
    // Neither throws: bytes that are not valid UTF-8, or on Windows parts of a
    // name that are not valid UTF-16, become the replacement character U+FFFD.
    std::filesystem::path PathFromText(std::string_view text);

    // UTF-8 text with forward slashes between the parts, on every platform.
    std::string TextFromPath(const std::filesystem::path& path);

    // Writes `bytes` to the file at `path`, whole or not at all. They go to a
    // new file beside it, which is flushed to the disk and then takes the old
    // file's place in one step, so a crash or power cut leaves the old file or
    // the new one, never part of either. The folder must exist already.
    //
    //     Result<> saved = WriteWholeFile("saves/slot 1.txt", text);
    Result<> WriteWholeFile(std::string_view path, std::string_view bytes);

    namespace internal
    {
        // The file beside `target` that WriteWholeFile writes first.
        std::filesystem::path WritingPath(const std::filesystem::path& target);

        // WriteWholeFile's last step, for files written in pieces at
        // WritingPath(target): flushes `written` to the disk and moves it into
        // `target`'s place. On failure `written` is removed.
        Result<> MoveIntoPlace(const std::filesystem::path& written, const std::filesystem::path& target);
    }
}
