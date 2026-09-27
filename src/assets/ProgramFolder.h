#pragma once

#include <filesystem>

namespace easyforge::internal
{
    // The folder of the running program's executable, or an empty path when the
    // platform cannot tell.
    std::filesystem::path ExecutableFolder();
}
