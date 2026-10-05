#pragma once

// windows.h for easyforge's own Windows code: wide-character functions, and no
// min and max macros.

#ifndef NOMINMAX
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

#include <string>
#include <string_view>

namespace easyforge::internal
{
    // A Windows struct whose first member holds its own size, such as
    // MONITORINFO: zeroed, with that size set.
    template <typename Type>
    Type SizedStruct()
    {
        Type value {};
        *reinterpret_cast<UINT*>(&value) = static_cast<UINT>(sizeof(Type));
        return value;
    }

    std::wstring ToWide(std::string_view text);
    std::string FromWide(std::wstring_view text);
}
