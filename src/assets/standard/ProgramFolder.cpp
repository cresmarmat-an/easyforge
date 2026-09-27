#include "../ProgramFolder.h"

#include <system_error>

namespace easyforge::internal
{
    std::filesystem::path ExecutableFolder()
    {
        // Linux and Android describe the running program in /proc. Platforms
        // without it fall back to the working directory's search.
        std::error_code error;
        std::filesystem::path program = std::filesystem::read_symlink("/proc/self/exe", error);
        if (error)
        {
            return {};
        }
        return program.parent_path();
    }
}
