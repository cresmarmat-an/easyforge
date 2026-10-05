#pragma once

#include <filesystem>
#include <string>
#include <string_view>

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
}
