if(NOT WIN32)
    # Linux, macOS, and the other platforms arrive in later stages.
    message(STATUS "easyforge window does not support ${CMAKE_SYSTEM_NAME} yet, so it is not built")
    return()
endif()

easyforge_add_library(window
    SOURCES
        Window.cpp
        WindowState.cpp)

target_sources(easyforge_window PRIVATE
    windows/Icons.cpp
    windows/Keyboard.cpp
    windows/Text.cpp
    windows/Win32Platform.cpp
    windows/Win32Window.cpp)

target_link_libraries(easyforge_window PRIVATE user32 gdi32 shell32 imm32 dwmapi shcore advapi32)
