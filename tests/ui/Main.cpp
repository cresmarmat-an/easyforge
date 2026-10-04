#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <easyforge/core/Log.h>
#include <easyforge/core/Testing.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

int main(int argumentCount, char** arguments)
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // A failed check in the standard library prints instead of opening a
    // dialog, which would stop the run until someone closes it.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

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
