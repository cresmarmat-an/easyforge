#include "../DebuggerOutput.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>

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
}
