# Fails when a public header uses a name that windows.h defines as a macro.
#
# windows.h turns names such as DrawText into DrawTextW. A header that declares
# DrawText still compiles, but programs that include windows.h first would call
# a function that does not exist. Comments and string literals are ignored.
#
#     cmake -D HEADER_DIRECTORY=<repository>/include -P CheckHeaderNames.cmake

set(forbidden
    # Lowercase macros from windows.h, windef.h, and rpcndr.h.
    interface near far min max small hyper
    # Functions that become their A or W versions.
    CallWindowProc ChangeDisplaySettings CopyFile CreateDialog CreateDirectory CreateEvent CreateFile
    CreateFont CreateMutex CreateProcess CreateSemaphore CreateWindow DefWindowProc DeleteFile DialogBox
    DispatchMessage DrawState DrawText EnumDisplayDevices EnumDisplaySettings EnumFonts ExpandEnvironmentStrings
    FindFirstFile FindNextFile FindWindow FormatMessage GetClassName GetCommandLine GetCurrentDirectory
    GetCurrentTime GetEnvironmentVariable GetFileAttributes GetFullPathName GetMessage GetModuleFileName
    GetModuleHandle GetMonitorInfo GetObject GetProp GetTempPath GetTextMetrics GetUserName GetWindowText
    LoadBitmap LoadCursor LoadIcon LoadImage LoadLibrary LoadMenu LoadString MessageBox MoveFile OpenEvent
    OutputDebugString PeekMessage PlaySound PostMessage RegisterClass RemoveDirectory RemoveProp ReportEvent
    SendMessage SetCurrentDirectory SetEnvironmentVariable SetFileAttributes SetProp SetWindowText StartDoc
    TextOut UnregisterClass Yield
    # Function-like macros from windowsx.h.
    IsMinimized IsMaximized IsRestored GetWindowStyle GetWindowID
    # Uppercase macros that are easy to reach for as enum values.
    ABSOLUTE CALLBACK CONST DELETE ERROR IGNORE IN INFINITE OPAQUE OPTIONAL OUT RELATIVE TRANSPARENT)

file(GLOB_RECURSE headers "${HEADER_DIRECTORY}/*.h")

list(JOIN forbidden "|" alternatives)
set(pattern "(^|[^A-Za-z0-9_])(${alternatives})([^A-Za-z0-9_]|$)")

set(problems "")
foreach(header IN LISTS headers)
    file(READ "${header}" text)
    string(REGEX REPLACE "//[^\n]*" "" text "${text}")
    string(REGEX REPLACE "\"[^\"\n]*\"" "\"\"" text "${text}")

    string(REGEX MATCHALL "${pattern}" matches "${text}")
    foreach(match IN LISTS matches)
        string(REGEX REPLACE "${pattern}" "\\2" name "${match}")
        file(RELATIVE_PATH shown "${HEADER_DIRECTORY}" "${header}")
        list(APPEND problems "${shown} uses '${name}'")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES problems)

if(problems)
    list(JOIN problems "\n  " listed)
    message(FATAL_ERROR "Public headers use names that windows.h defines as macros:\n  ${listed}")
endif()

list(LENGTH headers count)
message(STATUS "Checked ${count} public headers")
