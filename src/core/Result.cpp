#include <easyforge/core/Result.h>

#include <cstdlib>

#include <easyforge/core/Log.h>

namespace easyforge::internal
{
    void StopOnFailedResult(const std::string& error)
    {
        Log(LogLevel::Error, "The value of a failed Result was read. The error was: {}", error);
        std::abort();
    }
}
