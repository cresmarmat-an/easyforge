#pragma once

#include <string_view>

namespace easyforge::internal
{
    // Shows text in an attached debugger's output window. Does nothing when no
    // debugger is attached, or on platforms without one.
    void WriteToDebugger(std::string_view text);
}
