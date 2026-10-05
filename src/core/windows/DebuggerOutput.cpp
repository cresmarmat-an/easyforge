#include "../DebuggerOutput.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdlib>
#include <string>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

namespace easyforge::internal
{
    void WriteToDebugger(std::string_view text)
    {
        if (!IsDebuggerPresent() || text.empty())
        {
            return;
        }

        int byteCount = static_cast<int>(text.size());
        int wideCount = MultiByteToWideChar(CP_UTF8, 0, text.data(), byteCount, nullptr, 0);
        if (wideCount <= 0)
        {
            return;
        }

        std::wstring wide(static_cast<std::size_t>(wideCount), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), byteCount, wide.data(), wideCount);
        OutputDebugStringW(wide.c_str());
    }

    void StopErrorDialogs()
    {
        // Inherited by programs this one starts.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    }
}
