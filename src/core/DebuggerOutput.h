#pragma once

#include <string_view>

namespace easyforge::internal
{
    // Shows text in an attached debugger's output window. Does nothing when no
    // debugger is attached, or on platforms without one.
    void WriteToDebugger(std::string_view text);

    // Failures print instead of opening a dialog: a crash, a program that
    // cannot start a part it needs, and a failed check in the C++ library's
    // debug build. A test run then never waits for someone to close a window.
    void StopErrorDialogs();
}
