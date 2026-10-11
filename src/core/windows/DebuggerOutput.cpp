#include "../DebuggerOutput.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
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
        // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): byteCount is its length
        int wideCount = MultiByteToWideChar(CP_UTF8, 0, text.data(), byteCount, nullptr, 0);
        if (wideCount <= 0)
        {
            return;
        }

        std::wstring wide(static_cast<std::size_t>(wideCount), L'\0');
        // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): byteCount is its length
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

#if defined(_DEBUG) && defined(_MSC_VER)
    namespace
    {
        _CrtMemState LeakCheckStart {};

        // Runs from exit(), after the static objects made since the check
        // started are destroyed, since exit() runs those in reverse order.
        void ReportLeaks()
        {
            _CrtMemState end {};
            _CrtMemCheckpoint(&end);
            _CrtMemState difference {};
            if (!_CrtMemDifference(&difference, &LeakCheckStart, &end))
            {
                return;
            }
            // Differences, which are negative when more was freed than allocated.
            auto counted = [](auto value) { return static_cast<long long>(value); };
            long long blocks = counted(difference.lCounts[_NORMAL_BLOCK]) + counted(difference.lCounts[_CLIENT_BLOCK]);
            long long bytes = counted(difference.lSizes[_NORMAL_BLOCK]) + counted(difference.lSizes[_CLIENT_BLOCK]);
            if (blocks <= 0)
            {
                return;
            }
            std::fprintf(stderr, "\n%lld blocks (%lld bytes) allocated while the tests ran were never freed:\n",
                blocks, bytes);
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
            _CrtMemDumpAllObjectsSince(&LeakCheckStart);
            std::fflush(stderr);
            std::_Exit(3);
        }
    }

    void CheckForLeaksAtExit()
    {
        _CrtMemCheckpoint(&LeakCheckStart);
        std::atexit(ReportLeaks);
    }
#else
    void CheckForLeaksAtExit()
    {
    }
#endif
}
