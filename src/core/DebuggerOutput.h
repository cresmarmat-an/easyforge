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

    // Notes what memory is allocated now. When the program ends, after its
    // static objects are gone, anything allocated since and still allocated
    // is printed and the program exits with code 3. Only debug builds whose C
    // library keeps count of its allocations check; elsewhere it does nothing.
    void CheckForLeaksAtExit();
}
