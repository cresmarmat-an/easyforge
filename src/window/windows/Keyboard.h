#pragma once

#include <easyforge/core/Keys.h>

namespace easyforge::internal
{
    // The key at a position on the keyboard, from the scan code Windows reports
    // with each key message, whether the code had the extended prefix, and the
    // virtual key, which decides when the scan code is missing or ambiguous.
    Key KeyFromScanCode(unsigned scanCode, bool extended, unsigned virtualKey);
}
