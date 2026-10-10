#include <easyforge/core/Paths.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <format>
#include <system_error>

namespace easyforge::internal
{
    Result<> MoveIntoPlace(const std::filesystem::path& written, const std::filesystem::path& target)
    {
        auto giveUp = [&](std::string_view what) -> Result<> {
            std::error_code ignored;
            std::filesystem::remove(written, ignored);
            return Failure(std::format("{}: {}", TextFromPath(target), what));
        };

        // Flushed first, so the new file never takes the old one's place with
        // its bytes still only in memory.
        HANDLE file = CreateFileW(written.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return giveUp("the file could not be written");
        }
        bool flushed = FlushFileBuffers(file) != 0;
        CloseHandle(file);
        if (!flushed)
        {
            return giveUp("the file could not be written; the disk may be full");
        }
        if (!MoveFileExW(written.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            return giveUp("the file could not be replaced; it may be read-only or open in another program");
        }
        return {};
    }
}
