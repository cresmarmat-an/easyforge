#pragma once

#include <easyforge/core/Keys.h>
#include <easyforge/window/Export.h>

// The library's own tests use what is marked for export here, so it stays
// reachable when the library is a DLL.
namespace easyforge::internal
{
    // The key at a position on the keyboard, from the scan code Windows reports
    // with each key message, whether the code had the extended prefix, and the
    // virtual key, which decides when the scan code is missing or ambiguous.
    EASYFORGE_WINDOW_EXPORT Key KeyFromScanCode(unsigned scanCode, bool extended, unsigned virtualKey);
}
