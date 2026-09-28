#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <easyforge/core/Log.h>
#include <easyforge/core/Testing.h>

int main(int argumentCount, char** arguments)
{
    // Direct3D's validation layer checks every call the renderer makes, when the
    // computer has it installed. Anything it reports fails the run.
    _putenv_s("EASYFORGE_GRAPHICS_VALIDATION", "1");
    std::vector<std::string> problems;
    easyforge::SetLogHandler([&problems](easyforge::LogLevel level, std::string_view message) {
        std::fprintf(stderr, "%.*s\n", static_cast<int>(message.size()), message.data());
        if (level == easyforge::LogLevel::Error)
        {
            problems.emplace_back(message);
        }
    });

    int result = easyforge::RunTests(argumentCount, arguments);
    if (!problems.empty())
    {
        std::fprintf(stderr, "%zu errors were logged while the tests ran\n", problems.size());
        return 1;
    }
    return result;
}
