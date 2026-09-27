#pragma once

// windows.h for easyforge's own Windows code: wide-character functions, and no
// min and max macros.

#ifndef NOMINMAX
#define NOMINMAX
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
    std::wstring ToWide(std::string_view text);
    std::string FromWide(std::wstring_view text);
}
