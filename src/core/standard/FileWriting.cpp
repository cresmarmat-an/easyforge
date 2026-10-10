#include <easyforge/core/Paths.h>

#include <format>
#include <system_error>

namespace easyforge::internal
{
    // The standard library cannot flush a file to the disk, so here the new
    // file only takes the old one's place in one step.
    Result<> MoveIntoPlace(const std::filesystem::path& written, const std::filesystem::path& target)
    {
        std::error_code error;
        std::filesystem::rename(written, target, error);
        if (error)
        {
            std::error_code ignored;
            std::filesystem::remove(written, ignored);
            return Failure(std::format("{}: the file could not be replaced: {}", TextFromPath(target), error.message()));
        }
        return {};
    }
}
